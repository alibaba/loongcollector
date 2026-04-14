# 数据模型（C++）

LoongCollector **C++ Core** 在 **collection pipeline**（采集配置）执行路径中使用以 **`PipelineEventGroup`** 为容器、**`PipelineEvent`** 为单条数据抽象的可观测事件模型，涵盖日志、指标、链路 Span 与原始字节等类型。概念上与 [数据模型（Golang）](data-model-golang.md) 中的 Pipeline Event 体系一致；本节侧重 **C++ 类型、内存与元数据约定**。

实现代码位于 `core/models/`，命名空间仍为 **`logtail`**（与仓库历史一致）。

## PipelineEventGroup（事件组）

`PipelineEventGroup` 表示**一组**待在同一上下文中处理或刷写的 `PipelineEvent`，是 Native 输入 / 处理器 / Flusher 之间传递的主要单元。

- **事件列表**：`MutableEvents()` / `GetEvents()`，元素类型为 `PipelineEventPtr`（见下文）。
- **SourceBuffer**：每个组持有主 `SourceBuffer`（`GetSourceBuffer()`），用于在**连续内存块**中分配字符串，多数 `StringView` 指向该缓冲，避免频繁堆分配。
- **组级 Tags**：`SetTag` / `GetTag` 等，一般对应需要**随数据输出**的标签（如 `__tag__:__path__` 一类语义，具体键名由插件与协议约定）。
- **组级 Metadata**：`SetMetadata(EventGroupMetaKey, ...)`，键为枚举 **`EventGroupMetaKey`**（如日志路径、K8s/容器信息、Prometheus 抓取状态等）。注释中说明：用于生成 tag/日志等逻辑，**本身不直接作为输出字段**；与组级 Tags 分工不同。
- **ExactlyOnce**：可挂载 `RangeCheckpoint`（`SetExactlyOnceCheckpoint`），用于Exactly-Once 等场景。
- **构造事件**：`AddLogEvent` / `AddMetricEvent` / `AddSpanEvent` / `AddRawEvent`（或对应的 `Create*` 返回 `unique_ptr`）。

组对象**仅可移动、不可拷贝**（拷贝需使用 `Copy()` 显式深拷贝）。

## PipelineEvent（基类）

`PipelineEvent` 为抽象基类，子类对应不同 **`PipelineEvent::Type`**：

| 枚举值 | 含义 | C++ 类型 |
|--------|------|----------|
| `LOG` | 日志 | `LogEvent` |
| `METRIC` | 指标 | `MetricEvent` |
| `SPAN` | 链路 Span | `SpanEvent` |
| `RAW` | 原始内容（如未解析字节） | `RawEvent` |

公共能力包括：

- **时间戳**：`GetTimestamp` / `SetTimestamp`（`time_t`），以及可选的**纳秒部分** `GetTimestampNanosecond` / `SetTimestamp(..., uint32_t ns)`。
- **所属组**：内部持有指向 `PipelineEventGroup` 的指针，用于回查 `SourceBuffer` 等（`GetSourceBuffer()` 经由此关联）。
- **深拷贝**：`Copy()` 纯虚，由子类实现。

与 Golang 文档中的 `PipelineEvent` 接口相比，C++ 基类**未统一暴露 Name / Tags**；名称与标签分布在各子类中（例如指标名在 `MetricEvent`，Span 属性在 `SpanEvent`）。

## PipelineEventPtr 与对象池

`PipelineEventPtr` 是对 `std::unique_ptr<PipelineEvent>` 的包装，**禁止拷贝、可移动**，析构时可根据标记将事件归还 **`EventPool`**（见 `PipelineEventPtr` 构造参数 `fromPool` / `pool`）。

插件侧常用 `Is<T>()`、`Cast<T>()`、`Get<T>()` 做类型判断与向下转型。

## LogEvent

表示一条**结构化日志**（键值对集合 + 可选级别与文件位置）：

- **Contents**：`SetContent` / `GetContent` / `DelContent` 及迭代器；键值类型为 `StringView` 对。同一 key 的重复与顺序行为与部分历史解析插件兼容有关（见头文件注释）。
- **级别**：`SetLevel` / `GetLevel`（`StringView`），语义可与 OpenTelemetry Logs 等等级对齐。
- **文件位置**：`SetPosition(offset, size)` / `GetPosition()`，用于文件采集场景下的偏移与原始长度。

