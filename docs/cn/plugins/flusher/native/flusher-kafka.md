# Kafka

## 简介

`flusher_kafka_native` 将事件序列化为 JSON 文本后异步投递到 Kafka，底层基于 librdkafka。

## 版本

[Alpha](../../stability-level.md)

## 配置参数

| 参数 | 类型 | 是否必选 | 默认值 | 说明 |
| :--- | :--- | :--- | :--- | :--- |
| `Type` | string | 是 | / | 固定为 `flusher_kafka_native` |
| `Brokers` | []string | 是 | / | Kafka 集群地址列表，如 `["host1:9092", "host2:9092"]` |
| `Topic` | string | 是 | / | 发送的目标 Topic 名称。支持动态 Topic 同 kafka_flusher_v2 扩展插件（仅字符串替换）。 |
| `Version` | string | 否 | `"1.0.0"` | Kafka 协议版本，格式 `x.y.z[.n]`，用于推导底层 librdkafka 兼容参数。 |
| `BulkFlushFrequency` | uint | 否 | `0` | 批量发送等待时长（毫秒），映射 `linger.ms` |
| `BulkMaxSize` | uint | 否 | `2048` | 单批最大消息数，映射 `batch.num.messages` |
| `MaxMessageBytes` | uint | 否 | `1000000` | 单条消息最大字节数，映射 `message.max.bytes` |
| `QueueBufferingMaxKbytes` | uint | 否 | `1048576` | 本地队列总容量（KB），映射 `queue.buffering.max.kbytes` |
| `QueueBufferingMaxMessages` | uint | 否 | `100000` | 本地队列最大消息数，映射 `queue.buffering.max.messages` |
| `RequiredAcks` | int | 否 | `1` | 确认级别：`0`/`1`/`-1(all)`，映射 `acks` |
| `Timeout` | uint | 否 | `30000` | 请求超时（毫秒），映射 `request.timeout.ms` |
| `MessageTimeoutMs` | uint | 否 | `300000` | 消息发送（含重试）超时（毫秒），映射 `message.timeout.ms` |
| `MaxRetries` | uint | 否 | `3` | 失败重试次数，映射 `message.send.max.retries` |
| `RetryBackoffMs` | uint | 否 | `100` | 重试退避（毫秒），映射 `retry.backoff.ms` |
| `Kafka` | map[string]string | 否 | / | 透传自定义 librdkafka 配置，如 `{ "compression.type": "lz4" }` |

## 样例

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths:
      - "/root/test/**/flusher_test*.log"
flushers:
  - Type: flusher_kafka_native
    Brokers: ["kafka:29092"]
    Topic: "test-topic"
    Version: "3.6.0"
    MaxMessageBytes: 5242880
    MaxRetries: 2
    Kafka:
      compression.type: lz4
```
