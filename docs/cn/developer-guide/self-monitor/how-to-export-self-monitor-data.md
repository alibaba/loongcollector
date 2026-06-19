# 如何将自监控数据发送到Go Pipeline

LoongCollector 从 v3.1.0 开始支持将自监控数据（指标、告警）直接发送到 Go Pipeline，从而可以与 Go 生态的输出插件（如 `flusher_prometheus`）配合使用。

## 背景

`input_internal_metrics` 和 `input_internal_alarms` 是两个原生（C++）输入插件。在此特性引入之前，它们只能与原生输出插件（如 `flusher_file`、`flusher_sls`）组合使用，无法直接对接 Go Pipeline 的输出插件（如 `flusher_prometheus`）。

本特性通过在 C++ 与 Go Pipeline 之间新增 `ProcessPipelineEventGroup` 数据通道，使得自监控数据（以 `PipelineEventGroup` 格式）可以序列化后传入 Go 侧，并由 Go 侧的 Flusher 插件处理和输出。

## 适用场景

- 将自监控指标暴露为 Prometheus Remote Write 端点，由 Prometheus Server 拉取。
- 将自监控告警通过 Go 侧 Flusher 转发到自定义后端。

## 使用方法

### 自监控指标 + Prometheus 输出

以下示例配置将 LoongCollector 自监控指标通过 `flusher_prometheus` 暴露为 Prometheus Remote Write 接口：

```yaml
enable: true
inputs:
  - Type: input_internal_metrics
    Agent:
      Enable: true
      IntervalSeconds: 15
    Runner:
      Enable: true
      IntervalSeconds: 15
    Pipeline:
      Enable: true
      IntervalSeconds: 15
    Plugin:
      Enable: true
      IntervalSeconds: 60
    Component:
      Enable: true
      IntervalSeconds: 60
    PluginSource:
      Enable: true
      IntervalSeconds: 60
flushers:
  - Type: flusher_prometheus
    Labels:
      job: loongcollector
```

将以上配置保存到运行目录下的 `conf/continuous_pipeline_config/local/self_monitor_prometheus.yaml`，LoongCollector 会自动加载该采集配置。

之后可以在 Prometheus 中通过以下方式验证指标是否正常上报：

```promql
{job="loongcollector"}
```

### 自监控告警 + Go Flusher 输出

以下示例配置将 LoongCollector 自监控告警通过 `flusher_prometheus` 输出：

```yaml
enable: true
inputs:
  - Type: input_internal_alarms
flushers:
  - Type: flusher_prometheus
    Labels:
      job: loongcollector_alarms
```

## 技术实现

### 数据流

```text
input_internal_metrics/input_internal_alarms
  ↓  (C++ PipelineEventGroup)
ProcessorRunner::FlushBatch
  ↓  TransferPipelineEventGroupToPB + SerializeToString
LogtailPlugin::ProcessPipelineEventGroup  (C++ → Go CGo 边界)
  ↓  plugin_export.go: ProcessPipelineEventGroup
pluginv2Runner::ReceivePipelineEventGroup
  ↓  helper.TransferPBToPipelineGroupEvents
InputPipeContext.Collector().Collect
  ↓
Flusher (e.g. flusher_prometheus)
```

### 配置验证放宽

在此特性引入前，`CollectionConfig::Parse()` 会拒绝"原生 Input + Go Flusher"的组合（因为原生 Input 的数据格式不经过 Go Pipeline）。

本特性为 `input_internal_metrics` 和 `input_internal_alarms` 引入了专门的 `hasSelfMonitorInput` 标志，放宽了上述限制，使自监控 Input 可以与任意 Go Flusher 配合使用。

### Protobuf 数据格式

自监控数据在 C++ 与 Go 之间使用 `models.PipelineEventGroup` Protobuf 消息序列化传输，支持三种事件类型：

- **MetricEvents**：用于 `input_internal_metrics` 的多值指标数据（`UntypedMultiDoubleValues`）
- **LogEvents**：用于 `input_internal_alarms` 的告警日志数据
- **SpanEvents**：预留，用于后续 Trace 数据支持

## 注意事项

1. `input_internal_metrics` 输出的是多值 MetricEvent（`UntypedMultiDoubleValues`），部分 Go Flusher 可能需要确认是否支持此格式。`flusher_prometheus` 已支持。
2. 该特性要求 LoongCollector v3.1.0 及以上版本。
3. 与原生 Flusher 的组合（如 `flusher_file`）仍然支持，本特性为新增能力，不影响原有用法。
