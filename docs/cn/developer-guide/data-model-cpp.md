# 数据模型（C++）

LoongCollector C++ 部分全部使用 `Pipeline Event` 数据模型。

## Pipeline Event模型

`Pipeline Event` 数据模型中，在插件中流转的基础数据结构是 `PipelineEventGroup`，一个 `PipelineEventGroup` 是对多个 `PipelineEvent` 的集合， `PipelineEvent` 有多种类型。下面，依次介绍各个数据结构。

### PipelineEventGroup

`PipelineEventGroup` 是由Input产生的，例如一次文件读取就会生成一个EventGroup，一次读取可能生成多个event，他们的元信息被记录在metadata中，例如hostname，ip，pod_name，container_name，filename，inode，topic等。

需要注意的是内存的分配。`PipelineEventGroup` 内部虽然有多个 `PipelineEvent`，但是 `PipelineEvent` 中的数据并不自己持有内存，而是使用 `PipelineEventGroup` 的 `mSourceBuffer` 分配内存。也正因此，`PipelineEvent` 必须由 `PipelineEventGroup` 创建。

```cpp
class PipelineEventGroup {
    GroupMetadata mMetadata; // Used to generate tag/log. Will not output.
    SizedMap mTags; // custom tags to output
    EventsContainer mEvents; // PipelineEvents
    std::shared_ptr<SourceBuffer> mSourceBuffer;
};
```

下面介绍 `PipelineEventGroup` 中各个字段。

#### Metadata

metadata 的定位是存储 events 的元信息，一般由 input 生成，不会进行修改，但可能增加。

输入包括：

* 都携带的 ip 、 hostname 等。
* 特定 input 插件生成的如文件名、 offset 等。

用途包括：

* 在程序打印日志中使用。如打印日志被丢弃时，打印文件名、 inode 。
* 生成 Tag 。例如使用 `host.ip` 信息生成 `__source__` tag 。 SPL 同样可以使用 metadata 来生成 tag 。
* 处理日志时生成字段。例如切分日志时从 `file.offset` 结合切分偏移生成单条日志的 `__offset__`。

#### Tags

tags 的定位是用于输出的 events 元信息，可以进行修改，需要 processor 生成。

输入包括：

* 在 processor 中生成（包括 SPL ）。例如使用 metadata 中的 `host.ip` 信息生成 `__source__` tag 。
  * SPL 根据数据生成 Tag 。例如为 log 打上 `log_level` tag 。此操作可能迫使 `PipelineEventGroup` 分裂。
* processor 插件生成的 cloud host meta 等。

用途包括：

* processor 中处理 tag （包括 SPL ）。
* 序列化输出。 sls 直接对应 pb 结构中的 tag ，其他 flusher 根据 converter 转换。

### PipelineEvent

`PipelineEvent` 是承载各种Event的公共部分，当前为基类，作为公共部分组合到各种Event中。目前已经支持的有`LogEvent`，`MetricEvent`，`SpanEvent`和`RawEvent`。

```cpp
class PipelineEvent {
    time_t timestamp = 0;
    std::optional<uint32_t> mTimestampNanosecond;
    PipelineEventType mType = VOID_EVENT_TYPE;
    std::shared_ptr<SourceBuffer> mSourceBuffer;
    PipelineEventGroup* mPipelineEventGroupPtr = nullptr;
};
```

#### LogEvent

`LogEvent` 额外多了 contents 字段，来存储日志类型数据的不同字段。还有 level 字段，用来存储日志级别。

```cpp
class LogEvent : public PipelineEvent {
    ContentsContainer mContents; // Log Contents
    StringView mLevel;  // Log Level
}
```

#### MetricEvent

`MetricEvent` 的结构包含指标名、指标值和标签。主流的 metrics 数据有单值(eg. Prometheus)和多值(eg. influxdb)两种设计，LoongCollector 中也需要支持两种不同的设计，基于此设计了 MetricValue 包含了 UntypedSingleValue（单值） 和 UntypedMultiDoubleValues（多值） 两个不同的类型。

```cpp
class UntypedSingleValue;
class UntypedMultiDoubleValues;
using MetricValue = std::variant<std::monostate, UntypedSingleValue, UntypedMultiDoubleValues>;

class MetricEvent : public PipelineEvent {
    StringView mName; // Metric Name
    MetricValue mValue; // Metric Value
    SizedMap mTags; // Metric Tags
}

struct UntypedMultiDoubleValue {
    UntypedValueMetricType MetricType;
    double Value;
};

struct UntypedMultiDoubleValues {
    std::map<StringView, UntypedMultiDoubleValue> mValues;
    PipelineEvent* mMetricEventPtr;
}
```

#### SpanEvent

目前主流的开源 tracing 模型有opentracing opentelemetry skywalking 等几种不同的规范。其中zipkin jaeger 都是 opentracing 的一种实现，而opentelemetry 可以认为是在opentracing的基础上扩展的新规范和超集（兼容 w3c 和定义了明确的 OTLP 协议）。

不管在哪种 trace 模型中，trace 都是由多个 Span 组成的，不同的是在 skywalking 和许多私有协议中，trace 和 span 中间还有一个 segment 或者 transaction 概念。  

在 Span 中，一般还会定义携带数据语义的 Tags ，和记录一些关键事件发生的 logs 或 events 。  

所以我们定义的 `SpanEvent` 结构要兼容上面提到的几种开源协议，具体的结构如下

```cpp
class SpanEvent : public PipelineEvent {
    StringView mTraceId; // required
    StringView mSpanId; // required
    StringView mTraceState;
    StringView mParentSpanId;
    StringView mName; // required
    Kind mKind = Kind::Unspecified;
    uint64_t mStartTimeNs = 0; // required
    uint64_t mEndTimeNs = 0; // required
    SizedMap mTags;
    std::vector<InnerEvent> mEvents;
    std::vector<SpanLink> mLinks;
    StatusCode mStatus = StatusCode::Unset;
    SizedMap mScopeTags; // store InstrumentedScope info in otlp
}
```

#### RawEvent

`RawEvent` 是一个特殊的中间态结构，它用于LoongCollector内部一些模块的数据流传、处理（例如Batcher、Serializer中）。它的定义很简单。

```cpp
class RawEvent : public PipelineEvent {
    StringView mContent;
}
```
