# 如何将业务日志采集到Kafka

`iLogtail`是阿里云日志服务（SLS）团队自研的可观测数据采集`Agent`，拥有的轻量级、高性能、自动化配置等诸多生产级别特性，可以署于物理机、虚拟机、`Kubernetes`等多种环境中来采集遥测数据。iLogtail在阿里云上服务了数万家客户主机和容器的可观测性采集工作，在阿里巴巴集团的核心产品线，如淘宝、天猫、支付宝、菜鸟、高德地图等也是默认的日志、监控、Trace等多种可观测数据的采集工具。目前iLogtail已有千万级的安装量，每天采集数十PB的可观测数据，广泛应用于线上监控、问题分析/定位、运营分析、安全分析等多种场景，在实战中验证了其强大的性能和稳定性。

在当今云原生的时代，我们坚信开源才是iLogtail最优的发展策略，也是释放其最大价值的方法。因此，我们决定将`iLogtail`开源，期望同众多开发者一起将iLogtail打造成世界一流的可观测数据采集器。

## 背景 <a href="#lqmpz" id="lqmpz"></a>

日志作为可观测性建设中的重要一环，可以记录详细的访问请求以及错误信息，在业务分析、问题定位等方面往往会发挥很大的作用。一般开发场景下，当需要进行日志分析时，往往是直接在日志文件中进行grep搜索对应的关键字；但在大规模分布式生产环境下，此方法效率低下，常见解决思路是建立集中式日志收集系统，将所有节点上的日志统一收集、管理、分析。目前市面上比较主流的开源方案是基于ELK构建一套日志采集系统。

![](<../.gitbook/assets/getting-started/collect-to-kafka/elk-arch.png>)

该架构中，`Filebeat`作为日志源的采集`Agent`部署在业务集群上进行原始日志采集，并采集到的日志发送到消息队列`Kafka`集群。之后，由`Logstash`从`Kafka`消费数据，并经过过滤、处理后，将标准化后的日志写入`Elasticsearch`集群进行存储。最后，由`Kibana`呈现给用户查询。

该架构两个关键设计考虑：

* 引入`Kafka`。
  * 有助于上下游解耦。采集端可以专注于日志采集，数据分析段专注于从`Kafka`消费数据，`Kafka`也支持重复消费，可以有效降低日志重复采集对端的影响。
  * 当业务量增长时，容易出现日志量突增的情况，直接写入ES很可能出现性能下降，甚至有ES集群宕机的风险。引入`Kafka`能很好的解决流量峰值的问题，削峰填谷，可以保证写入ES集群流量的均衡。
  * `Kafka`作为消息队列具有持久化的能力，数据不会丢失，吞度量大，能够很好的防止数据丢失。
* 使用`Filebeat`作为日志源的采集Agent。
  * `Filebeat`是一个轻量级的日志传输工具，作为采集端Agent，可以有效弥补`Logstash`的缺点（例如，依赖`java`、数据量大时资源消耗过多）。

