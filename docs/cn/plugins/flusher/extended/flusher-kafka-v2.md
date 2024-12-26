# Kafka

## 简介

`flusher_kafka_v2` `flusher`插件可以实现将采集到的数据，经过处理后，发送到Kafka。

## 版本

[Beta](../../stability-level.md)

## 配置参数

| 参数                                    | 类型       | 是否必选 | 说明                                                                                                         |
|---------------------------------------|----------|------|------------------------------------------------------------------------------------------------------------|
| Type                                  | String   | 是    | 插件类型                                                                                                       |
| Brokers                               | String数组 | 是    | Kafka Brokers                                                                                              |
| Topic                                 | String   | 是    | Kafka Topic,支持动态topic, 例如: `test_%{content.appname}`                                                       |
| Version                               | String   | 否    | Kafka协议版本号 ,例如：`2.0.0`，默认值：`1.0.0`                                                                         |
| Headers                               | header数组 | 否    | kafka消息头 ，配置使用请参考本文中`Headers`配置用例                                                                          |
| Convert                               | Struct   | 否    | iLogtail数据转换协议配置                                                                                           |
| Convert.Protocol                      | String   | 否    | iLogtail数据转换协议，kafka flusher 可选值：`custom_single`,`custom_single_flatten`,`otlp_log_v1`。默认值：`custom_single` |
| Convert.Encoding                      | String   | 否    | iLogtail flusher数据转换编码，可选值：`json`、`none`、`protobuf`，默认值：`json`                                             |
| Convert.TagFieldsRename               | Map      | 否    | 对日志中tags中的json字段重命名                                                                                        |
| Convert.ProtocolFieldsRename          | Map      | 否    | iLogtail日志协议字段重命名，可当前可重命名的字段：`contents`,`tags`和`time`                                                      |
| Authentication                        | Struct   | 否    | Kafka连接访问认证配置，支持`SASL/PLAIN`，根据kafka服务端认证方式选择配置                                                            |
| Authentication.PlainText.Username     | String   | 否    | PlainText认证用户名                                                                                             |
| Authentication.PlainText.Password     | String   | 否    | PlainText认证密码                                                                                              |
| Authentication.SASL.Username          | String   | 否    | SASL认证用户名                                                                                                  |
| Authentication.SASL.Password          | String   | 否    | SASL认证密码                                                                                                   |
| Authentication.Sasl.SaslMechanism     | String   | 否    | SASL认证，配置可选项：`PLAIN`、`SCRAM-SHA-256`、`SCRAM-SHA-512`                                                       |
| Authentication.TLS.Enabled            | Boolean  | 否    | 是否启用TLS安全连接,                                                                                               |
| Authentication.TLS.CAFile             | String   | 否    | TLS CA根证书文件路径                                                                                              |
| Authentication.TLS.CertFile           | String   | 否    | TLS连接`kafka`证书文件路径                                                                                         |
| Authentication.TLS.KeyFile            | String   | 否    | TLS连接`kafka`私钥文件路径                                                                                         |
| Authentication.TLS.MinVersion         | String   | 否    | TLS支持协议最小版本，可选配置：`1.0, 1.1, 1.2, 1.3`,默认：`1.2`                                                             |
| Authentication.TLS.MaxVersion         | String   | 否    | TLS支持协议最大版本,可选配置：`1.0, 1.1, 1.2, 1.3`,默认采用：`crypto/tls`支持的版本，当前`1.3`                                       |
| Authentication.TLS.InsecureSkipVerify | Boolean  | 否    | 是否跳过TLS证书校验                                                                                                |
| Authentication.Kerberos.ServiceName   | String   | 否    | 服务名称，例如：kafka                                                                                              |
| Authentication.Kerberos.UseKeyTab     | Boolean  | 否    | 是否采用keytab，配置此项后需要配置KeyTabPath，默认为：`false`                                                                 |
| Authentication.Kerberos.Username      | Boolean  | 否    | UseKeyTab设置为`false`的情况下，需要指定用户名                                                                            |
| Authentication.Kerberos.Password      | String   | 否    | UseKeyTab设置为`false`的情况下，需要指定密码                                                                             |
| Authentication.Kerberos.Realm         | String   | 否    | kerberos认证管理域,大小写敏感                                                                                        |
| Authentication.Kerberos.ConfigPath    | Boolean  | 否    | Kerberos krb5.conf                                                                                         |
| Authentication.Kerberos.KeyTabPath    | String   | 否    | keytab的路径                                                                                                  |
| PartitionerType                       | String   | 否    | Partitioner类型。取值：`roundrobin`、`hash`、`random`。默认为：`random`。                                                |
| RequiredAcks                          | int      | 否    | ACK的可靠等级.0=无响应,1=等待本地消息,-1=等待所有副本提交.默认1，                                                                   |
| Compression                           | String   | 否    | 压缩算法，可选值：`none`, `snappy`，`lz4`和`gzip`，默认值`none`                                                           |
| CompressionLevel                      | Int      | 否    | 压缩级别，可选值：`1~9`，默认值：`4`,设置为`0`则禁用`Compression`                                                              |
| MaxMessageBytes                       | Int      | 否    | 一个批次提交的大小限制，配置和`message.max.bytes`对应，默认值：`1000000`                                                         |
| MaxOpenRequests                       | Int      | 否    | 一个连接允许的最大打开的请求数，默认值:`5`                                                                                    |
| MaxRetries                            | Int      | 否    | 提交失败重试次数，最大`3`次，默认值：`3`                                                                                    |
| BulkMaxSize                           | Int      | 否    | 单次请求提交事件数，默认`2048`                                                                                         |
| BulkFlushFrequency                    | Int      | 否    | 发送批量 Kafka 请求之前等待的时间,0标识没有时延，默认值:`0`                                                                       |
| Timeout                               | Int      | 否    | 等待Kafka brokers响应的超时时间，默认`30s`                                                                             |
| BrokerTimeout                         | int      | 否    | kafka broker等待请求的最大时长，默认`10s`                                                                              |
| Metadata.Retry.Max                    | int      | 否    | 最大重试次数，默认值：`3`                                                                                             |
| Metadata.Retry.Backoff                | int      | 否    | 在重试之前等待leader选举发生的时间，默认值：`250ms`                                                                           |
| Metadata.RefreshFrequency             | int      | 否    | Metadata刷新频率，默认值：`250ms`                                                                                   |
| Metadata.Full                         | int      | 否    | 获取原数数据的策略，获取元数据时使用的策略，当此选项为`true`时，客户端将为所有可用主题维护一整套元数据，如果此选项设置为`false`，它将仅刷新已配置主题的元数据。默认值:`false`。         |
| HashKeys                              | String数组 | 否    | PartitionerType为`hash`时，需指定HashKeys。                                                                       |
| HashOnce                              | Boolean  | 否    |                                                                                                            |
| ClientID                              | String   | 否    | 写入Kafka的Client ID，默认取值：`LogtailPlugin`。                                                                    |