C++ 侧**没有**单独的 TraceID/SpanID 成员；若需要链路关联，通常放在 **Contents** 的字段中，由解析或处理插件写入。

## MetricEvent

表示一条**指标事件**：

- **名称**：`GetName` / `SetName` / `SetNameNoCopy`。
- **取值 `MetricValue`**：`std::variant` 包装，当前可为：
  - **`UntypedSingleValue`**：单 `double`，用于常见的 counter/gauge 单值。
  - **`UntypedMultiDoubleValues`**：**多字段浮点值**（`StringView` → `UntypedMultiDoubleValue`，内含 `MetricTypeCounter` / `MetricTypeGauge` 与 `double Value`），对应文档与插件中所说的「多值」指标（例如 [input_internal_metrics](../plugins/input/native/input-internal-metrics.md)）。
  - 空状态：`std::monostate`。
- **Labels 式 Tags**：`SetTag` / `GetTag` 等，序列化时作为指标标签的一部分。
- **Metadata**：`SetMetadata` / `GetMetadata` 等，注释中说明会在序列化时与 name、value、labels 等**并列展开**（具体由 Flusher/协议适配层消费）。

通过 `Is<T>()` / `GetValue<T>()` / `MutableValue<T>()` / `SetValue<T>(...)` 访问具体取值类型。

### 多值 MetricEvent（UntypedMultiDoubleValues）

`UntypedMultiDoubleValues` 内部为 `std::map<StringView, UntypedMultiDoubleValue>`，并提供 `GetValue` / `SetValue` / 迭代接口。适合 Histogram/Summary 等在单条事件中携带**多个桶或统计字段**的场景。

## SpanEvent

对齐 **OTLP Span** 的简化实现（头文件注释中的差异包括：忽略部分 OTLP 字段、**Attribute 值仅支持字符串**、`InstrumentedScope` 信息落在 **`mScopeTags`** 等）。

要点：

- 标识与时间：`TraceId`、`SpanId`、`ParentSpanId`、`TraceState`、`Name`、`Kind`、`StatusCode`，以及 **`StartTimeNs` / `EndTimeNs`（纳秒）**。
- **Span 级 Tags**：字符串 KV，对应 OTLP 属性语义。
- **InnerEvent**：Span 内嵌事件（时间戳纳秒 + Name + Tags）。
- **SpanLink**：链接其他 Span，内含 TraceId/SpanId/TraceState 与 Tags。
- **`PipelineEvent` 基类时间戳**与 Span 的纳秒起止时间并存；具体以何者为准取决于序列化与插件逻辑。

`PipelineEventGroup` 在注释中与 OTLP 的 **ResourceSpan** 类比：资源属性可落在**组级 Tags**。

## RawEvent

承载**一块原始内容**（`GetContent` / `SetContent` / `SetContentNoCopy`），类型为 `StringView`，适用于尚未解析为 Log/Metric/Span 的字节流。

## StringView、SourceBuffer 与 NoCopy API

- 大量字段使用 **`StringView`**（`common/StringView.h`），指向 `SourceBuffer` 或其它生命周期受控的内存。
- 名称以 **`NoCopy`** 结尾的接口（如 `SetContentNoCopy`、`SetTagNoCopy`）表示调用方保证传入缓冲的生命周期与组内 `SourceBuffer` 一致，**避免额外拷贝**，在热路径上常用。

新增字符串时通常通过组的 `SourceBuffer` 分配，再 `SetXxxNoCopy`。

## 与 Protobuf / 跨语言

`core/protobuf/models/` 下存在 **`pipeline_event_group.proto`** 等定义及生成代码，用于与 **Go 侧插件 / collection pipeline** 或其它组件的序列化交换；运行时内存中的权威表示仍是上述 C++ 模型，转换在适配层完成。

## 延伸阅读

- [数据模型（Golang）](data-model-golang.md)：接口级定义与 Metric/Trace/Log 字段语义对照。
- 源码入口：`core/models/PipelineEventGroup.h`、`PipelineEvent.h`、`LogEvent.h`、`MetricEvent.h`、`MetricValue.h`、`SpanEvent.h`、`RawEvent.h`、`PipelineEventPtr.h`。
