# Kafka

## 简介

`flusher_kafka` `flusher`插件可以实现将采集到的数据，经过处理后，发送到Kafka。

## 版本

[Deprecated](../stability-level.md)，请使用`flusher_kafka_v2`

## 配置参数

| 参数              | 类型       | 是否必选 | 说明                                                          |
| --------------- | -------- | ---- | ----------------------------------------------------------- |
| Type            | String   | 是    | 插件类型                                                        |
| Brokers         | String数组 | 是    | Kafka Brokers                                               |
| Topic           | String   | 是    | Kafka Topic                                                 |
| SASLUsername    | String   | 否    | SASL用户名                                                     |
| SASLPassword    | String   | 否    | SASL密码                                                      |
| PartitionerType | String   | 否    | Partitioner类型。取值：`roundrobin`、`hash`、`random`。默认为：`random`。 |
| HashKeys        | String数组 | 否    | PartitionerType为`hash`时，需指定HashKeys。                        |
| HashOnce        | Boolean  | 否    |                                                             |
| ClientID        | String   | 否    | 写入Kafka的Client ID，默认取值：`LogtailPlugin`。                     |

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到Kafka。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_kafka
    Brokers: 
      - 192.XX.XX.1:9092
      - 192.XX.XX.2:9092
    Topic: KafkaTestTopic
```
