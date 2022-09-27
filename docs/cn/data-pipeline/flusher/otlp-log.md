# OTLP Log

## 简介
`flusher_otlp_log` `flusher`插件可以实现将采集到的数据，经过处理后，发送到支持`Opentelemetry log protocol`的后端。

## 配置参数

| 参数                | 类型       | 是否必选 | 说明                                       |
|-------------------|----------|------|------------------------------------------|
| Type              | String   | 是    | 插件类型                                     |
| Version           | String   | 否    | otlp 协议默认，默认为 v1                         |
| Grpc              | Struct   | 是    | gRPC 配置项                                 |
| Grpc.Endpoint     | String   | 是    | gRPC Server 地址                           |
| Grpc.Compression  | String   | 否    | gRPC 数据压缩协议，可选 gzip、snappy、zstd。默认为 nono |
| Grpc.Headers      | String数组   | 否    | gRPC 自定义 Headers                         |


## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到 `、Opentelemetry` 后端。

```
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test_log
    FilePattern: "*.log"
flushers:
  - Type: flusher_otlp_log
    Grpc:
      Endpoint: http://192.168.xx.xx:8176
      Headers:
        X-AppKey: 8bc8f787-b0b2-4f26-89c6-d3950a090fef
      Retry:
        MaxCount: 3
```
