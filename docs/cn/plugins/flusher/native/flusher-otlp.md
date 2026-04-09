# OTLP (gRPC)

## 简介

`flusher_otlp_native` `flusher`插件将内部 `PipelineEventGroup` 中的 Log、Metric、Span 事件通过 OTLP/gRPC 协议发送至外部 OTel Collector 或兼容端点。支持 Logs、Metrics、Traces 三种信号类型以及 RawEvent（作为 Log 发送）。[源代码](https://github.com/alibaba/loongcollector/blob/main/core/plugin/flusher/opentelemetry/FlusherOTLPNative.h)

## 版本

[Alpha](../../stability-level.md)

## 版本说明

* 推荐版本：LoongCollector v3.0.5 及以上

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type | String，无默认值（必填） | 插件类型，固定为`flusher_otlp_native`。 |
| Endpoint | String，无默认值（必填） | 目标 OTel Collector gRPC 地址，格式为`host:port`，例如`localhost:4317`。 |
| TimeoutMs | Int，30000 | gRPC 请求超时时间，单位为毫秒。 |
| EnableTLS | Bool，false | 是否启用 TLS 加密连接。 |
| Headers | Object，无 | 额外的 gRPC 自定义 Headers，键值对格式。 |

## 数据映射

### Logs

| 内部 LogEvent 字段 | OTLP LogRecord 字段 |
|---|---|
| `GetTimestamp()` + `GetTimestampNanosecond()` | `set_time_unix_nano` |
| `GetContent("message")` | `mutable_body()->set_string_value` |
| `GetLevel()` | `set_severity_text` |
| 遍历 Content | `add_attributes()` |
| PipelineEventGroup Tags | `resource.attributes[]` |

### Metrics

| 内部 MetricEvent 字段 | OTLP Metric 字段 |
|---|---|
| `GetName()` | `set_name` |
| `GetValue<UntypedSingleValue>()->mValue` | `mutable_gauge()->add_data_points()->set_as_double` |
| Tags | `data_points[].attributes[]` |

### Traces

| 内部 SpanEvent 字段 | OTLP Span 字段 |
|---|---|
| `GetTraceId()/GetSpanId()/GetParentSpanId()` | `set_trace_id/set_span_id/set_parent_span_id` |
| `GetStartTimeNs()/GetEndTimeNs()` | `set_start_time_unix_nano/set_end_time_unix_nano` |
| `GetStatus()` | `mutable_status()->set_code` |
| `GetKind()` | `set_kind` |
| Tags | `add_attributes()` |
| PipelineEventGroup Tags | `resource.attributes[]` |

### RawEvents

RawEvent 会被转换为 LogRecord 发送，其中 `GetContent()` 作为 log body。

## 样例

接收 OTLP 数据并转发到另一个 OTel Collector：

```yaml
enable: true
inputs:
  - Type: input_forward
    Protocol: OTLP
    Endpoint: 0.0.0.0:4316
flushers:
  - Type: flusher_otlp_native
    Endpoint: localhost:4317
    TimeoutMs: 5000
    EnableTLS: false
```

发送日志到 OTel Collector 并添加自定义 Headers：

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths:
      - /var/log/app/*.log
flushers:
  - Type: flusher_otlp_native
    Endpoint: collector.example.com:4317
    EnableTLS: true
    Headers:
      Authorization: Bearer your-token-here
```
