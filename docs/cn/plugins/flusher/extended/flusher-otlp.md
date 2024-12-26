# OTLP Log

## 简介

`flusher_otlp` `flusher`插件可以实现将采集到的数据，经过处理后，发送到支持`Opentelemetry Protocol`的后端。
v1流水线目前只支持Logs数据，v2流水线支持Logs/Metrics/Traces三种数据。

## 版本

[Alpha](../../stability-level.md)

## 配置参数

| 参数                | 类型       | 是否必选 | 说明                                       |
|-------------------|----------|------|------------------------------------------|
| Type              | String   | 是    | 插件类型                                     |
| Version           | String   | 否    | otlp 协议默认，默认为 v1                         |
| Logs              | Struct   | 否    | Logs gRPC 配置项                                 |
| Logs.Endpoint     | String   | 否    | Logs gRPC Server 地址                           |
| Logs.Compression  | String   | 否    | Logs gRPC 数据压缩协议，可选 gzip、snappy、zstd。默认为 nono |
| Logs.Headers      | String数组 | 否    | Logs gRPC 自定义 Headers                         |
| Logs.Timeout      | int      | 否    | Logs gRPC 连接超时时间，单位为ms，默认为5000                |
| Logs.WaitForReady | bool     | 否    | Logs gRPC 数据发送前是否等待就绪, 默认为false               |
| Metrics              | Struct   | 否    | Metrics gRPC 配置项                                 |
| Metrics.Endpoint     | String   | 否    | Metrics gRPC Server 地址                           |
| Metrics.Compression  | String   | 否    | Metrics gRPC 数据压缩协议，可选 gzip、snappy、zstd。默认为 nono |
| Metrics.Headers      | String数组 | 否    | Metrics gRPC 自定义 Headers                         |
| Metrics.Timeout      | int      | 否    | Metrics gRPC 连接超时时间，单位为ms，默认为5000                |
| Metrics.WaitForReady | bool     | 否    | Metrics gRPC 数据发送前是否等待就绪, 默认为false               |
| Traces              | Struct   | 否    | Traces gRPC 配置项                                 |
| Traces.Endpoint     | String   | 否    | Traces gRPC Server 地址                           |
| Traces.Compression  | String   | 否    | Traces gRPC 数据压缩协议，可选 gzip、snappy、zstd。默认为 nono |
| Traces.Headers      | String数组 | 否    | Traces gRPC 自定义 Headers                         |
| Traces.Timeout      | int      | 否    | Traces gRPC 连接超时时间，单位为ms，默认为5000                |
| Traces.WaitForReady | bool     | 否    | Traces gRPC 数据发送前是否等待就绪, 默认为false               |

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到 `Opentelemetry` Log后端。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_otlp
    Logs:
      Endpoint: http://192.168.xx.xx:8176
      Headers:
        X-AppKey: 8bc8f787-b0b2-4f26-89c6-d3950a090fef
      Retry:
        MaxCount: 3
```

### v2 Pipeline

监听4316端口的`Opentelemetry`的gRPC请求，并将采集结果发送到 `Opentelemetry` 后端。其中Logs发送到<http://192.168.xx.xx:4317，Metrics发送到http://192.168.xx.xx:4319，Traces不发送。>

```yaml
enable: true
version: v2
inputs:
  - Type: service_otlp
    Protocals:
      GRPC:        
        Endpoint: 0.0.0.0:4316
flushers:
  - Type: flusher_otlp
    Logs:
      Endpoint: http://192.168.xx.xx:4317
    Metrics:
      Endpoint: http://192.168.xx.xx:4319   
```

监听4316端口的`Opentelemetry`的gRPC请求，并将采集结果发送到 `Opentelemetry` 后端。Logs、Metrics、Traces发送到不同的后端。

```yaml
enable: true
version: v2
inputs:
  - Type: service_otlp
    Protocals:
      GRPC:        
        Endpoint: 0.0.0.0:4316
flushers:
  - Type: flusher_otlp
    Logs:
      Endpoint: 0.0.0.0:4318
    Metrics:
      Endpoint: 0.0.0.0:4319
    Traces:
      Endpoint: 0.0.0.0:4320
```
