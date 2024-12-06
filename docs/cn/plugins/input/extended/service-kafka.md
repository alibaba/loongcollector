# Kafka

## 简介

`service_kafka` `input`插件实现了`ServiceInputV1`和`ServiceInputV2`接口，插件用于采集Kafka的消息。

## 版本

[Stable](../../stability-level.md)

## 配置参数

| 参数                        | 类型      | 是否必选 | 说明                                                                                                                                          |
|---------------------------|---------|------|---------------------------------------------------------------------------------------------------------------------------------------------|
| Type                      | String  | 是    | 插件类型，指定为`service_kafka`。                                                                                                                    |
| Format                    | String  | 否    | ilogtail 1.6.0新增，仅ServiceInputV2支持。v2版本支持格式:`raw`, `prometheus`,  `otlp_metricv1`、`otlp_tracev1`</p><p>说明：`raw`格式以原始请求字节流传输数据，默认值：`raw`</p> |
| Version                   | String  | 是    | Kafka集群版本号。                                                                                                                                 |
| Brokers                   | Array   | 是    | Kafka服务器地址列表。                                                                                                                               |
| ConsumerGroup             | String  | 是    | Kafka消费组名称。                                                                                                                                 |
| Topics                    | Array   | 是    | 待消费的Kafka订阅主题列表。                                                                                                                            |
| ClientID                  | String  | 是    | 消费Kafka的用户ID。                                                                                                                               |
| Offset                    | String  | 否    | Kafka初始消费位移类型，可选值包括：oldest和newest。如果未添加该参数，则默认使用oldest，表示从最早可用的位移处开始消费。                                                                     |
| MaxMessageLen（Deprecated） | Integer | 否    | Kafka消息的最大允许长度，单位为字节，取值范围为：1～524288。如果未添加该参数，则默认使用524288，即512KB。 ilogtail 1.6.0不再使用此参数                                                      |
| SASLUsername              | String  | 否    | SASL用户名。                                                                                                                                    |
| SASLPassword              | String  | 否    | SASL密码。                                                                                                                                     |                                                               |
| Assignor                  | String  | 否    | 消费组消费分区分配策略。可以设置选项：range, roundrobin, sticky，默认值：range                                                                                      |
| DisableUncompress         | Boolean | 否    | ilogtail 1.6.0新增，禁用对于请求数据的解压缩, 默认取值为:`false`<p>目前仅针对Raw Format有效</p><p>仅v2版本有效</p>                                                          |
| FieldsExtend              | Boolean | 否    | <p>是否支持非integer以外的数据类型(如String)</p><p>目前仅针对有 String、Bool 等额外类型的 influxdb Format 有效，仅v2版本有效</p>                                              |

## 样例

采集服务器地址为172.xx.xx.48和172.xx.xx.34、主题为topicA和topicB的Kafka消息，并将采集结果输出至标准输出，其中Kafka集群的版本为2.1.1，消费组的名称为test-group，其余取默认值。

* 输入

```json
{"payload":"foo"}
```

### 采集配置（v1）

```yaml
enable: true
inputs:
  - Type: service_kafka
    Version: 2.1.1
    Brokers: 
        - 172.xx.xx.48
        - 172.xx.xx.34
    ConsumerGroup: test-group
    Topics:
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

### 采集配置（v2）

`v2`是`ilogtail 1.6.0`新增的实现，主要是支持配置`Format`指定一些特定的数据格式化方式`raw`, `prometheus`,  `otlp_metricv1`、`otlp_tracev1`，
其它配置变更可查看【配置参数】表。如果没有特殊的需求，使用默认的`v1`即可。

```yaml
enable: true
version: v2
inputs:
  - Type: service_kafka
    Version: 2.1.1
    Brokers: 
        - 172.xx.xx.48
        - 172.xx.xx.34
    ConsumerGroup: test-group
    Topics:
        - topicA
        - topicB
    ClientID: sls
flushers:
  - Type: flusher_stdout
    OnlyStdout: true  
```

* 输出

```json
{"eventType":"byteArray","name":"","timestamp":0,"observedTimestamp":0,"tags":{},"byteArray":"{\"payload \": \"foo \"}"}
```
