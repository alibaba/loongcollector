# 如何添加自监控指标

本文说明如何在 **C++ Core** 或 **Golang 插件**中新增自监控指标，以及如何与 `input_internal_metrics` 等采集路径衔接。

## C++ 添加自监控指标

### 整体机制

- **写入侧**：`WriteMetrics`（[`core/monitor/MetricManager.h`](https://github.com/alibaba/loongcollector/blob/main/core/monitor/MetricManager.h)）维护一条 `MetricsRecord` 链表；每条记录包含 **category**、**labels**、以及若干 **Counter / TimeCounter / IntGauge / DoubleGauge**。
- **读取侧**：`ReadMetrics` 定期做快照，并转换为 `SelfMonitorMetricEvent`，供 `input_internal_metrics` 等逻辑消费，最终以[多值 `MetricEvent`（`UntypedMultiDoubleValues`）](../../data-model-cpp.md)等形式输出；详见该文档 **MetricEvent** 小节。
- **典型流程**：`CreateMetricsRecordRef` → 在 `MetricsRecordRef` 上 `CreateCounter` / `CreateTimeCounter` / `CreateIntGauge` / `CreateDoubleGauge` → `CommitMetricsRecordRef`。注释见 `WriteMetrics::CreateMetricsRecordRef`。

指标类型与宏定义见 [`core/monitor/metric_models/MetricTypes.h`](https://github.com/alibaba/loongcollector/blob/main/core/monitor/metric_models/MetricTypes.h)（如 `ADD_COUNTER`、`SET_GAUGE` 等，使用前宜判空）。

### 原生插件（继承 `Plugin`）

[`Plugin`](https://github.com/alibaba/loongcollector/blob/main/core/collection_pipeline/plugin/interface/Plugin.h) 已封装 **插件级**记录的创建与提交：

- 在 **`InputInstance::Init` / `ProcessorInstance::Init` / `FlusherInstance::Init`** 中，框架会**先于** `Plugin::Init` 调用 `CreateMetricsRecordRef(插件名, PluginID)`，为当前插件绑定 `MetricCategory::METRIC_CATEGORY_PLUGIN`，并注入 `project`、`pipeline_name`、`logstore`、`plugin_type`、`plugin_id` 等标签。
- 插件在 **`Init` 内**通过 `GetMetricsRecordRef()` 创建具体指标；`Init` 返回前框架会调用 **`CommitMetricsRecordRef()`**。

参考实现：[`InputFile::Init`](https://github.com/alibaba/loongcollector/blob/main/core/plugin/input/InputFile.cpp) 中在 `GetMetricsRecordRef()` 上 `CreateIntGauge(METRIC_PLUGIN_MONITOR_FILE_TOTAL)`，并结合 `PluginMetricManager`（见下）。

**新增插件级指标名**时，通常需要：

1. 在 [`MetricConstants.h`](https://github.com/alibaba/loongcollector/blob/main/core/monitor/metric_constants/MetricConstants.h)（及对应 `.cpp`）中增加 **字符串常量**；
2. 在采集路径上 **`Add` / `Set`** 打点；
3. 在 [自监控指标说明](internal-metrics-description.md) 中补充文档，便于运维理解。

### 按「数据源」拆分的指标（`METRIC_CATEGORY_PLUGIN_SOURCE`）

文件类等输入往往对**每个文件（或类似维度）**需要独立 labels。此时可使用 **`PluginMetricManager`** + **`ReentrantMetricsRecord`**（[`ReentrantMetricsRecord.h`](https://github.com/alibaba/loongcollector/blob/main/core/monitor/metric_models/ReentrantMetricsRecord.h)）：

- 在插件 `Init` 中构造 `PluginMetricManager`，传入默认 labels（一般来自 `GetMetricsRecordRef()->GetLabels()`）、指标名到 `MetricType` 的映射，以及 category（常为 `METRIC_CATEGORY_PLUGIN_SOURCE`）。
- 运行期通过 `GetOrCreateReentrantMetricsRecordRef(额外 labels)` 获取可重入记录，再 `GetCounter` 等打点；不用时 `ReleaseReentrantMetricsRecordRef`。

`InputFile` / `InputStaticFile` 与 `FileServer::GetOrCreateReentrantMetricsRecordRef` 的配合即此类模式。

### Pipeline、组件、Runner、Agent 等非「单插件」场景

不继承 `Plugin` 的模块（如 [`CollectionPipeline`](https://github.com/alibaba/loongcollector/blob/main/core/collection_pipeline/CollectionPipeline.cpp)）可**直接使用**：

```cpp
WriteMetrics::GetInstance()->CreateMetricsRecordRef(
    mMetricsRecordRef,
    MetricCategory::METRIC_CATEGORY_PIPELINE,  // 或 COMPONENT / AGENT / RUNNER 等
    {{METRIC_LABEL_KEY_PROJECT, ...}, ...});
auto c = mMetricsRecordRef.CreateCounter(METRIC_PIPELINE_PROCESSORS_IN_EVENTS_TOTAL);
// ...
WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricsRecordRef);
```

`MetricCategory` 取值见 [`MetricRecord.h`](https://github.com/alibaba/loongcollector/blob/main/core/monitor/metric_models/MetricRecord.h) 中 `MetricCategory` 静态常量，需与 [自监控指标说明](internal-metrics-description.md) 中的 **Agent / Runner / Pipeline / Plugin / PluginSource / Component** 分级一致。

### 动态标签

`WriteMetrics::CreateMetricsRecordRef` 支持 **`DynamicMetricLabels`**（`std::function<std::string()>`），用于采样时刻才确定的标签值；适用于少量需要惰性求值的场景。

### 验证与采集

- 指标是否进入快照，可由现有 **`input_internal_metrics`** 采集配置验证，步骤见 [如何收集自监控指标](how-to-collect-internal-metrics.md)。
- **TimeCounter** 的 `Add` 接收 **`std::chrono::nanoseconds`**，对外聚合时按实现换算为毫秒等（见 `MetricTypes.h` 注释）。

---

## Golang 添加自监控指标

参见 [Golang 自监控指标接口](../../plugin-development/plugin-debug/plugin-self-monitor-guide.md)。
