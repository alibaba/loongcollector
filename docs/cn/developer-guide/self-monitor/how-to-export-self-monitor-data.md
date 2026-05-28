# 如何导出自监控数据

本文说明如何将 LoongCollector **自监控指标**（`input_internal_metrics`）和 **自监控告警**（`input_internal_alarms`）导出到外部系统。当前请使用下文 **过渡方案**；单条配置直连 Prometheus 的能力尚在建设中。

相关文档：

- [如何收集自监控指标](metrics/how-to-collect-internal-metrics.md)
- [自监控指标说明](metrics/internal-metrics-description.md)
- [自监控告警说明](alarms/internal-alarms-description.md)
- [input_internal_metrics 插件](../../plugins/input/native/input-internal-metrics.md)
- [input_internal_alarms 插件](../../plugins/input/native/input-internal-alarms.md)

---

## 当前能力与限制

| 能力 | 状态 | 说明 |
|------|------|------|
| `input_internal_metrics` → `flusher_file` | 已支持 | 官方推荐，输出 JSON 行到本地文件 |
| `input_internal_alarms` → `flusher_file` | 已支持 | 告警为 LogEvent，输出 JSON 行 |
| `input_internal_metrics` → `flusher_prometheus`（单条配置） | 建设中 | 见 [Issue #2568](https://github.com/alibaba/loongcollector/issues/2568)；当前配置校验会拒绝该组合 |
| `input_internal_alarms` → `flusher_prometheus` | 不适用 | 告警为日志形态，不应使用 Prometheus RemoteWrite |

若配置 `input_internal_metrics` 与 `flusher_prometheus` 时 pipeline 无法启动，报错类似：

```text
extended flusher plugins coexist with native input plugins other than input_file or input_container_stdio or input_*_security
```

这表示当前构建尚未放开 `input_internal_metrics` 与 `flusher_prometheus` 的直接组合，请使用下文 **过渡方案**（两段 Pipeline）。

---

## 立即可用：过渡方案（两段 Pipeline）

通过 **两条采集配置** 将自监控指标送到 Prometheus RemoteWrite（当前可用方式）：

```text
Pipeline 1（写文件）:  input_internal_metrics  →  flusher_file
Pipeline 2（读文件转发）:  input_file  →  processor_*  →  flusher_prometheus
```

`input_file` 属于可与 Extended Flusher 组合的白名单 Input，因此 Pipeline 2 可在现有版本上加载。Pipeline 1 仅使用 Native 插件，无组合限制。

### Pipeline 1：采集自监控指标并写入本地文件

保存为 `conf/continuous_pipeline_config/local/self_metrics_file.yaml`：

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
    FilePath: self_monitor/self_metrics.log
```

**输出格式**：每行一条 JSON。`__name__` 为指标类别（如 `agent`、`pipeline`），`__labels__` 为标签，`__value__` 为多个指标字段的 map。

```json
{
    "__labels__": {
        "hostname": "host-1",
        "instance_id": "i-xxx"
    },
    "__name__": "agent",
    "__time__": 1735194085,
    "__value__": {
        "cpu": 0.002,
        "memory_used_mb": 25.0,
        "pipeline_config_total": 1.0
    }
}
```

请将 `self_monitor/self_metrics.log` 配置为 **绝对路径**（Pipeline 2 的 `input_file` 需要）。

### Pipeline 2 样例：读取指标文件并 RemoteWrite 到 Prometheus

以下为 Pipeline 2 的参考配置，保存为 `conf/continuous_pipeline_config/local/self_metrics_prometheus.yaml`：

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
  # 将每行 JSON（在 content 字段中）解析为结构化字段
  - Type: processor_json
    SourceKey: content
  # 可选：注入环境、集群等公共 label
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

- `processor_json` 解析后，自监控字段（`__name__`、`__labels__`、`__value__` 等）会出现在日志事件的字段中；`flusher_prometheus` 期望 **Metric 类型** 事件，若 RemoteWrite 无数据，需增加将 JSON 字段转为 Metric 的处理（可参考 [`processor_log_to_sls_metric`](../../plugins/processor/extended/processor-log-to-sls-metric.md) 对 `__name__` / `__value__` 的用法，或按业务自定义 processor）。
- 将 `FilePaths` 替换为 Pipeline 1 实际输出的 **绝对路径**。
- 两段配置需 **同时 enable**；Pipeline 2 会在文件有新行时持续读取并转发。

### 告警数据

`input_internal_alarms` 输出 **LogEvent**，**不适用** 上述 Prometheus 过渡链路。告警请单独使用 `input_internal_alarms` → `flusher_file`（或 `flusher_http` 等日志类 Flusher），参见 [input_internal_alarms 插件](../../plugins/input/native/input-internal-alarms.md)。

---

## 指标命名约定（对接 Prometheus 时）

自监控一条 JSON 指标包含多个字段（`__value__` map）。写入 Prometheus 时建议 **按字段展开** 为多条时间序列：

| JSON 字段 | Prometheus 映射 |
|-----------|-----------------|
| `__name__` = `agent` | 指标名前缀，如 `agent` |
| `__value__.cpu` | 指标名 `agent_cpu`（或 `loongcollector_agent_cpu`） |
| `__labels__` | Prometheus labels（`hostname`、`pipeline_name` 等） |
| `__time__` | sample timestamp（秒） |

规划中的直连能力将按 `{category}_{field}` 规则在 `flusher_prometheus` 编码阶段展开多值 Metric（`category` 为事件名，`field` 为 `__value__` 中的键）。

---

## 规划能力（建设中）

计划支持在 **单条采集配置** 内完成：`input_internal_metrics` →（可选 processor）→ `flusher_prometheus`，无需经过本地文件与 `input_file`。相关开发跟踪 [Issue #2568](https://github.com/alibaba/loongcollector/issues/2568)。

**当前请勿使用下述配置**；加载时会因 Native Input 与 Extended Flusher 组合限制而失败。仅供了解规划中的配置形态。

<details>
<summary>规划中的指标直连 Prometheus 配置样例（建设中，不可用）</summary>

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
    Timeout: "10s"
```

</details>

---

## 故障排查

| 现象 | 可能原因 | 处理建议 |
|------|----------|----------|
| 配置加载失败，`extended flusher plugins coexist with native input` | 在单条配置中混用了 `input_internal_metrics` 与 `flusher_prometheus` | 拆成两段 Pipeline（见上文过渡方案） |
| Pipeline 2 无数据写入 Prometheus | `input_file` 路径错误或 processor 未产出 Metric 事件 | 检查绝对路径、`processor_json` 及 Metric 转换链路 |
| 指标文件无数据 | `input_internal_metrics` 未 Start 或 Interval 未到 | 检查配置 `enable` 与 Interval（单位为分钟） |
| RemoteWrite 4xx/5xx | Endpoint、鉴权、TLS | 检查 `Endpoint`、`Headers`、`Authenticator` |
| Prometheus 中 series 名称与 JSON 不一致 | 多值展开为 `{category}_{field}` | 按本文命名约定调整 Grafana/PromQL |
| 告警未出现在 Prometheus | 告警为 LogEvent | 改用 `flusher_file` / `flusher_http` |

---

## 参考

- GitHub Issue：[Support exporting input_internal_metrics via flusher_prometheus #2568](https://github.com/alibaba/loongcollector/issues/2568)
- [flusher_prometheus 插件说明](../../plugins/flusher/extended/flusher-prometheus.md)
