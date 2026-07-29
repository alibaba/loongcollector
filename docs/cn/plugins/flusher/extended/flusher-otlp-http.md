# OTLP/HTTP

## 简介

`flusher_otlp_http` `flusher` 插件可以实现将采集到的数据，经过处理后，通过 **OTLP/HTTP** 协议发送到支持 `OpenTelemetry Protocol` 的后端。协议行为对齐 OpenTelemetry Collector 的 `otlphttpexporter`：以 HTTP POST 提交 protobuf 或 JSON 编码的 OTLP `ExportRequest`。

与走 gRPC 的 [`flusher_otlp`](flusher-otlp.md) 相比，本插件适用于后端只开放 HTTP 端口（通常是 `4318`）、或链路上存在只支持 HTTP/1.1 的网关、负载均衡的场景。

> **当前版本仅支持 Logs 数据。** Metrics / Traces 将在后续版本支持；v2 采集配置下遇到 Metric / Span 事件时会被丢弃，不会导致采集配置报错。

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| v1 接口 | v2 接口 | Log | Metric | Span |
| --- | --- | --- | --- | --- |
| ✓ | ✓ | v1&v2 | — | — |

## 版本

[Alpha](../../stability-level.md)

## 配置参数

| 参数 | 类型 | 是否必选 | 说明 |
| --- | --- | --- | --- |
| Type | string | 是 | 插件类型，固定为 `flusher_otlp_http` |
| Version | string | 否 | otlp 协议版本，默认为 `v1` |
| Endpoint | string | 否 | 基础地址，如 `http://192.168.xx.xx:4318`。各信号的路径会自动追加，Logs 为 `/v1/logs`。当 `Logs.Endpoint` 已配置时可省略 |
| Logs | Struct | 否 | Logs 信号的独立配置 |
| Logs.Endpoint | string | 否 | Logs 的完整 URL。**配置后完全覆盖 `Endpoint`，且不再追加 `/v1/logs`**，需自行写完整路径 |
| Logs.Headers | Map | 否 | Logs 专属 HTTP Headers，与 `Headers` 合并，同名键以此处为准 |
| Encoding | string | 否 | 请求体编码，可选 `proto`、`json`，默认为 `proto`。对应 `Content-Type` 分别为 `application/x-protobuf`、`application/json` |
| Compression | string | 否 | 请求体压缩，可选 `gzip`、`none`，默认为 `gzip`。压缩时会自动带上 `Content-Encoding: gzip` |
| Headers | Map | 否 | 追加到每个请求的自定义 HTTP Headers |
| Timeout | string | 否 | 单次请求的超时时间，默认 `30s` |
| Retry.Enable | bool | 否 | 是否开启重试，默认为 `true` |
| Retry.MaxRetryTimes | int | 否 | 最大重试次数，默认为 `3`（即单批数据最多请求 4 次） |
| Retry.InitialDelay | string | 否 | 首次重试的时间间隔，默认为 `1s`，之后以 2 的倍数递增 |
| Retry.MaxDelay | string | 否 | 重试间隔的上限，默认为 `30s` |
| MaxConnsPerHost | int | 否 | 单个 Host 的最大连接数，大于 `http.DefaultTransport` 的默认值时生效 |
| MaxIdleConnsPerHost | int | 否 | 单个 Host 的最大空闲连接数，大于默认值时生效 |
| IdleConnTimeout | string | 否 | HTTP 连接保持空闲的最长时间，大于默认值（`90s`）时生效 |
| WriteBufferSize | int | 否 | HTTP 连接的写缓冲区大小，单位为字节 |
| Authenticator | Struct | 否 | 认证扩展的类型与配置，需实现 `extensions.ClientAuthenticator`，如 `ext_basicauth` |
| RequestInterceptors | Struct 数组 | 否 | 请求拦截器扩展链，需实现 `extensions.RequestInterceptor` |

`Content-Type` 与 `Content-Encoding` 由 `Encoding`、`Compression` 决定，即使在 `Headers` 中配置了同名项也不会生效，以避免请求头与实际请求体不一致。`User-Agent` 可以通过 `Headers` 覆盖。

## 重试策略

按 OTLP/HTTP 规范，仅以下响应会被重试：

| 情况 | 是否重试 | 说明 |
| --- | --- | --- |
| 连接失败、超时、连接重置 | 是 | 按指数退避重试 |
| `429 Too Many Requests` | 是 | 解析 `Retry-After` |
| `502 Bad Gateway` | 是 | 按指数退避重试 |
| `503 Service Unavailable` | 是 | 解析 `Retry-After` |
| `504 Gateway Timeout` | 是 | 按指数退避重试 |
| 其他状态码（含 `400`、`401`、`403`、`413`、**`500`**） | 否 | 视为永久失败，数据丢弃并告警 |

`429`、`503` 响应中的 `Retry-After` 支持整数秒（如 `Retry-After: 5`）和 HTTP 日期（RFC 1123）两种格式，解析结果会作为本次退避时间的下界，即实际等待时间为 `max(指数退避, Retry-After)`。

响应码为 `2xx` 但响应体中带有 `partial_success` 时，会记录告警日志并将本批数据视为发送成功，不会重发（重发会导致已接收的数据重复）。

## 样例

### v1 Pipeline

采集 `/home/test-log/` 路径下的所有文件名匹配 `*.log` 规则的文件，并将采集结果发送到 `Opentelemetry` 后端的 `http://192.168.xx.xx:4318/v1/logs`。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths:
      - /home/test-log/*.log
flushers:
  - Type: flusher_otlp_http
    Endpoint: http://192.168.xx.xx:4318
    Headers:
      X-AppKey: 8bc8f787-b0b2-4f26-89c6-d3950a090fef
    Retry:
      MaxRetryTimes: 3
```

### v2 Pipeline

监听 4316 端口的 `Opentelemetry` gRPC 请求，将采集结果以 JSON 编码、不压缩的方式发送到 `http://192.168.xx.xx:4318/v1/logs`。

```yaml
enable: true
global:
  StructureType: v2
inputs:
  - Type: service_otlp
    Protocals:
      GRPC:
        Endpoint: 0.0.0.0:4316
flushers:
  - Type: flusher_otlp_http
    Endpoint: http://192.168.xx.xx:4318
    Encoding: json
    Compression: none
```

### 自定义 Logs 路径

当后端的日志接收路径不是标准的 `/v1/logs` 时，用 `Logs.Endpoint` 指定完整 URL。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths:
      - /home/test-log/*.log
flushers:
  - Type: flusher_otlp_http
    Logs:
      Endpoint: http://192.168.xx.xx:8080/api/otlp/logs
      Headers:
        X-Tenant: tenant-a
    Timeout: 10s
```

### 配合认证扩展

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths:
      - /home/test-log/*.log
flushers:
  - Type: flusher_otlp_http
    Endpoint: http://192.168.xx.xx:4318
    Authenticator:
      Type: ext_basicauth
extensions:
  - Type: ext_basicauth
    Username: user
    Password: password
```
