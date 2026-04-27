# OTLP (HTTP)

## 简介

`flusher_otlp_http_native` `flusher`插件将内部 `PipelineEventGroup` 序列化为 OTLP 格式，通过 HTTP POST 发送至 OTel Collector 的 HTTP 端点。支持 `json` 和 `protobuf` 两种序列化格式。[源代码](https://github.com/alibaba/loongcollector/blob/main/core/plugin/flusher/opentelemetry/FlusherOTLPHttpNative.h)

## 版本

[Alpha](../../stability-level.md)

## 版本说明

* 推荐版本：LoongCollector v3.0.5 及以上

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type | String，无默认值（必填） | 插件类型，固定为`flusher_otlp_http_native`。 |
| Url | String，无默认值（必填） | 目标 OTel Collector HTTP URL，例如`http://localhost:4318/v1/logs`。 |
| Format | String，`protobuf` | 序列化格式，可选值：`protobuf`（`application/x-protobuf`）、`json`（`application/json`）。 |
| EnableTLS | Bool，false | 是否启用 TLS 加密连接。 |
| Headers | Object，无 | 额外的 HTTP 自定义 Headers，键值对格式。 |

## 数据流

```
PipelineEventGroup (in)
  → Send()
    → Format=protobuf: SerializeAndPushProtobuf()
      → OTLPEventGroupSerializer::SerializeToBinaryString()
        → PipelineEventGroup → OTLP protobuf binary
    → Format=json: SerializeAndPush()
      → OTLPEventGroupSerializer::DoSerialize()
        → PipelineEventGroup → OTLP protobuf → JSON string
    → SenderQueueItem 创建 + PushToQueue
      → HttpSink 消费队列
        → BuildRequest() 构造 HTTP POST (Content-Type: application/x-protobuf 或 application/json)
          → OnSendDone() 处理响应
            → 2xx: 释放数据
            → 非 2xx: 保留重试
```

## 样例

使用 protobuf 格式（默认）发送到 OTel Collector：

```yaml
enable: true
inputs:
  - Type: input_forward
    Protocol: OTLP
    Endpoint: 0.0.0.0:4316
flushers:
  - Type: flusher_otlp_http_native
    Url: http://localhost:4318/v1/logs
```

使用 JSON 格式发送到 OTel Collector：

```yaml
enable: true
inputs:
  - Type: input_forward
    Protocol: OTLP
    Endpoint: 0.0.0.0:4316
flushers:
  - Type: flusher_otlp_http_native
    Url: http://localhost:4318/v1/logs
    Format: json
```

发送到 OTel Collector 并添加自定义 Headers 和 TLS：

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths:
      - /var/log/app/*.log
flushers:
  - Type: flusher_otlp_http_native
    Url: https://collector.example.com/v1/logs
    EnableTLS: true
    Headers:
      Authorization: Bearer your-token-here
      X-Custom-Header: custom-value
```
