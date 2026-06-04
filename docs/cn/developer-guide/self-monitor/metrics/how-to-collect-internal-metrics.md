# 如何收集自监控指标

本文说明如何使用 `input_internal_metrics` 采集 LoongCollector 自监控指标，并导出到 Prometheus 等外部系统。插件在**采集配置（collection pipeline）**中输出 C++ **多值 MetricEvent**（`UntypedMultiDoubleValues`），配置保存于 `conf/continuous_pipeline_config/local` 后由 LoongCollector 自动加载。

相关文档：

- [自监控指标说明](internal-metrics-description.md)
- [input_internal_metrics 插件](../../../plugins/input/native/input-internal-metrics.md)
- [flusher_prometheus 插件](../../../plugins/flusher/extended/flusher-prometheus.md)

---

## 导出到外部系统

| 方式 | 适用版本 | 典型场景 |
|------|----------|----------|
| **指标直写** | 高于 v3.3.5（不含 v3.3.5） | 单条配置内 `input_internal_metrics` 直连 Flusher |
| **经文件中转** | 各版本通用 | 先落盘 JSON，再由 `input_file` 读取并转发 |

---

## 方式一：指标直写（单条采集配置）

### 版本要求

**LoongCollector 版本须高于 v3.3.5（不含 v3.3.5）**，才支持 Native `input_internal_metrics` 与扩展 Flusher 同条配置，并经 `ProcessPipelineEventGroup` 将 Metric 送入 Go Pipeline。

