# ServiceInput示例插件

## 简介

`service_http_server` `input`插件可以接收来自unix socket、http/https、tcp的请求，并支持sls协议、otlp等多种协议。

## 配置参数

| 参数    | 类型   | 是否必选 | 说明                                         |
| ------- | ------ | -------- | -------------------------------------------- |
| Type    | String | 是       | 插件类型，固定为`service_http_server`      |
| Format | String | 否       | <p>数据格式。</p> <p>支持格式：`sls`、`prometheus`、`influxdb`、`otlp_logv1`、`statsd`</p> |
| Address | String | 否       | <p>监听地址。</p><p></p> |
| ReadTimeoutSec | String | 否       | <p>读取超时时间。</p><p>默认取值为:`10s`。</p> |
| ShutdownTimeoutSec | String | 否       | <p>关闭超时时间。</p><p>默认取值为:`5s`。</p> |
| MaxBodySize | String | 否       | <p>最大传输 body 大小。</p><p>默认取值为:`64k`。</p> |
| UnlinkUnixSock | String | 否       | <p>启动前如果监听地址为unix socket，是否进行强制释放。</p><p>默认取值为:`true`。</p> |

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