虽然引入了`Filebeat`作为日志源的采集`Agent`，可以有效提升端上的采集效率及资源占用情况，但是超大流量场景下`Filebeat`依然会有些力不从心。此外，容器场景下Filebeat支持也不是很好。而iLogtail作为阿里云日志服务（SLS）团队自研的可观测数据采集`Agent`，在[日志采集性能](https://github.com/alibaba/ilogtail/blob/main/docs/zh/performance/Performance-compare-with-filebeat.md)及[K8s](https://developer.aliyun.com/article/806369)支持上都有不错的体验，因此我们可以将`iLogtail`引入到日志采集系统的架构中。由于`iLogtail`有明显的性能优势，且具有极强的数据处理能力，因此可以将`Logstash`所承载的日志处理前置到`iLogtail`来进行，这样可以有效降低用户的存储成本。

此外，如果`iLogtail`跟阿里云日志服务（SLS）有天然的集成优势，当使用SLS作为后端存储系统时，可以直接写入，不需要额外的再引入消息队列。

![](<../.gitbook/assets/getting-started/collect-to-kafka/ilogtail-arch.png>)

## 操作实战
本文将会详细介绍如何使用`iLogtail`社区版将日志采集到`Kafka`中，从而帮助使用者构建日志采集系统。
### 场景 <a href="#qarop" id="qarop"></a>

采集`/root/bin/input_data/access.log`、`/root/bin/input_data/error.log`，并将采集到的日志写入本地部署的kafka中。为此，我们将配置两个采集配置项。
其中，`access.log`需要正则解析；`error.log`为单行文本打印。

![](<../.gitbook/assets/getting-started/collect-to-kafka/collection-config.png>)

### 前提条件 <a href="#hvouy" id="hvouy"></a>

* 安装

```bash
# 下载、解压
$ wget  https://dlcdn.apache.org/kafka/3.2.0/kafka_2.13-3.2.0.tgz
$ tar -xzf kafka_2.13-3.2.0.tgz
$ cd kafka_2.13-3.2.0

# 启动
# Start the ZooKeeper service
# Note: Soon, ZooKeeper will no longer be required by Apache Kafka.
$ bin/zookeeper-server-start.sh config/zookeeper.properties &

# Start the Kafka broker service
$ bin/kafka-server-start.sh config/server.properties &
```

* 分别创建两个topic，用于存储access-log、error-log。

```bash
bin/kafka-topics.sh --create --topic access-log --bootstrap-server localhost:9092
bin/kafka-topics.sh --create --topic error-log --bootstrap-server localhost:9092
```

* 更多部署说明，详见[链接](https://kafka.apache.org/quickstart)。

### 安装ilogtail <a href="#gee97" id="gee97"></a>

* 下载

```bash
$ wget https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/latest/ilogtail-latest.linux-amd64.tar.gz
tar -xzvf ilogtail-latest.linux-amd64.tar.gz
$ cd ilogtail-<version>

$ ll
drwxrwxr-x 5 505 505      4096 7月  10 18:00 example_config
-rwxr-xr-x 1 505 505  84242040 7月  11 00:00 ilogtail
-rwxr-xr-x 1 505 505     16400 7月  11 00:00 libPluginAdapter.so
-rw-r--r-- 1 505 505 115963144 7月  11 00:00 libPluginBase.so
-rw-rw-r-- 1 505 505     11356 7月  11 00:00 LICENSE
-rw-rw-r-- 1 505 505      4834 7月  11 00:00 README.md
drwxr-xr-x 2 root root    4096 7月  12 09:55 user_yaml_config.d
```

* 采集配置

在`user_yaml_config.d`创建针对`access_log`、`error_log`分别创建两个采集配置，两个采集配置分别将日志采集到`Kafka`不同的`Topic`中。

```yaml
# 访问日志采集配置
$ cat user_yaml_config.d/access_log.yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /root/bin/input_data/
    FilePattern: access.log
processors:
  - Type: processor_regex
    SourceKey: content
    Regex: ([\d\.]+) \S+ \S+ \[(\S+) \S+\] \"(\w+) ([^\\"]*)\" ([\d\.]+) (\d+) (\d+) (\d+|-) \"([^\\"]*)\" \"([^\\"]*)\"
    Keys:
      - ip
      - time
      - method
      - url
      - request_time
      - request_length
      - status
      - length
      - ref_url
      - browser
flushers:
  - Type: flusher_kafka
    Brokers:
      - localhost:9092
    Topic: access-log
```

```yaml
# 错误日志采集配置
$ cat user_yaml_config.d/error_log.yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /root/bin/input_data/
    FilePattern: error.log
flushers:
  - Type: flusher_kafka
    Brokers:
      - localhost:9092
    Topic: error-log
```

```bash
$ tree user_yaml_config.d/
user_yaml_config.d/
├── access_log.yaml
└── error_log.yaml
```

* 启动

```bash
$ nohup ./ilogtail > stdout.log 2> stderr.log &
```

### 验证 <a href="#ovijv" id="ovijv"></a>

* 访问日志验证

```json
# 终端1: 启动kafka-console-consumer，消费access-log
$ bin/kafka-console-consumer.sh --topic access-log --from-beginning --bootstrap-server localhost:9092

# 终端2: 写入访问日志
$ echo '127.0.0.1 - - [10/Aug/2017:14:57:51 +0800] "POST /PutData?Category=YunOsAccountOpLog HTTP/1.1" 0.024 18204 200 37 "-" "aliyun-sdk-java"' >> /root/bin/input_data/access.log

# 终端1: 消费到写入的访问日志，说明流程正常。
{"Time":1657592120,"Contents":[{"Key":"__tag__:__path__","Value":"/root/bin/input_data/access.log"},{"Key":"ip","Value":"127.0.0.1"},{"Key":"time","Value":"10/Aug/2017:14:57:51"},{"Key":"method","Value":"POST"},{"Key":"url","Value":"/PutData?Category=YunOsAccountOpLog HTTP/1.1"},{"Key":"request_time","Value":"0.024"},{"Key":"request_length","Value":"18204"},{"Key":"status","Value":"200"},{"Key":"length","Value":"37"},{"Key":"ref_url","Value":"-"},{"Key":"browser","Value":"aliyun-sdk-java"}]}
```

* 错误日志验证

```json
# 终端1: 启动kafka-console-consumer，消费error-log
$ bin/kafka-console-consumer.sh --topic error-log --from-beginning --bootstrap-server localhost:9092

# 终端2: 写入错误日志
$ echo -e '2022-07-12 10:00:00 ERROR This is a error!\n2022-07-12 10:00:00 ERROR This is a new error!' >> /root/bin/input_data/error.log

# 终端1: 消费到写入的错误日志，说明流程正常。
{"Time":1657591799,"Contents":[{"Key":"__tag__:__path__","Value":"/root/bin/input_data/error.log"},{"Key":"content","Value":"2022-07-12 10:00:00 ERROR This is a error!"}]}
{"Time":1657591799,"Contents":[{"Key":"__tag__:__path__","Value":"/root/bin/input_data/error.log"},{"Key":"content","Value":"2022-07-12 10:00:00 ERROR This is a new error!"}]}
```

## 总结 <a href="#ovoc3" id="ovoc3"></a>

以上，我们介绍了使用iLogtail社区版将日志采集到Kafka的方法，大家可以与其他开源软件Kafka、ELK配合，构建出属于自己的开源日志采集系统；同样的，如果对采集的稳定性、查询的体验有更高的要求，也可以基于SLS构建商业版的可观测平台。



## 关于iLogtail
iLogtail作为阿里云SLS提供的可观测数据采集器，可以运行在服务器、容器、K8s、嵌入式等多种环境，支持采集数百种可观测数据（日志、监控、Trace、事件等），已经有千万级的安装量。目前，iLogtail已正式开源，欢迎使用及参与共建。

* GitHub：[https://github.com/alibaba/ilogtail](https://github.com/alibaba/ilogtail)

* 社区版文档：[https://ilogtail.gitbook.io/ilogtail-docs/about/readme](https://ilogtail.gitbook.io/ilogtail-docs/about/readme)

* 交流群请扫描

![](../.gitbook/assets/chatgroup.png)
