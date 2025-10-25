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
| `Headers` | header数组 | 否 | / | Kafka 消息头，静态键值对数组，`value` 仅支持字符串 |
| `PartitionerType` | String | 否 | 分区策略：`random` 或 `hash`。默认 `random`。当为 `hash` 时，会基于指定的 `HashKeys` 生成消息键（Key），并使用 `murmur2_random` 作为底层分区器。 |
| `HashKeys` | String数组 | 否 | 参与分区键生成的字段（仅对 `LOG` 事件生效）。每项必须以 `content.` 前缀开头，如：`["content.service", "content.user"]`。当 `PartitionerType` = `hash` 时必填。 |
| `Authentication.TLS.Enabled` | bool | 否 | false | 启用 SSL 连接，对应 `security.protocol=ssl` |
| `Authentication.TLS.CAFile` | string | 否 | / | CA 证书路径，映射 `ssl.ca.location` |
| `Authentication.TLS.CertFile` | string | 否 | / | 客户端证书路径，映射 `ssl.certificate.location`（与 KeyFile 必须成对配置，否则将视为配置错误） |
| `Authentication.TLS.KeyFile` | string | 否 | / | 客户端私钥路径，映射 `ssl.key.location`（与 CertFile 必须成对配置，否则将视为配置错误） |
| `Authentication.TLS.KeyPassword` | string | 否 | / | 私钥口令，映射 `ssl.key.password`（可选） |

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
    Headers:
      - key: "h1"
        value: "v1"
      - key: "h2"
        value: "v2"
    Kafka:
      compression.type: lz4
```

## 动态 Topic

`Topic` 支持动态格式化，按事件内容或分组标签动态路由到不同的 Kafka Topic。支持的占位符：

- `%{content.key}`: 取日志内容中的字段值（仅对 `LOG` 类型事件生效）。
- `%{tag.key}`: 取分组标签（`GroupTags`）中的键值。
- `${ENV_NAME}`: 取分组标签中名为 `ENV_NAME` 的值（通常由上游处理器/输入端注入）。

示例：根据日志中的 `service` 字段动态路由到不同 Topic：

```yaml
flushers:
  - Type: flusher_kafka_native
    Brokers: ["kafka:29092"]
    Topic: "app-%{content.service}"
    KafkaVersion: "3.6.0"
```

示例：根据标签 `env` 和日志字段 `service` 组合路由：

```yaml
flushers:
  - Type: flusher_kafka_native
    Brokers: ["kafka:29092"]
    Topic: "${env}-%{content.service}"
    KafkaVersion: "3.6.0"
```

当动态格式化失败（字段缺失等）时，将回退到原始 `Topic` 模板字符串对应的静态值，并记录错误日志。

## 分区策略

当需要将相同业务键的日志落到同一分区时，可以开启 `hash` 分区：

- `PartitionerType: "hash"`：启用哈希分区，内部映射为 librdkafka `partitioner=murmur2_random`，与 Java 客户端默认分区器兼容（NULL Key 随机分配）。
- `HashKeys`：从日志内容中取值拼接成消息 Key，示例：

```yaml
flushers:
  - Type: flusher_kafka_native
    Brokers: ["kafka:29092"]
    Topic: "hash-topic"
    Version: "2.8.0"
    PartitionerType: "hash"
    HashKeys: ["content.service", "content.user"]
```

## TLS配置

支持通过 TLS/SSL 安全连接到 Kafka 集群。

```yaml
flushers:
  - Type: flusher_kafka_native
    Brokers: ["kafka:29093"]
    Topic: "tls-topic"
    Version: "2.8.0"
    Headers:
      - key: "env"
        value: "prod"
    Authentication:
      TLS:
        Enabled: true
        CAFile: "/etc/kafka/ssl/ca.crt"
        # 可选：若需要客户端证书/私钥
        # CertFile: "/etc/kafka/ssl/client.crt"
        # KeyFile: "/etc/kafka/ssl/client.key"
        # KeyPassword: "***"
```

## Headers

- 形态：数组，每个元素包含 `key` 与 `value` 两个字段，均为字符串；
- 特性：静态配置，发送时原样附加到每条 Kafka 消息的 Header 中；

示例：

```yaml
Headers:
  - key: "trace_id"
    value: "fixed-trace"
  - key: "source"
    value: "loongcollector"
```