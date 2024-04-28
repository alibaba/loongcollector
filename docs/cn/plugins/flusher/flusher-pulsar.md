# Pulsar

## 简介

`flusher_pulsar` `flusher`插件可以实现将采集到的数据，经过处理后，发送到`Pulsar`。

## 版本

[Alpha](../stability-level.md)

## 配置参数

| 参数                                    | 类型       | 是否必选 | 说明                                                                                                         |
|---------------------------------------|----------|------|------------------------------------------------------------------------------------------------------------|
| Type                                  | String   | 是    | 插件类型                                                                                                       |
| Url                                   | String   | 是    | Pulsar url,多地址用逗号分隔，可以参考本文中的用例配置                                                                           |
| Topic                                 | String   | 是    | Pulsar Topic,支持动态topic, 例如: `test_%{contents.appname}`                                                     |
| Name                                  | String   | 否    | producer名称，默认ilogtail                                                                                      |
| Convert                               | Struct   | 否    | ilogtail数据转换协议配置                                                                                           |
| Convert.Protocol                      | String   | 否    | ilogtail数据转换协议，kafka flusher 可选值：`custom_single`,`custom_single_flatten`,`otlp_log_v1`。默认值：`custom_single` |
| Convert.Encoding                      | String   | 否    | ilogtail flusher数据转换编码，可选值：`json`、`none`、`protobuf`，默认值：`json`                                             |
| Convert.TagFieldsRename               | Map      | 否    | 对日志中tags中的json字段重命名                                                                                        |
| Convert.ProtocolFieldsRename          | Map      | 否    | ilogtail日志协议字段重命名，可当前可重命名的字段：`contents`,`tags`和`time`                                                      |
| EnableTLS                             | Boolean  | 否    | 是否启用TLS安全连接，对应采用TLS和Athenz两种认证模式都需要设置为true，默认值：`false`                                                     |
| TLSTrustCertsFilePath                 | String   | 否    | TLS CA根证书文件路径，对应采用TLS和Athenz认证时需要指定                                                                        |
| Authentication                        | Struct   | 否    | Pulsar连接访问认证配置                                                                                             |
| Authentication.TLS.CertFile           | String   | 否    | TLS连接`Pulsar`证书文件路径                                                                                        |
| Authentication.TLS.KeyFile            | String   | 否    | TLS连接`Pulsar`私钥文件路径                                                                                        |
| Authentication.Token.Token            | String   | 否    | 采用JWT 认证方式的token                                                                                           |
| Authentication.Athenz.ProviderDomain  | String   | 否    | Provider domain name                                                                                       |
| Authentication.Athenz.TenantDomain    | String   | 否    | 租户域                                                                                                        |
| Authentication.Athenz.TenantService   | String   | 否    | 租户服务                                                                                                       |
| Authentication.Athenz.PrivateKey      | String   | 否    | Tenant private key path                                                                                    |
| Authentication.Athenz.KeyID           | String   | 否    | Key id for the tenant private key                                                                          |
| Authentication.Athenz.PrincipalHeader | String   | 否    |                                                                                                            |
| Authentication.Athenz.ZtsURL          | String   | 否    | ZTS server的地址                                                                                              |
| Authentication.OAuth2.Enabled         | Boolean  | 否    | 是否启用OAuth2认证                                                                                               |
| Authentication.OAuth2.IssuerURL       | String   | 是    | 认证提供商的URL，OAuth2.Enabled开启时必填                                                                              |
| Authentication.OAuth2.PrivateKey      | String   | 是    | JSON 凭据文件的 URL，OAuth2.Enabled开启时必填                                                                         |
| Authentication.OAuth2.Audience        | String   | 否    | Pulsar 集群的 OAuth 2.0 “资源服务” 的标识符                                                                           |
| Authentication.OAuth2.Scope           | String   | 否    | 访问范围                                                                                                       |
| CompressionType                       | String   | 否    | 压缩算法，`NONE,LZ4,ZLIB,ZSTD`，默认值`NONE`                                                                        |
| BlockIfQueueFull                      | Boolean  | 否    | 队列满的时候是否阻塞，默认值:`false`                                                                                     |
| SendTimeout                           | Int      | 否    | 发送超时时间，默认`30s`                                                                                             |
| OperationTimeout                      | Int      | 否    | pulsar producer创建、订阅、取消订阅的超时时间，默认`30s`                                                                     |
| ConnectionTimeout                     | Int      | 否    | tcp连接建立超时时间，默认`5s`                                                                                         |
| MaxConnectionsPerBroker               | Int      | 否    | 单个broker连接池保持的连接数，默认`1`                                                                                    |
| MaxReconnectToBroker                  | Int      | 否    | 重连broker的最大重试次数，默认为无限                                                                                      |
| HashingScheme                         | Int      | 否    | 消息push分区的分发方式：`JavaStringHash`,`Murmur3_32Hash`,默认值：`JavaStringHash`                                       |
| BatchingMaxPublishDelay               | int      | 否    | 提交时延，默认值：`1ms`                                                                                             |
| BatchingMaxMessages                   | int      | 否    | 批量提交最大消息数，默认值：`1000`                                                                                       |
| MaxCacheProducers                     | int      | 否    | 动态topic情况下最大Producer数量 ，默认最大数量：8,使用动态topic的使用可以根据自己的情况调整。                                                  |
| PartitionKeys                         | String数组 | 否    | 指定消息分区分发的key。                                                                                              |
| ClientID                              | String   | 否    | 写入Pulsar的Client ID，默认取值：`iLogtail`。                                                                        |

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，并将采集结果发送到Pulsar。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_pulsar
    URL: "pulsar://192.168.6.128:6650,192.168.6.129:6650,192.168.6.130:6650"
    Topic: PulsarTestTopic
