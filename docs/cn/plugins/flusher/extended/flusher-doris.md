# Doris

## 简介

`flusher_doris` `flusher`插件可以实现将采集到的数据，经过处理后，通过 Stream Load API 发送到 Apache Doris。该插件支持并发刷新、Group Commit、进度监控等特性，适用于高吞吐量数据写入场景。

## 版本

[Alpha](../../stability-level.md)

## 版本说明


* Apache Doris：v2.0 及以上

## 配置参数

| 参数                                | 类型       | 是否必选 | 说明                                                                                                                                                                                      |
|-----------------------------------|----------|------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Type                              | string   | 是    | 插件类型，固定为`flusher_doris`                                                                                                                                                                 |
| Addresses                         | String数组 | 是    | Doris FE (Frontend) 地址列表，格式为 "http://host:port" 或 "https://host:port"，必须包含协议前缀                                                                                                          |
| Database                          | String   | 是    | 目标 Doris 数据库名称                                                                                                                                                                          |
| Table                             | String   | 是    | 目标 Doris 表名称                                                                                                                                                                            |
| Authentication                    | Struct   | 是    | Doris 连接访问认证配置                                                                                                                                                                          |
| Authentication.PlainText.Username | String   | 是    | Doris 用户名                                                                                                                                                                               |
| Authentication.PlainText.Password | String   | 是    | Doris 密码                                                                                                                                                                                |
| Authentication.PlainText.Database | String   | 否    | 数据库名称（可覆盖顶层 Database 参数）                                                                                                                                                                |
| Convert                           | Struct   | 否    | ilogtail数据转换协议配置                                                                                                                                                                        |
| Convert.Protocol                  | String   | 否    | ilogtail数据转换协议，可选值：`custom_single`、`otlp_log_v1`。默认值：`custom_single`                                                                                                                    |
| Convert.Encoding                  | String   | 否    | ilogtail flusher数据转换编码，可选值：`json`、`none`、`protobuf`，默认值：`json`                                                                                                                          |
| Convert.TagFieldsRename           | Map      | 否    | 对日志中tags中的json字段重命名                                                                                                                                                                     |
| Convert.ProtocolFieldsRename      | Map      | 否    | ilogtail日志协议字段重命名，可重命名的字段：`contents`、`tags`和`time`                                                                                                                                      |
| LoadProperties                    | Map      | 否    | 额外的 Stream Load 属性（如 `strict_mode`、`max_filter_ratio`、`timeout` 等），将设置在 HTTP 请求头中，参考 [Doris Stream Load 文档](https://doris.apache.org/zh-CN/docs/data-operate/import/stream-load-manual) |
| LogProgressInterval               | Int      | 否    | 进度日志输出间隔（秒），周期性输出总数据量、总行数、加载速度等统计信息，默认值：10，设置为 0 可禁用                                                                                                                                    |
| GroupCommit                       | String   | 否    | Group Commit 模式，用于优化小批量加载。可选值：`off`（禁用，每次立即提交）、`sync`（同步提交，等待确认）、`async`（异步提交，立即返回）。默认值：`off`                                                                                           |
| Concurrency                       | Int      | 否    | 并发刷新的 goroutine 数量。设置为 1 时为同步模式（顺序刷新），大于 1 时为并发模式（多个 worker 并发刷新，显著提升吞吐量）。默认值：1                                                                                                         |
| QueueCapacity                     | Int      | 否    | 并发模式下的任务队列容量。队列满时会阻塞以确保不丢失数据。建议设置为 Concurrency 的 2-4 倍。默认值：1024                                                                                                                         |

## 样例

### 基础配置

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到 Doris。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_doris
    Addresses: 
      - http://192.168.1.1:8030
      - http://192.168.1.2:8030
    Database: example_db
    Table: example_table
    Authentication:
      PlainText:
        Username: root
        Password: password
```

### 配合 Aggregator 攒批

通过配置 `aggregator` 插件，可以在发送到 flusher 之前对日志进行攒批，减少网络请求次数，提升整体性能。推荐配合 `aggregator_base` 或 `aggregator_context` 使用。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
aggregators:
  - Type: aggregator_base
    MaxLogCount: 10000        # 每个 LogGroup 最多 1万条日志
    MaxLogGroupCount: 4       # Aggregator 最多缓存 4 个 LogGroup
flushers:
  - Type: flusher_doris
    Addresses: 
      - http://192.168.1.1:8030
    Database: example_db
    Table: example_table
    Authentication:
      PlainText:
        Username: root
        Password: password
```

### 高性能并发配置

对于高吞吐量场景，同时配置 aggregator 攒批和 flusher 并发刷新，可获得最佳性能：

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
aggregators:
  - Type: aggregator_base
    MaxLogCount: 20000        
    MaxLogGroupCount: 10      
flushers:
  - Type: flusher_doris
    Addresses: 
      - http://192.168.1.1:8030
      - http://192.168.1.2:8030
    Database: example_db
    Table: example_table
    Authentication:
      PlainText:
        Username: root
        Password: password
    # 启用并发刷新
    Concurrency: 16
    QueueCapacity: 2048
    # 启用异步 Group Commit
    GroupCommit: async
    # 每 5 秒输出一次进度日志
    LogProgressInterval: 5
```

### 自定义 Stream Load 属性

通过 `LoadProperties` 自定义 Doris Stream Load 行为：

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_doris
    Addresses: 
      - http://192.168.1.1:8030
    Database: example_db
    Table: example_table
    Authentication:
      PlainText:
        Username: root
        Password: password
    LoadProperties:
      strict_mode: "false"       # 非严格模式
      max_filter_ratio: "0.1"    # 最大过滤比例 10%
      timeout: "600"             # 超时时间 600 秒
```