- `Version`需要填写的是`kafka protocol version`版本号，`flusher_kafka_v2`当前支持的`kafka`版本范围：`0.8.2.x~3.3.1`。
  请根据自己的`kafka`版本号参照下面的`kafka protocol version`规则进行配置。**建议根据自己的`kafka`
  版本指定对应`protocol version`**,
  `kafka protocol version`支持版本号如下：

```plain
0.8.2.0,0.8.2.1,0.8.2.2
0.9.0.0,0.9.0.1
0.10.0.0,0.10.0.1,0.10.1.0,0.10.1.1,0.10.2.0,0.10.2.1,0.10.2.2
0.11.0.0,0.11.0.1,0.11.0.2
1.0.0,1.0.1,1.0.2,1.1.0,1.1.1,
2.0.0,2.0.1,2.1.0,2.1.1,2.2.0,2.2.1,2.2.2,2.3.0,2.3.1,2.4.0,2.4.1,2.5.0,2.5.1,2.6.0,2.6.1,2.6.2,2.7.0,2.7.1,2.8.0,2.8.1,2.8.2
3.0.0,3.0.1,3.0.2,3.1.0,3.1.1,3.1.2,3.2.0,3.2.1,3.2.2,3.2.3,3.3.0,3.3.1,3.3.2,3.4.0,3.4.1,3.5.0,3.5.1,3.6.0
```

- `Brokers`是个数组，多个`Broker`地址不能使用`;`或者`,`来隔开放在一行里，`yaml`配置文件中正确的多个`Broker`地址配置参考如下：

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_kafka_v2
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
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_kafka_v2
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
    "k8s.namespace.name": "java_app",
    "host.ip": "192.168.6.128",
    "host.name": "master",
    "log.file.path": "/data/test.log"
  },
  "time": 1664435098
}
```

### 动态topic

针对上面写入的这种日志格式，如果想根据`application`名称针对不用的应用推送到不同的`topic`，
则`topic`可以这样配置。

```yaml
Topic: test_%{content.application}
```

最后`ilogtail`就自动将日志推送到`test_springboot-docker`这个`topic`中。
`topic`动态表达式规则：

- `%{content.fieldname}`。`content`代表从`contents`中取指定字段值
- `%{tag.fieldname}`,`tag`表示从`tags`中取指定字段值，例如：`%{tag.k8s.namespace.name}`
- `${env_name}`, 读取系统变量绑定到动态`topic`上，`ilogtail 1.5.0`开始支持。
- 其它方式暂不支持

#### 动态topic中使用系统变量

动态`topic`绑定系统变量的两种场景：

- 将系统变量采集添加到日志的`tag`中，然后使用`%{tag.fieldname}`规则完成绑定。
- 对系统变量无采集存储需求，只是想根据设定的系统变量将日志推送到指定的`topic`中，直接采用`${env_name}`
  规则完成绑定，此方式需要`1.5.0`才支持。

由于上面提到的两种系统变量的采集绑定都需要做一些特殊配置，因此下面将分别介绍下相关的配置操作。

**（1）将系统变量采集到日志中完成动态`topic`绑定**

将系统变量采集添加到日志中有两种方式，一种是在`ilogtail`容器`env`添加，另一种是通过`processor_add_fields` 插件添加，
两种方式不同的配置参考下面的介绍

- 在`daemonset`或者`sidecar`方式部署的`ilogtail`容器`env`配置部分添加自定义的系统变量，配置参考案例如下：

```yaml
env:
  - name: ALIYUN_LOG_ENV_TAGS       # add log tags from env
    value: _node_name_|_node_ip_|_app_name_
  - name: _app_name_  # 添加自定义_app_name_变量，
    value: kafka