低于或等于 v3.3.5 时，请使用 [方式二：经文件中转](#方式二经文件中转两段采集配置)。

### 支持的 Flusher

#### 扩展 Flusher（须 `global.StructureType: v2`）

在 v2 Plugin Runner 下，能与 Native Input 桥接并消费 `PipelineGroupEvents` 的扩展 Flusher，须实现 `FlusherV2` 接口（`Export` 接收 Metric 等事件）。当前代码中**仅**下列插件满足：

| 插件 | 说明 |
|------|------|
| `flusher_prometheus` | Remote Write；多值 Metric 按 `{category}_{field}` 展开为多条 series（如 `agent_cpu`） |

配置须设置 `global.StructureType: v2`。可与可选 Go Processor（如 `processor_add_fields`）组合。

#### 原生 Flusher（C++，无需 Go 桥接）

与 `input_internal_metrics` 同处 **Native 处理链路**，序列化层已支持多值 MetricEvent：

| 插件 | 说明 |
|------|------|
| `flusher_file` | JSON 行写入本地文件（调试、落盘或作为方式二的 Pipeline 1 输出） |
| `flusher_sls` | 须将 `TelemetryType` 设为 `metrics_multivalue`（或文档所列多值指标类型） |
| `flusher_blackhole` | 丢弃事件，仅用于压测或占位，**不**构成实际导出 |

### 不支持的 Flusher（指标直写场景）

| 类型 | 插件示例 | 原因 |
|------|----------|------|
| 扩展 Flusher（v2 Runner） | `flusher_http`、`flusher_kafka`、`flusher_kafka_v2`、`flusher_loki`、`flusher_elasticsearch`、`flusher_clickhouse`、`flusher_doris`、`flusher_otlp`、`flusher_pulsar`、`flusher_stdout` 等 | 未实现 `FlusherV2`；v2 配置下无法与 Native 桥接 Metric 同配；v1 路径对桥接 Metric 会丢弃 |
| 原生 Flusher | `flusher_kafka` / `flusher_kafka_native` | C++ 实现中尚未支持 MetricEvent 写出（见 `FlusherKafka.cpp` 注释） |

> 若目标为 Prometheus 而版本 ≤ v3.3.5，或需使用上表「不支持」的扩展 Flusher，请改用 [方式二](#方式二经文件中转两段采集配置)。

### 示例：直连 Prometheus RemoteWrite

保存为 `conf/continuous_pipeline_config/local/self_metrics_prometheus.yaml`：

```yaml
enable: true
global:
  StructureType: v2
inputs:
  - Type: input_internal_metrics
    Agent:
      Enable: true
      Interval: 1
    Pipeline:
      Enable: true
      Interval: 1
    Plugin:
      Enable: true
      Interval: 1
    Component:
      Enable: true
      Interval: 1
processors:
  - Type: processor_add_fields
    Fields:
      env: production
      component: loongcollector
    IgnoreIfExist: false
flushers:
  - Type: flusher_prometheus
    Endpoint: "https://example.com/api/v1/write"
    SeriesLimit: 1000
    Compression: "snappy"
    Concurrency: 1
    QueueCapacity: 4096
    DropEventWhenQueueFull: false
    Timeout: "10s"
    Retry:
      Enable: true
      MaxRetryTimes: 3
      InitialDelay: "1s"
      MaxDelay: "30s"
```

### 指标命名约定（Prometheus）

自监控一条 JSON/Metric 记录包含多个字段（`__value__` map）。`flusher_prometheus` 编码时按字段展开：

| 来源字段 | Prometheus 映射 |
|----------|-----------------|
| 事件名 / `__name__` = `agent` | 指标名前缀 |
| `__value__.cpu` | 指标名 `agent_cpu` |
| Tags / `__labels__` | Prometheus labels |
| 时间戳 | sample timestamp（毫秒） |

---

## 方式二：经文件中转（两段采集配置）

适用于 **LoongCollector ≤ v3.3.5**，或需使用方式一「不支持」列表中的扩展 Flusher、或需在第二段做 JSON 解析与字段转换的场景。

```text
Pipeline 1（写文件）:  input_internal_metrics  →  flusher_file
Pipeline 2（读文件转发）:  input_file  →  processor_*  →  flusher_prometheus（或其它扩展 Flusher）
```

- Pipeline 1 仅使用 Native 插件，无「Native Input + 扩展 Flusher」组合限制。
- `input_file` 属于可与扩展 Flusher 组合的白名单 Input，Pipeline 2 可在较低版本上加载。
- 两段配置须**同时** `enable`；`FilePaths` 须为 Pipeline 1 输出文件的**绝对路径**。

### Pipeline 1：采集并写入本地文件

`conf/continuous_pipeline_config/local/self_metrics_file.yaml`：

```yaml
enable: true
inputs:
  - Type: input_internal_metrics
    Agent:
      Enable: true
      Interval: 1
    Pipeline:
      Enable: true
      Interval: 1
    Plugin:
      Enable: true
      Interval: 1
    Component:
      Enable: true
      Interval: 1
flushers:
  - Type: flusher_file
    FilePath: /path/to/loongcollector/self_monitor/self_metrics.log
```

配置生效后，约一个 `Interval` 周期（单位为**分钟**）即有输出。每行一条 JSON，`__name__` 为指标类别，`__labels__` 为标签，`__time__` 为时间戳，`__value__` 为指标值 map。示例：

```json
{
    "__labels__": {"hostname": "xxx", "instance_id": "xxx"},
    "__name__": "agent",
    "__time__": 1735194085,
    "__value__": {"cpu": 0.002, "memory_used_mb": 25.0, "pipeline_config_total": 1.0}
}
```

### Pipeline 2：读取指标文件并 Remote Write 到 Prometheus

`conf/continuous_pipeline_config/local/self_metrics_prometheus_forward.yaml`：

```yaml
enable: true
global:
  StructureType: v2
inputs:
  - Type: input_file
    FilePaths:
      - /path/to/loongcollector/self_monitor/self_metrics.log
    FileEncoding: utf8
processors:
  - Type: processor_json
    SourceKey: content
  - Type: processor_add_fields
    Fields:
      env: production
      component: loongcollector
    IgnoreIfExist: false
flushers:
  - Type: flusher_prometheus
    Endpoint: "https://example.com/api/v1/write"
    SeriesLimit: 1000
    Compression: "snappy"
    Concurrency: 1
    QueueCapacity: 4096
    DropEventWhenQueueFull: false
    Timeout: "10s"
    Retry:
      Enable: true
      MaxRetryTimes: 3
      InitialDelay: "1s"
      MaxDelay: "30s"
```

**说明：**

- `processor_json` 解析后，自监控字段（`__name__`、`__labels__`、`__value__` 等）出现在日志事件字段中；`flusher_prometheus` 期望 **Metric** 类型事件。若 RemoteWrite 无数据，需增加将 JSON 转为 Metric 的 Processor（可参考 [`processor_log_to_sls_metric`](../../../plugins/processor/extended/processor-log-to-sls-metric.md) 对 `__name__` / `__value__` 的用法，或按业务自定义）。
- 将 `FilePaths` 替换为 Pipeline 1 实际绝对路径。

---

## 故障排查

| 现象 | 可能原因 | 处理建议 |
|------|----------|----------|
| 配置加载失败，`extended flusher plugins coexist with native input` | 版本 ≤ v3.3.5 或未含直连特性 | 升级到 **高于 v3.3.5** 的版本，或改用两段 Pipeline |
| Pipeline 2 无数据写入 Prometheus | 路径错误或未产出 Metric 事件 | 检查绝对路径、`processor_json` 及 Metric 转换链路 |
| 指标文件无数据 | `input_internal_metrics` 未 Start 或 Interval 未到 | 检查 `enable` 与 Interval（单位为**分钟**） |
| RemoteWrite 4xx/5xx | Endpoint、鉴权、TLS | 检查 `Endpoint`、`Headers`、`Authenticator` |
| Prometheus 中 series 名称与 JSON 不一致 | 多值展开为 `{category}_{field}` | 按命名约定调整 Grafana / PromQL |

---

## 参考

- GitHub Issue：[Support exporting input_internal_metrics via flusher_prometheus #2568](https://github.com/alibaba/loongcollector/issues/2568)
