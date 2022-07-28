# Kafka

## 简介

`service_kafka` `input`插件可以采集Kafka的消息。

## 配置参数

| 参数 | 类型 | 是否必选 | 说明 |
| --- | --- | --- | --- |
| Type | String | 是 | 插件类型，指定为`service_kafka`。 |
| Version | String | 是 | Kafka集群版本号。 |
| Brokers | Array | 是 | Kafka服务器地址列表。 |
| ConsumerGroup | String | 是 | Kafka消费组名称。 |
| Topic | Array | 是 | 待消费的Kafka订阅主题列表。 |
| ClientID | String | 是 | 消费Kafka的用户ID。 |
| Offset | String | 否 | Kafka初始消费位移类型，可选值包括：oldest和newest。如果未添加该参数，则默认使用oldest，表示从最早可用的位移处开始消费。 |
| MaxMessageLen | Integer | 否 | Kafka消息的最大允许长度，单位为字节，取值范围为：1～524288。如果未添加该参数，则默认使用524288，即512KB。 |
| SASLUsername | String | 否 | SASL用户名。 |
| SASLPassword | String | 否 | SASL密码。 |

## 样例

采集服务器地址为172.xx.xx.48和172.xx.xx.34、主题为topicA和topicB的Kafka消息，并将采集结果输出至标准输出，其中Kafka集群的版本为2.1.1，消费组的名称为test-group，其余取默认值。

* 输入

```json
{"payload":"foo"}
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: service_kafka
    Version: 2.1.1
    Brokers: 
        - 172.xx.xx.48
        - 172.xx.xx.34
    ConsumerGroup: test-group
    Topic:
        - topicA
        - topicB
    ClientID: sls
flushers:
  - Type: flusher_stdout
    OnlyStdout: true  
```

* 输出

```json
{"payload":"foo"}
```