```

自定义的变量`_app_name_`被添加到`ALIYUN_LOG_ENV_TAGS`中，日志的`tags`中会看到自定义的变量， 此时动态 `topic`
采用`%{tag.fieldname}`规则配置即可。

- 使用`processor_add_fields` 插件系统变量添加到日志中，配置参考如下：

```yaml
processors:
  - Type: processor_add_fields
    Fields:
      service: ${env_name}
    IgnoreIfExist: false
```

这里`${env_name}`生效依赖于`ilogtail`的`enable_env_ref_in_config`配置，从`ilogtail 1.5.0`开始支持。

**（2）直接采用`$`符将系统变量绑定动态`topic`中**

在`daemonset`或者`sidecar`方式部署的`ilogtail`容器`env`配置部分添加自定义的系统变量，配置参考案例如下：

```yaml
env:
  - name: ALIYUN_LOG_ENV_TAGS       # add log tags from env
    value: _node_name_|_node_ip_
  - name: app_name  # 添加自定义app_name变量，
    value: kafka
```

`app_name`添加到系统变量中后，直接采用动态topic的：`${env_name}`规则即可绑定。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_kafka_v2
    Brokers:
      - 192.XX.XX.1:9092
      - 192.XX.XX.2:9092
      - 192.XX.XX.3:9092
    Topic: ilogtail_${app_name}
```

- `${app_name}`就是我们上面添加的系统变量。

### TagFieldsRename

例如将`tags`中的`host.name`重命名为`hostname`，配置参考如下：

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
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
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
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
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
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

### 配置Headers

`iLogtail`中`Kafka`的消息头是以键值对数组的形式配置的。`header`中`value`仅支持字符串类型。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_kafka_v2
    Brokers:
      - 192.XX.XX.1:9092
      - 192.XX.XX.2:9092
      - 192.XX.XX.3:9092
    Topic: KafkaTestTopic
    Headers:
      - key: "key1"
        value: "value1"
      - key: "key2"
        value: "value2"
```

### 数据平铺

`ilogtail 1.8.0`新增数据平铺协议`custom_single_flatten`，`contents`、`tags`和`time`三个`convert`层的协议字段中数据做一级打平。
当前`convert`协议在单条数据处理仅支持`json`编码，因此`custom_single_flatten`需要配合`json`编码一起使用。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_kafka_v2
    Brokers:
      - 192.XX.XX.1:9092
      - 192.XX.XX.2:9092
      - 192.XX.XX.3:9092
    Convert:
      Protocol: custom_single_flatten
      Encoding: json
    Topic: KafkaTestTopic
```

非平铺前写入`kafka`的消息格式

```json
{
  "contents": {
    "class": "org.springframework.web.servlet.DispatcherServlet@initServletBean:547",
    "application": "springboot-docker",
    "level": "ERROR",
    "message": "Completed initialization in 9 ms",
    "thread": "http-nio-8080-exec-10",
    "@time": "2022-07-20 16:55:05.415"
  },
  "tags": {
    "k8s.namespace.name": "java_app",
    "host.ip": "192.168.6.128",
    "host.name": "master",
    "log.file.path": "/data/test.log"
  },
  "time": 1664435098
}
```

使用平铺协议后`custom_single_flatten`，`json`全部被一级平铺。

```json
{
  "class": "org.springframework.web.servlet.DispatcherServlet@initServletBean:547",
  "application": "springboot-docker",
  "level": "ERROR",
  "message": "Completed initialization in 9 ms",
  "thread": "http-nio-8080-exec-10",
  "@time": "2022-07-20 16:55:05.415",
  "k8s.namespace.name": "java_app",
  "host.ip": "192.168.6.128",
  "host.name": "master",
  "log.file.path": "/data/test.log",
  "time": 1664435098
}
```

## 安全连接配置

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
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
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

## Kerberos配置参考

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
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
