# Kafka

## 简介

`flusher_kafka_v2` `flusher`插件可以实现将采集到的数据，经过处理后，发送到Kafka。

## 配置参数

| 参数                                    | 类型       | 是否必选 | 说明                                                                                                 |
|---------------------------------------|----------|------|----------------------------------------------------------------------------------------------------|
| Type                                  | String   | 是    | 插件类型                                                                                               |
| Brokers                               | String数组 | 是    | Kafka Brokers                                                                                      |
| Topic                                 | String   | 是    | Kafka Topic,支持动态topic, 例如: `test_%{content.appname}`                                               |
| Version                               | String   | 否    | Kafka协议版本号 ,例如：`2.0.0`，默认值：`1.0.0`                                                                 |
| Convert                               | Struct   | 否    | ilogtail数据转换协议配置                                                                                   |
| Convert.Protocol                      | String   | 否    | ilogtail数据转换协议，kafka flusher 可选值：`custom_single`,`otlp_log_v1`。默认值：`custom_single`                 |
| Convert.Encoding                      | String   | 否    | ilogtail flusher数据转换编码，可选值：`json`、`none`、`protobuf`，默认值：`json`                                     |
| Convert.TagFieldsRename               | Map      | 否    | 对日志中tags中的json字段重命名                                                                                |
| Convert.ProtocolFieldsRename          | Map      | 否    | ilogtail日志协议字段重命名，可当前可重命名的字段：`contents`,`tags`和`time`                                              |
| Authentication                        | Struct   | 否    | Kafka连接访问认证配置，支持`SASL/PLAIN`，根据kafka服务端认证方式选择配置                                                    |
| Authentication.PlainText.Username     | String   | 否    | PlainText认证用户名                                                                                     |
| Authentication.PlainText.Password     | String   | 否    | PlainText认证密码                                                                                      |
| Authentication.SASL.Username          | String   | 否    | SASL认证用户名                                                                                          |
| Authentication.SASL.Password          | String   | 否    | SASL认证密码                                                                                           |
| Authentication.Sasl.SaslMechanism     | String   | 否    | SASL认证，配置可选项：`PLAIN`、`SCRAM-SHA-256`、`SCRAM-SHA-512`                                               |
| Authentication.TLS.Enabled            | Boolean  | 否    | 是否启用TLS安全连接,                                                                                       |
| Authentication.TLS.CAFile             | String   | 否    | TLS CA根证书文件路径                                                                                      |
| Authentication.TLS.CertFile           | String   | 否    | TLS连接`kafka`证书文件路径                                                                                 |
| Authentication.TLS.KeyFile            | String   | 否    | TLS连接`kafka`私钥文件路径                                                                                 |
| Authentication.TLS.MinVersion         | String   | 否    | TLS支持协议最小版本，可选配置：`1.0, 1.1, 1.2, 1.3`,默认：`1.2`                                                     |
| Authentication.TLS.MaxVersion         | String   | 否    | TLS支持协议最大版本,可选配置：`1.0, 1.1, 1.2, 1.3`,默认采用：`crypto/tls`支持的版本，当前`1.3`                               |
| Authentication.TLS.InsecureSkipVerify | Boolean  | 否    | 是否跳过TLS证书校验                                                                                        |
| Authentication.Kerberos.ServiceName   | String   | 否    | 服务名称，例如：kafka                                                                                      |
| Authentication.Kerberos.UseKeyTab     | Boolean  | 否    | 是否采用keytab，配置此项后需要配置KeyTabPath，默认为：`false`                                                         |
| Authentication.Kerberos.Username      | Boolean  | 否    | UseKeyTab设置为`false`的情况下，需要指定用户名                                                                    |
| Authentication.Kerberos.Password      | String   | 否    | UseKeyTab设置为`false`的情况下，需要指定密码                                                                     |
| Authentication.Kerberos.Realm         | String   | 否    | kerberos认证管理域,大小写敏感                                                                                |
| Authentication.Kerberos.ConfigPath    | Boolean  | 否    | Kerberos krb5.conf                                                                                 |
| Authentication.Kerberos.KeyTabPath    | String   | 否    | keytab的路径                                                                                          |
| PartitionerType                       | String   | 否    | Partitioner类型。取值：`roundrobin`、`hash`、`random`。默认为：`random`。                                        |
| QequiredAcks                          | int      | 否    | ACK的可靠等级.0=无响应,1=等待本地消息,-1=等待所有副本提交.默认1，                                                           |
| Compression                           | String   | 否    | 压缩算法，可选值：`none`, `snappy`，`lz4`和`gzip`，默认值`none`                                                   |
| CompressionLevel                      | Int      | 否    | 压缩级别，可选值：`1~9`，默认值：`4`,设置为`0`则禁用`Compression`                                                      |
| MaxMessageBytes                       | Int      | 否    | 一个批次提交的大小限制，配置和`message.max.bytes`对应，默认值：`1000000`                                                 |
| MaxRetries                            | Int      | 否    | 提交失败重试次数，最大`3`次，默认值：`3`                                                                            |
| BulkMaxSize                           | Int      | 否    | 单次请求提交事件数，默认`2048`                                                                                 |
| BulkFlushFrequency                    | Int      | 否    | 发送批量 Kafka 请求之前等待的时间,0标识没有时延，默认值:`0`                                                               |
| Timeout                               | Int      | 否    | 等待Kafka brokers响应的超时时间，默认`30s`                                                                     |
| BrokerTimeout                         | int      | 否    | kafka broker等待请求的最大时长，默认`10s`                                                                      |
| Metadata.Retry.Max                    | int      | 否    | 最大重试次数，默认值：`3`                                                                                     |
| Metadata.Retry.Backoff                | int      | 否    | 在重试之前等待leader选举发生的时间，默认值：`250ms`                                                                   |
| Metadata.RefreshFrequency             | int      | 否    | Metadata刷新频率，默认值：`250ms`                                                                           |
| Metadata.Full                         | int      | 否    | 获取原数数据的策略，获取元数据时使用的策略，当此选项为`true`时，客户端将为所有可用主题维护一整套元数据，如果此选项设置为`false`，它将仅刷新已配置主题的元数据。默认值:`false`。 |
| HashKeys                              | String数组 | 否    | PartitionerType为`hash`时，需指定HashKeys。                                                               |
| HashOnce                              | Boolean  | 否    |                                                                                                    |
| ClientID                              | String   | 否    | 写入Kafka的Client ID，默认取值：`LogtailPlugin`。                                                            |

