# Kafka


## 配置参数

| 参数 | 类型 | 是否必选 | 说明 |
| :--- | :--- | :--- | :--- |
| `Brokers` | String数组 | 是 | Kafka 集群的连接地址列表。例如：`["host1:9092", "host2:9092"]`。 |
| `Topic` | String | 是 | 消息默认发送到的 Topic 名称。 |
| `Producer.QueueBufferingMaxKbytes` | Int | 否 | 生产者本地消息队列的最大总容量（单位：KB）。默认值：`1048576` (1GB)。 |
| `Producer.QueueBufferingMaxMessages` | Int | 否 | 生产者本地消息队列允许缓存的最大消息数量。默认值：`100000`。 |
| `Producer.LingerMs` | Int | 否 | 发送消息前在队列中累积的最大等待时间（单位：毫秒）。增加此值可提高吞吐量，但会增加延迟。默认值：`5`。 |
| `Producer.BatchNumMessages` | Int | 否 | 单个批次中允许包含的最大消息数量。默认值：`10000`。 |
| `Producer.BatchSize` | Int | 否 | 单个批次的最大字节数限制。默认值：`1000000` (1MB)。 |
| `Producer.MaxMessageBytes` | Int | 否 | 允许插件发送的单条消息的最大字节数。默认值：`1000000` (1MB)。 |
| `Delivery.Acks` | Int | 否 | 消息发送的确认级别。`0`: 不等待响应；`1`: 等待 Leader 确认；`-1` (或 `all`): 等待所有 ISR 副本确认。默认值：`-1`。 |
| `Delivery.RequestTimeoutMs` | Int | 否 | 等待 Kafka Broker 响应 ACK 的超时时间（单位：毫秒）。默认值：`30000` (30秒)。 |
| `Delivery.MessageTimeoutMs` | Int | 否 | 消息在本地发送（包括重试）的最大总时间（单位：毫秒）。若超时，则消息发送失败。`0` 表示无限等待。默认值：`300000` (5分钟)。 |
| `Delivery.MaxRetries` | Int | 否 | 消息发送失败时的最大重试次数。默认值：`2147483647`。 |
| `Delivery.RetryBackoffMs` | Int | 否 | 每次重试间的最大退避等待时间（单位：毫秒）。默认值：`1000` (1秒)。 |