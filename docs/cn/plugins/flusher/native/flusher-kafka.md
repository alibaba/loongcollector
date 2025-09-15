# Kafka

## 版本

[Alpha](../../stability-level.md)

## 配置文件

| 参数 | 类型 | 是否必选 | 说明 |
| :--- | :--- | :--- | :--- |
| `Brokers` | String数组 | 是 | Kafka 集群的连接地址列表。例如：`["host1:9092", "host2:9092"]`。 |
| `Topic` | String | 是 | 消息默认发送到的 Topic 名称。支持动态 Topic 同 v2（仅字符串替换）。 |
| `Version` | String | 否 | Kafka 协议版本号，如：`"0.10.2.1"`、`"2.6.0"`、`"3.6.0"`。默认：`"1.0.0"`。用于推导底层 librdkafka 兼容参数。 |
| `BulkFlushFrequency` | Int | 否 | 批次发送等待时间（毫秒），映射 `linger.ms`，默认：`0`。 |
| `BulkMaxSize` | Int | 否 | 单批最大消息数，映射 `batch.num.messages`，默认：`2048`。 |
| `MaxMessageBytes` | Int | 否 | 单条消息最大字节数，映射 `message.max.bytes`，默认：`1000000`。 |
| `QueueBufferingMaxKbytes` | Int | 否 | 本地队列总容量（KB），映射 `queue.buffering.max.kbytes`，默认：`1048576`。 |
| `QueueBufferingMaxMessages` | Int | 否 | 本地队列最大消息数，映射 `queue.buffering.max.messages`，默认：`100000`。 |
| `RequiredAcks` | Int | 否 | 确认级别：`0`/`1`/`-1`（-1 等价于 `all`），映射 `acks`，默认：`1`。 |
| `Timeout` | Int | 否 | 请求超时（毫秒），映射 `request.timeout.ms`，默认：`30000`。 |
| `MessageTimeoutMs` | Int | 否 | 消息发送（含重试）超时（毫秒），映射 `message.timeout.ms`，默认：`300000`。 |
| `MaxRetries` | Int | 否 | 失败重试次数，映射 `message.send.max.retries`，默认：`3`。 |
| `RetryBackoffMs` | Int | 否 | 重试退避（毫秒），映射 `retry.backoff.ms`，默认：`100`。 |

## 样例配置

```yaml
enable: true
global:
  UsingOldContentTag: true
  DefaultLogQueueSize: 10
inputs:
  - Type: input_file
    FilePaths:
      - "/root/test/**/flusher_test*.log"
    MaxDirSearchDepth: 10
    TailingAllMatchedFiles: true
flushers:
  - Type: flusher_kafka_cpp
    Brokers: ["kafka:29092"]
    Topic: "test-topic-3x"
    Version: "3.6.0"
    MaxMessageBytes: 5242880
    MaxRetries: 2
```