- `Version`需要填写的是`kafka protocol version`版本号，`flusher_kafka_v2`当前支持的`kafka`版本范围：`0.8.2.x~2.7.0`。
请根据自己的`kafka`版本号参照下面的`kafka protocol version`规则进行配置。**建议根据自己的`kafka`版本指定对应`protocol version`**,
`kafka protocol version`配置规则如下：
```
// x代表小版本号
0.8.2.x,0.9.0.x,0.10.0.x,0.10.1.x,0.10.2.x,0.11.0.x,1.0.0,1.1.0,1.1.1,2.0.0,2.0.1,2.1.0,2.2.0,2.3.0,2.4.0,2.5.0,2.6.0,2.7.0
```


- `Brokers`是个数组，多个`Broker`地址不能使用`;`或者`,`来隔开放在一行里，`yaml`配置文件中正确的多个`Broker`地址配置参考如下：
```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test_log
    FilePattern: "*.log"
flushers:
  - Type: flusher_kafka
    Brokers:
      - 192.XX.XX.1:9092
      - 192.XX.XX.2:9092
      - 192.XX.XX.3:9092
    Topic: KafkaTestTopic
```



## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到Kafka。

```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test_log
    FilePattern: "*.log"
flushers:
  - Type: flusher_kafka
    Brokers: 
      - 192.XX.XX.1:9092
      - 192.XX.XX.2:9092
      - 192.XX.XX.3:9092
    Topic: KafkaTestTopic
```

## 进阶配置

以下面的一段日志为例，后来将展开介绍ilogtail kafka flusher的一些高阶配置

```plain
2022-07-22 10:19:23.684 ERROR [springboot-docker] [http-nio-8080-exec-10] com.benchmark.springboot.controller.LogController : error log
```

以上面这行日志为例 , 我们通`ilogtail`的`processor_regex`插件，将上面的日志提取处理后几个关键字段：

- time
- loglevel
- appname
- thread
- class
- message

最后推送到`kafka`的数据样例如下：

```json
{
  "contents": {
    "class": "org.springframework.web.servlet.DispatcherServlet@initServletBean:547",
    "application": "springboot-docker",
    "level": "ERROR",
    "message": "Completed initialization in 9 ms",
    "thread": "http-nio-8080-exec-10",
    "time": "2022-07-20 16:55:05.415"
  },
  "tags": {
    "k8s.namespace.name":"java_app",
    "host.ip": "192.168.6.128",
    "host.name": "master",
    "log.file.path": "/data/test.log"
  },
  "time": 1664435098
}
```

### 动态topic

针对上面写入的这种日志格式，如果想根据`application`名称针对不用的应用推送到不通的`topic`，
则`topic`可以这样配置。

