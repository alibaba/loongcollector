# Prometheus

## 简介

`flusher_prometheus` `flusher`插件可以实现将采集到的数据，经过处理后，通过http格式发送到指定的 Prometheus RemoteWrite 地址。
参数配置大部分继承`flusher_http`，详见[flusher_http](flusher-http.md)。

## 版本

[Alpha](../../stability-level.md)

## 配置参数

| 参数                     | 类型                  | 是否必选 | 说明                                                                                                                                                                                                                                      |
|------------------------|---------------------|------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Type                   | String              | 是    | 插件类型，固定为`flusher_prometheus`                                                                                                                                                                                                            |
| Endpoint               | String              | 是    | 要发送到的URL地址，遵从Prometheus RemoteWrite协议，示例：`http://localhost:8086/api/v1/write`                                                                                                                                                           |
| SeriesLimit            | Int                 | 否    | 一次序列化 Prometheus RemoteWrite 请求的时间序列的最大长度，默认1000                                                                                                                                                                                        |
| Headers                | Map<String,String>  | 否    | 发送时附加的http请求header，如可添加 Authorization、Content-Type等信息，支持动态变量写法，如`{"x-db":"%{tag.db}"}`<p>v2版本支持从Group的Metadata或者Group.Tags中获取动态变量，如`{"x-db":"%{metadata.db}"}`或者`{"x-db":"%{tag.db}"}`</p><p>默认注入prometheus相关的Header（e.g. snappy压缩）</p> |
| Query                  | Map<String,String>  | 否    | 发送时附加到url上的query参数，支持动态变量写法，如`{"db":"%{tag.db}"}`<p>v2版本支持从Group的Metadata或者Group.Tags中获取动态变量，如`{"db":"%{metadata.db}"}`或者`{"db":"%{tag.db}"}`</p>                                                                                       |
| Timeout                | String              | 否    | 请求的超时时间，默认 `60s`                                                                                                                                                                                                                        |
| Retry.Enable           | Boolean             | 否    | 是否开启失败重试，默认为 `true`                                                                                                                                                                                                                     |
| Retry.MaxRetryTimes    | Int                 | 否    | 最大重试次数，默认为 `3`                                                                                                                                                                                                                          |
| Retry.InitialDelay     | String              | 否    | 首次重试时间间隔，默认为 `1s`，重试间隔以会2的倍数递增                                                                                                                                                                                                          |
| Retry.MaxDelay         | String              | 否    | 最大重试时间间隔，默认为 `30s`                                                                                                                                                                                                                      |
| Concurrency            | Int                 | 否    | 向url发起请求的并发数，默认为`1`                                                                                                                                                                                                                     |
| MaxConnsPerHost        | Int                 | 否    | 每个host上的最大HTTP连接数（包含了拨号阶段的、活跃的、空闲的），默认`50`                                                                                                                                                                                              |
| MaxIdleConnsPerHost    | Int                 | 否    | 每个host上的最大空闲的HTTP连接数，默认`50`                                                                                                                                                                                                             |
| IdleConnTimeout        | String              | 否    | HTTP连接在关闭前保持闲置状态的最长时间，默认`90s`<p>当其值大于http.DefaultTransport.(*http.Transport).IdleConnTimeout时（当前是`90s`），会采用该值                                                                                                                           |
| WriteBufferSize        | Int                 | 否    | 写缓冲区的大小，默认`64KB`                                                                                                                                                                                                                        |
| QueueCapacity          | Int                 | 否    | 内部channel的缓存大小，默认为1024                                                                                                                                                                                                                  
| Authenticator          | Struct              | 否    | 鉴权扩展插件配置                                                                                                                                                                                                                                |
| Authenticator.Type     | String              | 否    | 鉴权扩展插件类型                                                                                                                                                                                                                                |
| Authenticator.Options  | Map<String,Struct>  | 否    | 鉴权扩展插件配置内容                                                                                                                                                                                                                              |
| AsyncIntercept         | Boolean             | 否    | 异步过滤数据，默认为否                                                                                                                                                                                                                             
| DropEventWhenQueueFull | Boolean             | 否    | 当队列满时是否丢弃数据，否则需要等待，默认为丢弃                                                                                                                                                                                                                |

## 样例

采集Prometheus指标，并将指标以Prometheus协议发送到`PROMETHEUS_REMOTEWRITE_ADDRESS`。
这里用到了`ext_default_encoder`插件（默认集成，无需用户手动配置），该插件可以配置使用Prometheus Encoder，从而支持将采集到的数据转换为Prometheus协议。
```yaml
enable: true
global:
  StructureType: v2
inputs:
- Type: service_prometheus
  ConfigFilePath: '/etc/prometheus/prometheus.yml'
flushers:
- Type: flusher_prometheus
  Endpoint: 'http://PROMETHEUS_REMOTEWRITE_ADDRESS/api/v1/write'
  Concurrency: 10
  QueueCapacity: 4096
  DropEventWhenQueueFull: true
  Authenticator:
    Type: ext_basicauth
extensions:
- Type: ext_basicauth
  Username: 'YOUR_USERNAME'
  Password: 'YOUR_PASSWORD'
```