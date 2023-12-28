# ClickHouse

## 简介

`flusher_clickhouse` `flusher`插件可以实现将采集到的数据，经过处理后，发送到 ClickHouse，需要 ClickHouse 版本至少为 `22.3`。

## 版本

[Alpha](../stability-level.md)

## 配置参数

| 参数                                | 类型       | 是否必选 | 说明                                                                                 |
|-----------------------------------|----------|------|------------------------------------------------------------------------------------|
| Addresses                         | String数组 | 是    | ClickHouse 地址                                                                      |
| Convert                           | Struct   | 否    | ilogtail数据转换协议配置                                                                   |
| Convert.Protocol                  | String   | 否    | ilogtail数据转换协议，kafka flusher 可选值：`custom_single`,`otlp_log_v1`。默认值：`custom_single` |
| Convert.Encoding                  | String   | 否    | ilogtail flusher数据转换编码，可选值：`json`、`none`、`protobuf`，默认值：`json`                     |
| Convert.TagFieldsRename           | Map      | 否    | 对日志中tags中的json字段重命名                                                                |
| Convert.ProtocolFieldsRename      | Map      | 否    | ilogtail日志协议字段重命名，可当前可重命名的字段：`contents`,`tags`和`time`                              |
| Authentication                    | Struct   | 是    | Clickhouse 连接访问认证配置                                                                |
| Authentication.PlainText.Username | String   | 否    | ClickHouse 用户名                                                                     |
| Authentication.PlainText.Password | String   | 否    | ClickHouse 密码                                                                      |
| Authentication.PlainText.Database | String   | 是    | 插入数据目标数据库名称                                                                        |
| Authentication.TLS.Enabled        | Boolean  | 否    | 是否启用 TLS 安全连接,                                                                     |
| Authentication.TLS.CAFile         | String   | 否    | TLS CA 根证书文件路径                                                                     |
| Authentication.TLS.CertFile       | String   | 否    | TLS 连接证书文件路径                                                                       |
| Authentication.TLS.KeyFile        | String   | 否    | TLS 连接私钥文件路径                                                                       |
| Authentication.TLS.MinVersion     | String   | 否    | TLS 支持协议最小版本，可选配置：`1.0, 1.1, 1.2, 1.3`,默认：`1.2`                                    |
| Authentication.TLS.MaxVersion     | String   | 否    | TLS 支持协议最大版本,可选配置：`1.0, 1.1, 1.2, 1.3`,默认采用：`crypto/tls`支持的版本，当前`1.3`              |
| Cluster                           | String   | 否    | 数据库对应集群名称                                                                          |
| Table                             | String   | 是    | 插入数据目标 null engine 数据表名称                                                           |
| MaxExecutionTime                  | Int      | 否    | 单次请求最长执行时间，默认 60 秒                                                                 |
| DialTimeout                       | String   | 否    | Dial 超时时间，默认 10 秒                                                                  |
| MaxOpenConns                      | Int      | 否    | 最大连接数，默认 5                                                                         |
| MaxIdleConns                      | Int      | 否    | 连接池连接数，默认 5                                                                        |
| ConnMaxLifetime                   | String   | 否    | 连接维持最大时长，默认 10 分钟                                                                  |
| BufferNumLayers                   | Int      | 否    | Buffer 缓冲区数量，默认 16                                                                 |
| BufferMinTime                     | Int      | 否    | 缓冲区数据刷新限制条件 min_time，默认 10                                                         |
| BufferMaxTime                     | Int      | 否    | 缓冲区数据刷新限制条件 max_time，默认 100                                                        |
| BufferMinRows                     | Int      | 否    | 缓冲区数据刷新限制条件 min_rows，默认 10000                                                      |
| BufferMaxRows                     | Int      | 否    | 缓冲区数据刷新限制条件 max_rows，默认 1000000                                                    |
| BufferMinBytes                    | Int      | 否    | 缓冲区数据刷新限制条件 min_bytes，默认 10000000                                                  |
| BufferMaxBytes                    | Int      | 否    | 缓冲区数据刷新限制条件 max_bytes，默认 100000000                                                 |
| Compression                       | String   | 否    | 压缩方式，默认 lz4，可选 none/gzip/deflate/lz4/br/zstd                                       |

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到 ClickHouse。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_clickhouse
    Addresses: 
      - 192.XX.XX.1:9092
      - 192.XX.XX.2:9092
    Authentication:
      PlainText:
        Database: default
        Username: user
        Password: 123456
    Table: demo
```