```yaml
Topic: test_%{content.application}
```

最后`ilogtail`就自动将日志推送到`test_springboot-docker`这个`topic`中。

`topic`动态表达式规则：

- `%{content.fieldname}`。`content`代表从`contents`中取指定字段值
- `%{tag.fieldname}`,`tag`表示从`tags`中取指定字段值，例如：`%{tag.k8s.namespace.name}`
- 其它方式暂不支持

### TagFieldsRename

例如将`tags`中的`host.name`重命名为`hostname`，配置参考如下：

```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test_log
    FilePattern: "*.log"
flushers:
  - Type: flusher_kafka_v2
    Brokers:
      - 192.XX.XX.1:9092
      - 192.XX.XX.2:9092
      - 192.XX.XX.3:9092
    Convert:
      TagFieldsRename:
        host.name: hostname
    Topic: KafkaTestTopic
```

### ProtocolFieldsRename

对`ilogtail`协议字段重命名，在`ilogtail`的数据转换协议中，
最外层三个字段`contents`,`tags`和`time`属于协议字段。`ProtocolFieldsRename`只能对
`contents`,`tags`和`time`这个三个字段进行重命名。
例如在使用`Elasticsearch`你可能想直接将`time`重命名为`@timestamp`，则配置参考如下：

```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test_log
    FilePattern: "*.log"
flushers:
  - Type: flusher_kafka_v2
    Brokers:
      - 192.XX.XX.1:9092
      - 192.XX.XX.2:9092
      - 192.XX.XX.3:9092
    Convert:
      TagFieldsRename:
        host.name: hostname
      ProtocolFieldsRename:
        time: '@timestamp'
    Topic: KafkaTestTopic
```

### 指定分区分发

`ilogtail`一共支持三种分区分发方式：

- `random`随机分发, 默认。
- `roundrobin`轮询分发。
- `hash`分发。

`random`和`roundrobin`分发只需要配置`PartitionerType`指定对应的分区分发方式即可。
`hash`分发相对比较特殊，可以指定`HashKeys`，`HashKeys`的中配置的字段名只能是`contents`中的字段属性。

配置用例：

```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test_log
    FilePattern: "*.log"
flushers:
  - Type: flusher_kafka_v2
    PartitionerType: hash
    HashKeys:
      - content.application  
    Brokers:
      - 192.XX.XX.1:9092
      - 192.XX.XX.2:9092
      - 192.XX.XX.3:9092
    Topic: KafkaTestTopic
```

- `content.application`中表示从`contents`中取数据`application`字段数据，如果对`contents`协议字段做了重命名，
例如重名为`messege`，则应该配置为`messege.application`

# 安全连接配置
`flusher_kafka_v2`支持多种安全认证连接`kafka`服务端。
- `PlainText`认证，`ilogtail v1.3.0`开始支持;
- `SASL`认证，`ilogtail v1.3.0`开始支持;
- `TLS`认证，`ilogtail v1.4.0`开始支持;
- `Kerberos`认证(待测试验证)，`ilogtail v1.4.0`开始支持;

前面两种配置比较简单，下面主要介绍下`TLS`和`Kerberos`两种认证的配置。
## TLS配置参考

```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test_log
    FilePattern: "*.log"
flushers:
  - Type: flusher_kafka_v2
    PartitionerType: hash
    HashKeys:
      - content.application  
    Brokers:
      - 192.XX.XX.1:9092
      - 192.XX.XX.2:9092
      - 192.XX.XX.3:9092
    Authentication:
      TLS:
        Enabled: true
        CAFile: /data/cert/ca.crt
        CertFile: /data/cert/client.crt
        KeyFile: /data/cert/client.key
        MinVersion: "1.1"
        MaxVersion: "1.2"
    Topic: KafkaTestTopic
```
**注:** 配置仅供参考，证书文件请自行生成后根据事情情况配置。
## Kerberos配置参考(待验证)
```yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /home/test_log
    FilePattern: "*.log"
flushers:
  - Type: flusher_kafka_v2
    PartitionerType: hash
    HashKeys:
      - content.application  
    Brokers:
      - 192.XX.XX.1:9092
      - 192.XX.XX.2:9092
      - 192.XX.XX.3:9092
    Authentication:
      Kerberos:
        ServiceName: kafka
        Realm: test
        UseKeyTab: true
        ConfigPath: "/etc/krb5.conf"
        KeyTabPath: "/etc/security/kafka.keytab"
    Topic: KafkaTestTopic
```
**注:** Kerberos认证由于缺乏环境，目前待测试验证，使用中如有问题请及时向社区反馈修复。