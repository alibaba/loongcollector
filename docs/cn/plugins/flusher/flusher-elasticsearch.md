# ElasticSearch

## 简介

`flusher_elasticsearch` `flusher`插件可以实现将采集到的数据，经过处理后，发送到 ElasticSearch，需要 ElasticSearch 版本至少为 `8`。

## 版本

[Alpha](../stability-level.md)

## 配置参数

| 参数                                | 类型       | 是否必选 | 说明                                                                                                                 |
|-----------------------------------|----------|------|--------------------------------------------------------------------------------------------------------------------|
| Type                              | String   | 是    | 插件类型，固定为`flusher_elasticsearch`                                                                                    |
| Addresses                         | String数组 | 是    | ElasticSearch 地址                                                                                                   |
| Convert                           | Struct   | 否    | ilogtail数据转换协议配置                                                                                                   |
| Convert.Protocol                  | String   | 否    | ilogtail数据转换协议，elasticsearch flusher 可选值：`custom_single`,`custom_single_flatten`,`otlp_log_v1`。默认值：`custom_single` |
| Convert.Encoding                  | String   | 否    | ilogtail flusher数据转换编码，可选值：`json`、`none`、`protobuf`，默认值：`json`                                                     |
| Convert.TagFieldsRename           | Map      | 否    | 对日志中tags中的json字段重命名                                                                                                |
| Convert.ProtocolFieldsRename      | Map      | 否    | ilogtail日志协议字段重命名，可当前可重命名的字段：`contents`,`tags`和`time`                                                              |
| Index                             | String   | 是    | 插入数据目标索引                                                                                                           |          |      |                                                                                    |
| Authentication                    | Struct   | 是    | ElasticSearch 连接访问认证配置                                                                                             |
| Authentication.PlainText.Username | String   | 是    | ElasticSearch 用户名                                                                                                  |
| Authentication.PlainText.Password | String   | 是    | ElasticSearch 密码                                                                                                   |
| Authentication.TLS.Enabled        | Boolean  | 否    | 是否启用 TLS 安全连接,                                                                                                     |
| Authentication.TLS.CAFile         | String   | 否    | TLS CA 根证书文件路径                                                                                                     |
| Authentication.TLS.CertFile       | String   | 否    | TLS 连接证书文件路径                                                                                                       |
| Authentication.TLS.KeyFile        | String   | 否    | TLS 连接私钥文件路径                                                                                                       |
| Authentication.TLS.MinVersion     | String   | 否    | TLS 支持协议最小版本，可选配置：`1.0, 1.1, 1.2, 1.3`,默认：`1.2`                                                                    |
| Authentication.TLS.MaxVersion     | String   | 否    | TLS 支持协议最大版本,可选配置：`1.0, 1.1, 1.2, 1.3`,默认采用：`crypto/tls`支持的版本，当前`1.3`                                              |
| HTTPConfig.MaxIdleConnsPerHost    | Int      | 否    | 每个host的连接池最大空闲连接数                                                                                                  |
| HTTPConfig.ResponseHeaderTimeout  | String   | 否    | 读取头部的时间限制，可选配置`Nanosecond`，`Microsecond`，`Millisecond`，`Second`，`Minute`，`Hour`                                    |

## 样例

### 常规索引

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到 ElasticSearch。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_elasticsearch
    Addresses: 
      - http://192.XX.XX.1:9092
      - http://192.XX.XX.2:9092
    Index: default
    Authentication:
      PlainText:
        Username: user
        Password: 123456
    HTTPConfig:
        MaxIdleConnsPerHost: 10
        ResponseHeaderTimeout: Second
```

### 动态索引

采集结果支持写入`ElasticSearch`动态索引，例如，数据写入到一个名字包含当前日期的索引，可以这样写（只提供`flushers`），更多关于动态索引格式化的信息请[参考这里](../../developer-guide/format-string/format-index.md)

```yaml
flushers:
  - Type: flusher_elasticsearch
    Addresses:
      - http://192.XX.XX.1:9092
      - http://192.XX.XX.2:9092
    Index: log_%{+yyyyMMdd}
    Authentication:
      PlainText:
        Username: user
        Password: 123456
```

### 数据平铺写入

`ilogtail 1.8.0`新增数据平铺协议`custom_single_flatten`，`contents`、`tags`和`time`三个`convert`层的协议字段中数据做一级打平。
当前`convert`协议在单条数据处理仅支持`json`编码，因此`custom_single_flatten`需要配合`json`编码一起使用。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_elasticsearch
    Addresses:
      - http://192.XX.XX.1:9092
      - http://192.XX.XX.2:9092
    Convert:
      Protocol: custom_single_flatten
      Encoding: json
    Index: default
```

非平铺前写入`ElasticSearch`的数据格式

```json
{
  "contents": {
    "class": "org.springframework.web.servlet.DispatcherServlet@initServletBean:547",
    "application": "springboot-docker",
    "level": "ERROR",
    "message": "Completed initialization in 9 ms",
    "thread": "http-nio-8080-exec-10",
    "@time": "2022-07-20 16:55:05.415"
  },
  "tags": {
    "k8s.namespace.name":"java_app",
    "host.ip": "192.168.6.128",
    "host.name": "master",
    "log.file.path": "/data/test.log"
  },
  "time": 1664435098
}
```

使用平铺协议后`custom_single_flatten`，`json`全部被一级平铺。

```json
{
    "class": "org.springframework.web.servlet.DispatcherServlet@initServletBean:547",
    "application": "springboot-docker",
    "level": "ERROR",
    "message": "Completed initialization in 9 ms",
    "thread": "http-nio-8080-exec-10",
    "@time": "2022-07-20 16:55:05.415",
    "k8s.namespace.name":"java_app",
    "host.ip": "192.168.6.128",
    "host.name": "master",
    "log.file.path": "/data/test.log",
    "time": 1664435098
}
```