```

## 进阶配置

以下面的一段日志为例，后来将展开介绍ilogtail pulsar flusher的一些高阶配置

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
- `${env_name}`, 读取系统变量绑定到动态`topic`上，`ilogtail 1.5.0`开始支持。可以参考`flusher-kafka_v2`中的使用。
- 其它方式暂不支持

### TagFieldsRename

例如将`tags`中的`host.name`重命名为`hostname`，配置参考如下：

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_pulsar
    URL: "pulsar://192.168.6.128:6650,192.168.6.129:6650,192.168.6.130:6650"
    Convert:
      TagFieldsRename:
        host.name: hostname
    Topic: PulsarTestTopic
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
  - Type: flusher_pulsar
    URL: "pulsar://192.168.6.128:6650,192.168.6.129:6650,192.168.6.130:6650"
    Convert:
      TagFieldsRename:
        host.name: hostname
      ProtocolFieldsRename:
        time: '@timestamp'
    Topic: PulsarTestTopic
```

### 指定分区分发

`ilogtail flusher pulsar`使用的官方`SDK`只支持`hash`方式分区投递，通过`HashingScheme`来选择不同的`hash`算法。
分发是可以指定`PartitionKeys`，`PartitionKeys`的中配置的字段名只能是`contents`中的字段属性。

配置用例：

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_pulsar
    PartitionKeys:
      - content.application  
    URL: "pulsar://192.168.6.128:6650,192.168.6.129:6650,192.168.6.130:6650"
    Topic: PulsarTestTopic
```

- `content.application`中表示从`contents`中取数据`application`字段数据，如果对`contents`协议字段做了重命名，
  例如重名为`messege`，则应该配置为`messege.application`

### 数据平铺

`ilogtail 1.8.0`新增数据平铺协议`custom_single_flatten`，`contents`、`tags`和`time`三个`convert`层的协议字段中数据做一级打平。
当前`convert`协议在单条数据处理仅支持`json`编码，因此`custom_single_flatten`需要配合`json`编码一起使用。

配置用例：

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_pulsar
    Convert:
      Protocol: custom_single_flatten
      Encoding: json
    URL: "pulsar://192.168.6.128:6650,192.168.6.129:6650,192.168.6.130:6650"
    Topic: PulsarTestTopic
```

非平铺前写入`pulsar`的消息格式

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
    "k8s.namespace.name":"java_app",
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
    "k8s.namespace.name":"java_app",
    "host.ip": "192.168.6.128",
    "host.name": "master",
    "log.file.path": "/data/test.log",
    "time": 1664435098
}
```

## 安全连接配置

`flusher_pulsar`支持多种安全认证连接`pulsar`服务端。

- `TLS`认证;
- `Token`JWT Token认证;
- `Athenz` pulsar租户域认证;
- `OAuth2`认证；

JWT Token认证配置比较简单，参照前面的配置表配置即可，下面主要介绍下`OAuth2`,`TLS`和`Athenz`两种认证的配置。

## OAuth2认证配置参考(待验证)

下面配置仅供参考，请根据服务器实际部署情况配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_pulsar
    URL: "pulsar://192.168.6.128:6650,192.168.6.129:6650,192.168.6.130:6650"
    Authentication:
      OAuth2:
        Enabled: true
        IssuerURL: https://accounts.google.com
        PrivateKey: file:/path/to/file/credentials_file.json
        Audience: https://broker.example.com
        Scope: api://pulsar-cluster-1/.default
    Topic: PulsarTestTopic
```

credentials_file.json配置内容样例

```json
{
  "type": "client_credentials",
  "client_id": "d9ZyX97q1ef8Cr81WHVC4hFQ64vSlDK3",
  "client_secret": "on1uJ...k6F6R",
  "client_email": "1234567890-abcdefghijklmnopqrstuvwxyz@developer.gserviceaccount.com",
  "issuer_url": "https://accounts.google.com"
}
```

## TLS配置参考(待验证)

下面配置仅供参考，请根据服务器实际部署情况配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_pulsar
    URL: "pulsar+ssl://192.168.6.128:6651,192.168.6.129:6651,192.168.6.130:6651"
    EnableTLS: true
    TLSTrustCertsFilePath: /data/cert/ca.crt
    Authentication:
      TLS:
        CertFile: /data/cert/client.crt
        KeyFile: /data/cert/client.key
    Topic: PulsarTestTopic
```

- `EnableTLS` 如果要启用`TLS`必须设置为`true`。开始`TLS`的情况下，URL头部为`pulsar+ssl://`
- `TLSTrustCertsFilePath`根证书需要设置。
**注:** 配置仅供参考，证书文件请自行生成后根据事情情况配置。

## Athenz认证配置参考(待验证)

下面配置仅供参考，请根据服务器实际部署情况配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
flushers:
  - Type: flusher_pulsar
    URL: "pulsar+ssl://192.168.6.128:6651,192.168.6.129:6651,192.168.6.130:6651"
    EnableTLS: true
    TLSTrustCertsFilePath: /data/cert/ca.crt
    Authentication:
      Athenz:
        ProviderDomain: pulsar
        TenantDomain: shopping
        TenantService: some_app
        PrivateKey: file:///path/to/client-key.pem
        KeyID: v1
    Topic: PulsarTestTopic
```

- `EnableTLS` 如果要启用`Athenz`认证必须设置为`true`。开始`TLS`的情况下，`URL`头部为`pulsar+ssl://`
- `TLSTrustCertsFilePath`根证书需要设置。
