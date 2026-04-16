# 转发输入插件

## 简介

`input_forward` 插件用于接收来自其他系统的数据转发请求，支持 LoongSuite 和 OTLP 协议。该插件可以作为数据转发的接收端，通过配置的匹配规则来处理接收到的数据。[源代码](https://github.com/alibaba/loongcollector/blob/main/core/plugin/input/InputForward.h)

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：LoongCollector v2.0.0 及以上

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type | String，无默认值（必填） | 插件类型，固定为`input_forward`。 |
| Protocol | String，无默认值（必填） | 转发协议类型。目前支持：`LoongSuite`、`OTLP`。 |
| Endpoint | String，无默认值（必填） | 监听地址和端口，格式为`IP:PORT`，例如`0.0.0.0:7899`。或者本地通信socket，例如`/root/loongcollector.sock`。 |

## 转发规则

### LoongSuite

LoongCollector 根据请求中的采集配置名进行转发，如果没有匹配的路由规则，则返回错误状态码。

#### GRPC
metadata 中配置 key 为 `X-Loongsuite-Apm-Configname`，value 为采集配置名。
返回状态码：
* OK：转发成功
* NOT_FOUND：没有匹配的采集配置
* INVALID_ARGUMENT：请求参数错误
* UNAVAILABLE：转发阻塞，请重试

## 样例

### 接收LoongSuite协议数据

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_forward
    Protocol: LoongSuite
    Endpoint: 0.0.0.0:7899
flushers:
  - Type: flusher_sls
    Project: "your-project"
    Logstore: "your-logstore"
    Region: cn-shanghai
    Endpoint: cn-shanghai.log.aliyuncs.com
```

* 输入

通过gRPC客户端发送LoongSuite协议的转发请求，请求需要在metadata中包含`x-loongsuite-apm-configname: <采集配置名>`字段，数据内容在请求的data字段中，为 `PipelineEventGroup` PB 序列化的结果。

* 输出

以下为示意：实际字段取决于上游 PB 解析后的事件内容及下游 Flusher 序列化方式。

```json
{
    "content": "接收到的原始数据内容",
    "__time__": "1642502400"
}
```

### OTLP

接收标准 OpenTelemetry Protocol (OTLP/gRPC) 的 Logs、Metrics、Traces 数据，将其转换为内部 `PipelineEventGroup` 格式并推入采集管道。

#### 数据映射

**Logs：**
- `time_unix_nano` → LogEvent 时间戳（秒 + 亚秒纳秒）
- `body.string_value` → `SetContent("content", ...)`
- `severity_text` → `SetContent("severity", ...)`
- `trace_id`, `span_id` → `SetContent("trace_id"/"span_id", ...)`
- `log_record.attributes[]` → `SetContent(key, value)`
- `scope.attributes[]` → `SetContent("__tag__:" + key, value)`（假 tag，scope 层不常用）
- `resource.attributes[]` → `eventGroup.SetTag(key, value)`（真 tag）

**Metrics：**
- `name` → `SetName(...)`
- `gauge/sum/histogram.data_points[].as_double()` → `SetValue<UntypedSingleValue>(...)`
- `scope.attributes[]` → `SetTag("__tag__:" + key, value)`（假 tag）
- `resource.attributes[]` → `eventGroup.SetTag(key, value)`（真 tag）

**Traces：**
- `trace_id`, `span_id`, `parent_span_id` → `SetTraceId/SetSpanId/SetParentSpanId`
- `name` → `SetName`
- `start_time_unix_nano`, `end_time_unix_nano` → `SetStartTimeNs/SetEndTimeNs`
- `status.code` → `SetStatus(Ok/Error/Unset)`
- `kind` → `SetKind(Client/Server/...)`
- `span.attributes[]` → `SetTag(key, value)`
- `scope.attributes[]` → `SetTag("__tag__:" + key, value)`（假 tag）
- `resource.attributes[]` → `eventGroup.SetTag(key, value)`（真 tag）

#### 配置匹配

服务通过 gRPC metadata 中的 `x-otlp-apm-configname` 字段匹配采集配置。如果仅注册了一个配置（如 onetime pipeline 场景），则自动匹配该配置。

返回状态码：
* `OK`：转发成功
* `NOT_FOUND`：没有匹配的采集配置
* `INVALID_ARGUMENT`：请求参数错误（如空请求体）
* `UNAVAILABLE`：队列已满，请重试

#### 样例

监听 4316 端口的 OTLP gRPC 请求，将数据转发到下游处理：

```yaml
enable: true
inputs:
  - Type: input_forward
    Protocol: OTLP
    Endpoint: 0.0.0.0:4316
flushers:
  - Type: flusher_sls
    Project: "your-project"
    Logstore: "your-logstore"
    Region: cn-shanghai
    Endpoint: cn-shanghai.log.aliyuncs.com
```

结合 `flusher_otlp` 实现 OTLP 接收-转发全链路：

```yaml
enable: true
inputs:
  - Type: input_forward
    Protocol: OTLP
    Endpoint: 0.0.0.0:4316
flushers:
  - Type: flusher_otlp
    Endpoint: localhost:4317
    TimeoutMs: 5000
    EnableTLS: false
```
