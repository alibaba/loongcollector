# Open Telemetry Service Input

## 简介

`service_otlp` `input`插件实现了`ServiceInputV1`和`ServiceInputV2`接口，可以接受`Opentelemetry log/metric/trace protocol`的http/gRPC请求，并且转换输出SLSProto或PipelineGroupEvents。

## 版本

[Beta](../stability-level.md)

## 配置参数

| 参数               | 类型      | 是否必选 | 说明                                       |
|-------------------|----------|-------|------------------------------------------|
| Type              | String   | 是    | 插件类型, 固定为`service_otlp`。                        |
| Protocals           | Struct   | 是    |   <p>接收的协议</p>                       |
| Protocals.GRPC    | Struct | 否    | 是否启用gRPC Server                                |
| Protocals.GRPC.Endpoint | string   | 否    | <p>gRPC Server 地址。</p><p>默认取值为:`0.0.0.0:4317`。</p>                            |
| Protocals.GRPC.MaxRecvMsgSizeMiB | int   | 否    | gRPC Server 最大接受Msg大小。                           |
| Protocals.GRPC.MaxConcurrentStreams | int   | 否    | gRPC Server 最大并发流。                           |
| Protocals.GRPC.ReadBufferSize       | int   | 否    | gRPC Server读缓存大小。 |
| Protocals.GRPC.WriteBufferSize      | int   | 否    | gRPC Server写缓存大小。               |
| Protocals.GRPC.Compression      | string   | 否    | gRPC Server压缩算法，可以用gzip。               |
| Protocals.GRPC.Decompression      | string   | 否    | gRPC Server解压算法，可以用gzip。               |
| Protocals.GRPC.TLSConfig      | Struct   | 否    | gRPC Server TLS CONFIG配置。               |
| Protocals.HTTP    | Struct | 否    | 是否启用HTTP Server                                |
| Protocals.HTTP.Endpoint | string   | 否    | <p>HTTP Server 地址。</p><p>默认取值为:`0.0.0.0:4318`。</p>                            |
| Protocals.HTTP.MaxRecvMsgSizeMiB | int   | 否    | HTTP Server 最大接受Msg大小。 <p>默认取值为:`64(MiB)`。</p>                          |
| Protocals.HTTP.ReadTimeoutSec | int   | 否    |  <p>HTTP 请求读取超时时间。</p><p>默认取值为:`10s`。</p>                           |
| Protocals.HTTP.ShutdownTimeoutSec       | int   | 否    | <p>HTTP Server关闭超时时间。</p><p>默认取值为:`5s`。</p> |

## 样例

* 只接收gRPC请求。
  
```yaml
enable: true
version: v2
inputs:
  - Type: service_otlp
    Protocals:
      GRPC:     
flushers:
  - Type: flusher_stdout
    OnlyStdout: true  
```

* 接收http/gRPC请求，使用默认otlp的默认端口。gRPC：4317，HTTP：4318.

```yaml
enable: true
version: v2
inputs:
  - Type: service_otlp
    Protocals:
      GRPC:        
      HTTP:        
flushers:
  - Type: flusher_stdout
    OnlyStdout: true  
```

* 完整配置
  
```yaml
enable: true
version: v2
inputs:
  - Type: service_otlp
    Protocals:
      GRPC:        
        Endpoint: 0.0.0.0:4317
        MaxRecvMsgSizeMiB: 64
        MaxConcurrentStreams: 100
        ReadBufferSize: 1024
        WriteBufferSize: 1024
      HTTP:
        Endpoint: 0.0.0.0:4318
        MaxRecvMsgSizeMiB: 64
        ReadTimeoutSec: 10
        ShutdownTimeoutSec: 5
flushers:
  - Type: flusher_stdout
    OnlyStdout: true  
```
