# 标准输出/文件

## 简介

`flusher_http` `flusher`插件可以实现将采集到的数据，经过处理后，通过http格式发送到指定的地址。

## 版本

[Alpha](../stability-level.md)

## 配置参数

| 参数                           | 类型                 | 是否必选 | 说明                                                                                                                                                                                         |
|------------------------------|--------------------|------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Type                         | String             | 是    | 插件类型，固定为`flusher_http`                                                                                                                                                                     |
| RemoteURL                    | String             | 是    | 要发送到的URL地址，示例：`http://localhost:8086/write`                                                                                                                                                |
| Headers                      | Map<String,String> | 否    | 发送时附加的http请求header，如可添加 Authorization、Content-Type等信息，支持动态变量写法，如`{"x-db":"%{tag.db}"}`<p>v2版本支持从Group的Metadata或者Group.Tags中获取动态变量，如`{"x-db":"%{metadata.db}"}`或者`{"x-db":"%{tag.db}"}`</p> |
| Query                        | Map<String,String> | 否    | 发送时附加到url上的query参数，支持动态变量写法，如`{"db":"%{tag.db}"}`<p>v2版本支持从Group的Metadata或者Group.Tags中获取动态变量，如`{"db":"%{metadata.db}"}`或者`{"db":"%{tag.db}"}`</p>                                          |
| Timeout                      | String             | 否    | 请求的超时时间，默认 `60s`                                                                                                                                                                           |
| Retry.Enable                 | Boolean            | 否    | 是否开启失败重试，默认为 `true`                                                                                                                                                                        |
| Retry.MaxRetryTimes          | Int                | 否    | 最大重试次数，默认为 `3`                                                                                                                                                                             |
| Retry.InitialDelay           | String             | 否    | 首次重试时间间隔，默认为 `1s`，重试间隔以会2的倍数递增                                                                                                                                                             |
| Retry.MaxDelay               | String             | 否    | 最大重试时间间隔，默认为 `30s`                                                                                                                                                                         |
| Convert                      | Struct             | 否    | ilogtail数据转换协议配置                                                                                                                                                                           |
| Convert.Protocol             | String             | 否    | ilogtail数据转换协议，可选值：`custom_single`,`influxdb`, `jsonline`。默认值：`custom_single`<p>v2版本可选值：`raw`</p>                                                                                          |
| Convert.Encoding             | String             | 否    | ilogtail flusher数据转换编码，可选值：`json`, `custom`，默认值：`json`                                                                                                                                     |
| Convert.Separator            | String             | 否    | ilogtail数据转换时，PipelineGroupEvents中多个Events之间拼接使用的分隔符。如`\n`。若不设置，则默认不拼接Events，即每个Event作为独立请求向后发送。 默认值为空。<p>当前仅在`Convert.Protocol: raw`有效。</p>                                               |
| Convert.IgnoreUnExpectedData | Boolean            | 否    | ilogtail数据转换时，遇到非预期的数据的行为，true 跳过，false 报错。默认值 true                                                                                                                                        |
| Convert.TagFieldsRename      | Map<String,String> | 否    | 对日志中tags中的json字段重命名                                                                                                                                                                        |
| Convert.ProtocolFieldsRename | Map<String,String> | 否    | ilogtail日志协议字段重命名，可当前可重命名的字段：`contents`,`tags`和`time`                                                                                                                                      |
| Concurrency                  | Int                | 否    | 向url发起请求的并发数，默认为`1`                                                                                                                                                                        |
| MaxConnsPerHost              | Int                | 否    | 每个host上的最大HTTP连接数（包含了拨号阶段的、活跃的、空闲的），默认`0`，表示不限制<p>当其值大于http.DefaultTransport.(*http.Transport).MaxConnsPerHost时（当前是`0`），会采用该值                                                              |
| MaxIdleConnsPerHost          | Int                | 否    | 每个host上的最大空闲的HTTP连接数，默认`0`，表示不限制<p>当其值大于http.DefaultTransport.(*http.Transport).MaxIdleConnsPerHost时（当前是`0`），会采用该值                                                                         |
| IdleConnTimeout              | String             | 否    | HTTP连接在关闭前保持闲置状态的最长时间，默认`90s`<p>当其值大于http.DefaultTransport.(*http.Transport).IdleConnTimeout时（当前是`90s`），会采用该值                                                                              |
| WriteBufferSize              | Int                | 否    | 写缓冲区的大小，不填不会给http.DefaultTransport.(*http.Transport).WriteBufferSize赋值，此时采用默认的`4KB`<p>当其值大于0时，会采用该值                                                                                        |
| QueueCapacity                | Int                | 否    | 内部channel的缓存大小，默认为1024                                                                                                                                                                     
| AsyncIntercept               | Boolean            | 否    | 异步过滤数据，默认为否                                                                                                                                                                                
| DropEventWhenQueueFull       | Boolean            | 否    | 当队列满时是否丢弃数据，否则需要等待，默认为不丢弃                                                                                                                                                                  |
| Compression                  | string             | 否    | 压缩策略，目前支持gzip和snappy，默认不开启                                                                                                                                                                 |

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果以 `custom_single` 协议、`json`格式提交到 `http://localhost:8086/write`。
且提交时，附加 header x-filepath，其值使用log中的 __Tag__:__path__ 的值

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_http
    RemoteURL: "http://localhost:8086/write"
    Headers:
      x-filepath: "%{tag.__path__}"
    Convert:
      Protocol: custom_single
      Encoding: json
```

采集Docker日志，并将采集结果以`jsonline`协议发送到`http://localhost:9428/insert/jsonline`。

```yaml
enable: true
inputs:
  - Type: service_docker_stdout
    Stderr: true
    Stdout: true
processors:
  - Type: processor_json
    SourceKey: content
    KeepSource: true
    ExpandDepth: 1
    ExpandConnector: ""
    KeepSourceIfParseError: true
flushers:
  - Type: flusher_http
    RemoteURL: http://localhsot:9428/insert/jsonline
    QueueCapacity: 64
    Convert:
      Protocol: jsonline
      Encoding: json
```

需要注意的是，由于使用`jsonline`协议（会将日志的content和tag打平），所以仅支持使用`json`格式进行提交。
由于`jsonline`默认会批量提交日志，所以建议调低`QueueCapacity`，避免在日志量较大的情况下，发生内存占用过多或OOM的问题。

采集Prometheus指标，并将指标以Prometheus协议发送到`PROMETHEUS_REMOTEWRITE_ADDRESS`。
这里用到了`ext_default_encoder`插件，该插件可以配置使用Prometheus Encoder，从而支持将采集到的数据转换为Prometheus协议。
```yaml
enable: true
global:
  StructureType: v2
inputs:
- Type: service_prometheus
  ConfigFilePath: '/etc/prometheus/prometheus.yml'
flushers:
- Type: flusher_http
  RemoteURL: 'http://PROMETHEUS_REMOTEWRITE_ADDRESS/api/v1/write'
  Concurrency: 10
  QueueCapacity: 4096
  DropEventWhenQueueFull: true
  Encoder:
    Type: ext_default_encoder
    Format: 'prometheus'
    SeriesLimit: 1024
  Authenticator:
    Type: ext_basicauth
extensions:
- Type: ext_basicauth
  Username: 'YOUR_USERNAME'
  Password: 'YOUR_PASSWORD'
```