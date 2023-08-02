# ElasticSearch

## 简介

`flusher_elasticsearch` `flusher`插件可以实现将采集到的数据，经过处理后，发送到 ElasticSearch，需要 ElasticSearch 版本至少为 `8`。

## 版本

[Alpha](../stability-level.md)

## 配置参数

| 参数                                | 类型       | 是否必选 | 说明                                                                                 |
|-----------------------------------|----------|------|------------------------------------------------------------------------------------|
| Type                              | String   | 是    | 插件类型，固定为`flusher_elasticsearch`                                                    |
| Addresses                         | String数组 | 是    | ElasticSearch 地址                                                                   |
| Convert                           | Struct   | 否    | ilogtail数据转换协议配置                                                                   |
| Convert.Protocol                  | String   | 否    | ilogtail数据转换协议，kafka flusher 可选值：`custom_single`,`otlp_log_v1`。默认值：`custom_single` |
| Convert.Encoding                  | String   | 否    | ilogtail flusher数据转换编码，可选值：`json`、`none`、`protobuf`，默认值：`json`                     |
| Convert.TagFieldsRename           | Map      | 否    | 对日志中tags中的json字段重命名                                                                |
| Convert.ProtocolFieldsRename      | Map      | 否    | ilogtail日志协议字段重命名，可当前可重命名的字段：`contents`,`tags`和`time`                              |
| Index                             | String   | 是    | 插入数据目标索引                                                                           |          |      |                                                                                    |
| Authentication                    | Struct   | 是    | ElasticSearch 连接访问认证配置                                                             |
| Authentication.PlainText.Username | String   | 是    | ElasticSearch 用户名                                                                  |
| Authentication.PlainText.Password | String   | 是    | ElasticSearch 密码                                                                   |
| Authentication.TLS.Enabled        | Boolean  | 否    | 是否启用 TLS 安全连接,                                                                     |
| Authentication.TLS.CAFile         | String   | 否    | TLS CA 根证书文件路径                                                                     |
| Authentication.TLS.CertFile       | String   | 否    | TLS 连接证书文件路径                                                                       |
| Authentication.TLS.KeyFile        | String   | 否    | TLS 连接私钥文件路径                                                                       |
| Authentication.TLS.MinVersion     | String   | 否    | TLS 支持协议最小版本，可选配置：`1.0, 1.1, 1.2, 1.3`,默认：`1.2`                                    |
| Authentication.TLS.MaxVersion     | String   | 否    | TLS 支持协议最大版本,可选配置：`1.0, 1.1, 1.2, 1.3`,默认采用：`crypto/tls`支持的版本，当前`1.3`              |
| HTTPConfig.MaxIdleConnsPerHost    | Int      | 否    | 每个host的连接池最大空闲连接数                                                                  |
| HTTPConfig.ResponseHeaderTimeout  | String   | 否    | 读取头部的时间限制，可选配置`Nanosecond`，`Microsecond`，`Millisecond`，`Second`，`Minute`，`Hour`    |
 

## 样例

### 常规索引

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到 ElasticSearch。

```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test_log
    FilePattern: "*.log"
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