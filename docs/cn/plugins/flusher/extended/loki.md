# Loki

## 简介

`flusher_loki` `flusher`插件可以实现将采集到的数据，经过处理后，发送到 Loki。

## 版本

Alpha

## 配置参数

| 参数                           | 类型       | 是否必选          | 说明                                                                                            |
|------------------------------|----------|---------------|-----------------------------------------------------------------------------------------------|
| Type                         | String   | 是             | 插件类型                                                                                          |
| Convert                      | Struct   | 否             | ilogtail数据转换协议配置                                                                              |
| Convert.Protocol             | String   | 否             | ilogtail数据转换协议，可选值：`custom_single`, `custom_single_flatten`,`otlp_log_v1`。默认值：`custom_single` |
| Convert.Encoding             | String   | 否             | ilogtail数据转换编码，可选值：`json`、`none`、`protobuf`，默认值：`json`                                        |
| Convert.TagFieldsRename      | Map      | 否             | 对日志中tags中的json字段重命名                                                                           |
| Convert.ProtocolFieldsRename | Map      | 否             | ilogtail日志协议字段重命名，可当前可重命名的字段：`contents`,`tags`和`time`                                         |
| Convert.OnlyContents         | Bool     | 否             | 仅发送contents中的字段，目前只能和`custom_single_flatten`协议一起使用，默认值：`false`                                |
| URL                          | String   | 是             | Loki 推送地址，例如：`http://localhost:3100/loki/api/v1/push`                                         |
| TenantID                     | String   | 否             | Loki 的租户 ID（需要 Loki 开启该功能），默认为空，表示单租户模式                                                       |
| MaxMessageWait               | Int      | 否             | 发送 batch 前的最长等待时间，默认 `1` 秒                                                                    |
| MaxMessageBytes              | Int      | 否             | 发送积累的最大的 batch 大小，默认 `1024 * 1024` bytes                                                      |
| Timeout                      | Int      | 否             | 等待 Loki 响应的最大时间，默认 `10` 秒                                                                     |
| MinBackoff                   | Int      | 否             | 重试之间的最短退避时间，默认 `500`毫秒                                                                        |
| MaxBackoff                   | Int      | 否             | 重试的最长退避时间，默认 `5`分钟                                                                            |
| MaxRetries                   | Int      | 否             | 最大重试次数，默认 `10`                                                                                |
| DynamicLabels                | String数组 | 两种Label至少选择一项 | 需要从日志中动态解析的标签列表，例如：`content.field1`                                                           |
| StaticLabels                 | Map      | 两种Label至少选择一项 | 需要添加到每条日志上的静态标签                                                                               |

## 样例

本样例采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到 Loki。在执行该任务之前，需要确保系统已经安装了Loki。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_loki
    URL: http://<loki 服务的地址与端口>/loki/api/v1/push
    TenantID: ilogtail
    MaxMessageWait: 100000000
    MaxMessageBytes: 16
    Timeout: 1000000000000
    MinBackoff: 100000000000
    MaxBackoff: 1000000000000
    MaxRetries: 10
    StaticLabels:
      source: ilogtail
```

运行 `ilogtail` 并收集到日志后，在 `Grafana` 或 `Logcli` 中可以通过以下的命令查询日志：

```plain
{ source="ilogtail" }
```

## 进阶配置

以下面的一段日志为例，后来将展开介绍ilogtail loki flusher的一些高阶配置

```plain
2022-07-22 10:19:23.684 ERROR [springboot-docker] [http-nio-8080-exec-10] com.benchmark.springboot.controller.LogController : error log
```

接下来，我们通过`ilogtail`的`processor_regex`插件，将上面的日志提取处理后几个关键字段。

- time
- level
- application
- thread
- class
- message

最后推送到`Loki`的数据样例如下：

```json
{
  "contents": {
    "class": "org.springframework.web.servlet.DispatcherServlet@initServletBean:547",
    "application": "springboot-docker",
    "level": "ERROR",
    "message": "error log",
    "thread": "http-nio-8080-exec-10",
    "time": "2022-07-20 16:55:05.415"
  },
  "tags": {
    "k8s.namespace.name":"java_app",
    "host.ip": "192.168.6.128",
    "host.name": "master",
    "log.file.path": "/home/test-log/test.log"
  },
  "time": 1664435098
}
```

### 动态 Label

针对上面写入的这种日志格式，如果想在 Loki 中根据`application`名称进行查询，那么需要进行如下的配置。

```yaml
DynamicLabels:
  - content.application
```

`flusher_loki` 会将去除前缀后的字符串作为存入 Loki 的 label。之后，在 Loki 中即可以 `{application="springboot-docker"}` 进行查询。
`DynamicLabels` 表达式规则：

- `content.fieldname`：`content` 代表从 `contents` 中解析指定字段值。
- `tag.fieldname`：`tag` 代表从 `tags` 中解析指定字段值。

### tag 重命名

`ilogtail` 中的 `converter` 支持通过配置对 `tags` 中的字段进行重命名。在 flusher_loki 中进行如下的配置，即可对存入 Loki 中动态 label 进行重命名：

```yaml
flushers:
  - Type: flusher_loki
    URL: http://<loki 服务的地址与端口>/loki/api/v1/push
    DynamicLabels:
      - tag.my.host.name
    Convert:
      TagFieldsRename:
        host.name: my.host.name
```

之后在 Loki 中进行查询时，可以通过重命名后的 label， `{my.host.name="master"}` 进行查询。
