# ServiceInput示例插件

## 简介

`service_http_server` `input`插件可以接收来自unix socket、http/https、tcp的请求，并支持sls协议、otlp等多种协议。

## 配置参数

| 参数                 | 类型       | 是否必选 | 说明                                                                                                                                |
|--------------------|----------|-----|-----------------------------------------------------------------------------------------------------------------------------------|
| Type               | String   | 是   | 插件类型，固定为`service_http_server`                                                                                                     |
| Format             | String   | 否   | <p>数据格式。</p> <p>支持格式：`sls`、`prometheus`、`influxdb`、`otlp_logv1`、`statsd`</p>  <p>v2版本支持格式: `raw`</p><p>说明：`raw`格式以原始请求字节流传输数据</p> |
| Address            | String   | 否   | <p>监听地址。</p><p></p>                                                                                                               |
| ReadTimeoutSec     | String   | 否   | <p>读取超时时间。</p><p>默认取值为:`10s`。</p>                                                                                                 |
| ShutdownTimeoutSec | String   | 否   | <p>关闭超时时间。</p><p>默认取值为:`5s`。</p>                                                                                                  |
| MaxBodySize        | String   | 否   | <p>最大传输 body 大小。</p><p>默认取值为:`64k`。</p>                                                                                           |
| UnlinkUnixSock     | String   | 否   | <p>启动前如果监听地址为unix socket，是否进行强制释放。</p><p>默认取值为:`true`。</p>                                                                        |
| FieldsExtend       | Boolean  | 否   | <p>是否支持非integer以外的数据类型(如String)</p><p>目前仅针对有 String、Bool 等额外类型的 influxdb Format 有效</p>                                            |
| QueryParams        | []String | 否   | 需要解析到Group.Metadata中的请求参数。<p>解析结果会以KeyValue放入Metadata。默认取值为`[]`，即不解析。</p><p>仅v2版本有效</p>                                           |
| QueryParamPrefix   | String   | 否   | 解析请求参数时需要添加的key前缀，如`_query_param_`。<p>前缀会直接拼接在每个QueryParam前，无额外连接符，默认取值为空，即不增加前缀。</p><p>仅v2版本有效</p>                               |
| HeaderParams       | []String | 否   | 需要解析到Group.Metadata中的header参数。<p>解析结果会以KeyValue放入Metadata。默认取值为`[]`，即不解析。</p><p>仅v2版本有效</p>                                       |
| HeaderParamPrefix   | String   | 否   | 解析Header参数时需要添加的key前缀，如`_header_param_`。<p>前缀会直接拼接在每个HeaderParam前，无额外连接符，默认取值为空，即不增加前缀。</p><p>仅v2版本有效</p>                         |
| DisableUncompress  | Boolean  | 否   | 禁用对于请求数据的解压缩, 默认取值为:`false`<p>目前仅针对Raw Format有效</p><p>仅v2版本有效</p>                                                                 |


## 样例

### 接收 OTLP 日志

* 采集配置

```yaml
enable: true
inputs:
  - Type: service_http_server
    Format: "otlp_logv1"
    Address: "http://127.0.0.1:12345"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true  
```

* 输入

使用 opentelemetry-java-sdk构造数据，基于[ExampleConfiguration.java](https://github.com/open-telemetry/opentelemetry-java-docs/blob/main/otlp/src/main/java/io/opentelemetry/example/otlp/ExampleConfiguration.java)、[OtlpExporterExample.java](https://github.com/open-telemetry/opentelemetry-java-docs/blob/main/otlp/src/main/java/io/opentelemetry/example/otlp/OtlpExporterExample.java)进行如下代码改造。

`ExampleConfiguration` 进行 `OTLP SDK` 初始化。
```java
  static OpenTelemetrySdk initHTTPOpenTelemetry() {
    // Include required service.name resource attribute on all spans and metrics
    Resource resource =
        Resource.getDefault()
            .merge(Resource.builder().put(SERVICE_NAME, "OtlpExporterExample").build());

    OpenTelemetrySdk openTelemetrySdk =
        OpenTelemetrySdk.builder()
            .setLogEmitterProvider(
                SdkLogEmitterProvider.builder()
                    .setResource(resource)
                    .addLogProcessor(SimpleLogProcessor
                        .create(OtlpHttpLogExporter
                                .builder()
                                .setEndpoint("http://127.0.0.1:12345/v1/logs")
                                .build()))
                    .build())
            .buildAndRegisterGlobal();

    Runtime.getRuntime()
        .addShutdownHook(new Thread(openTelemetrySdk.getSdkLogEmitterProvider()::shutdown));

    return openTelemetrySdk;
  }
```

`OtlpExporterExample` 构造数据。
```java
  OpenTelemetrySdk openTelemetry = ExampleConfiguration.initHTTPOpenTelemetry();

  LogEmitter logger = openTelemetry.getSdkLogEmitterProvider().get("io.opentelemetry.example");
  logger
    .logRecordBuilder()
    .setBody("log body1")
    .setAllAttributes(
      Attributes.builder()
                  .put("k1", "v1")
                  .put("k2", "v2").build())
    .setSeverity(Severity.INFO)
    .setSeverityText("INFO")
    .setEpoch(Instant.now())
    .setContext(Context.current())
    .emit();
```

* 输出

```
{
    "time_unix_nano": "1663913736115000000",
    "severity_number": "9",
    "severity_text": "INFO",
    "content": "log body1",
    "attributes": "{\"k1\":\"v1\",\"k2\":\"v2\"}",
    "resources": "{\"service.name\":\"OtlpExporterExample\",\"telemetry.sdk.language\":\"java\",\"telemetry.sdk.name\":\"opentelemetry\",\"telemetry.sdk.version\":\"1.18.0\"}",
    "__time__": "1663913736"
}
```

### 接收字节流数据

* 采集配置

```yaml
enable: true
version: v2
inputs:
  - Type: service_http_server
    Format: "raw"
    Address: "http://127.0.0.1:12345"
    QueryParams:
      - "QueryKey"
    QueryParamPrefix: "_param_prefix_"
    HeaderParams:
      - "HeaderKey"
    HeaderParamPrefix: "_header_prefix_"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
    Tags: true
```

* 输入

手动构造请求
```shell
curl --location --request POST 'http://127.0.0.1:12345?QueryKey=queryValue' --header 'HeaderKey: headerValue' --header 'Content-Type: text/plain' --data 'test_measurement,host=server01,region=cn value=0.5'
```

* 输出
```
[Event] event 1, metadata map[_header_prefix_HeaderKey:headerValue _param_prefix_QueryKey:queryValue], tags map[__hostname__:579ce1e01dea]

{
    "eventType":"byteArray",
    "name":"",
    "timestamp":0,
    "observedTimestamp":0,
    "tags":{
    },
    "byteArray":"test_measurement,host=server01,region=cn value=0.5"
}
```