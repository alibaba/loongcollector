# 主机环境采集业务日志到SLS

`iLogtail`是阿里云日志服务（SLS）团队自研的可观测数据采集`Agent`，拥有的轻量级、高性能、自动化配置等诸多生产级别特性，可以署于物理机、虚拟机、`Kubernetes`等多种环境中来采集遥测数据。iLogtail在阿里云上服务了数万家客户主机和容器的可观测性采集工作，在阿里巴巴集团的核心产品线，如淘宝、天猫、支付宝、菜鸟、高德地图等也是默认的日志、监控、Trace等多种可观测数据的采集工具。目前iLogtail已有千万级的安装量，每天采集数十PB的可观测数据，广泛应用于线上监控、问题分析/定位、运营分析、安全分析等多种场景，在实战中验证了其强大的性能和稳定性。
在当今云原生的时代，我们坚信开源才是iLogtail最优的发展策略，也是释放其最大价值的方法。因此，我们决定将`iLogtail`开源，期望同众多开发者一起将iLogtail打造成世界一流的可观测数据采集器。

## 背景

日志作为可观测性建设中的重要一环，可以记录详细的访问请求以及错误信息，在业务分析、问题定位等方面往往会发挥很大的作用。一般开发场景下，当需要进行日志分析时，往往是直接在日志文件中进行grep搜索对应的关键字；但在大规模分布式生产环境下，此方法效率低下，常见解决思路是建立集中式日志收集系统，将所有节点上的日志统一收集、管理、分析。目前市面上比较主流的开源方案是基于ELK构建一套日志采集分析系统。

![](<../.gitbook/assets/getting-started/collect-to-kafka/elk-arch.png>)

该架构中，`Filebeat`作为日志源的采集`Agent`部署在业务集群上进行原始日志采集，并采集到的日志发送到消息队列`Kafka`集群。之后，由`Logstash`从`Kafka`消费数据，并经过过滤、处理后，将标准化后的日志写入`Elasticsearch`集群进行存储。最后，由`Kibana`呈现给用户查询。虽然该架构可以提供比较完整的日志采集、分析功能，但是整体涉及的组件非常多，大规模生产环境部署复杂度高，且大流量下ES可能不稳定，运维成本都会很高。

阿里云提供的SLS服务是一个纯定位在日志/时序类可观测数据分析场景的云上托管服务，相对于ELK在日志领域内做了很多定制化开发，在易用性、功能完备性、性能、成本等方便，都有不错表现。`iLogtail`作为SLS官方标配的可观测数据采集器，在[日志采集性能](https://github.com/alibaba/ilogtail/blob/main/docs/zh/performance/Performance-compare-with-filebeat.md)及[K8s](https://developer.aliyun.com/article/806369)支持上都有不错的体验；`iLogtail`有明显的性能优势，可以将部分数据处理前置，有效降低存储成本。

![](<../.gitbook/assets/getting-started/collect-to-sls/sls-collect-arch.png>)

目前**社区版**`iLogtail`也对SLS提供了很好的支持，本文将会详细介绍如何使用**社区版**`iLogtail`，并结合SLS云服务快速构建出一套高可用、高性能的日志采集分析系统。

备注：`iLogtail`**社区版**相对于`iLogtail`企业版，核心采集能力上基本是一致的，但是管控、可观测能力上会有所弱化，这些能力需要配合SLS服务端才能发挥出来。欢迎使用[iLogtail企业版](https://help.aliyun.com/document_detail/95923.html)体验，两个版本差异详见[链接](https://ilogtail.gitbook.io/ilogtail-docs/about/compare-editions)。

## SLS简介

日志服务SLS是云原生观测与分析平台，为Log、Metric、Trace等数据提供大规模、低成本、实时的平台化服务。日志服务一站式提供数据采集、加工、查询与分析、可视化、告警、消费与投递等功能，全面提升您在研发、运维、运营、安全等场景的数字化能力。
通过SLS可以快速的搭建属于自己的可观测分析平台，可以快速享受到SLS提供的各种数据服务，包括不限于：查询与分析、可视化、告警等。

- 查询分析
  - 支持精确查询、模糊查询、全文查询、字段查询。
  - 以SQL作为查询和分析框架，同时在框架中融入PromQL语法和机器学习函数。

![](<../.gitbook/assets/getting-started/collect-to-sls/sls-search.png>)

- 可视化
  - 基于SLS统一的查询分析引擎，以图表的形式将查询与分析结果呈现出来，清晰呈现全局态势。
  - 支持与第三方可视化工具对接。

![](<../.gitbook/assets/getting-started/collect-to-sls/sls-visualization.png>)

- 监控告警：提供一站式的告警监控、降噪、事务管理、通知分派的智能运维平台。

![](<../.gitbook/assets/getting-started/collect-to-sls/sls-alert.png>)

## 操作实战

以下介绍如何使用`iLogtail`社区版采集主机环境业务日志到SLS。

### 场景

采集`/root/bin/input_data/access.log`、`/root/bin/input_data/error.log`，并将采集到的日志写入SLS中。
其中，`access.log`需要正则解析；`error.log`为单行文本打印。
如果之前已经使用`iLogtail`将日志采集到`Kafka`，在迁移阶段可以保持双写，等稳定后删除`Kafka Flusher`配置即可。

![](<../.gitbook/assets/getting-started/collect-to-sls/collect-to-sls-and-kafka.png>)

### 前提条件

- 登陆阿里云SLS控制台，[开通SLS服务](https://help.aliyun.com/document_detail/54604.html#section-j4p-xt3-arc)。
- 已创建一个Project；两个logstore，分别为access-log、error-log。更多信息，请参见[创建Project](https://help.aliyun.com/document_detail/48984.htm#section-ahq-ggx-ndb)和[创建Logstore](https://help.aliyun.com/document_detail/48990.htm#section-v52-2jx-ndb)。

![](<../.gitbook/assets/getting-started/collect-to-sls/create-logstore.png>)

- 开启[全文索引](https://help.aliyun.com/document_detail/90732.html)。
- 进入Project首页，查看[访问域名](https://help.aliyun.com/document_detail/29008.html?spm=5176.2020520112.help.dexternal.5d8b34c0QXLYgp)。

![](<../.gitbook/assets/getting-started/collect-to-sls/endpoint.png>)

### 安装iLogtail

- 下载

```shell
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
-rw-rw-r-- 1  505  505    118 7月  14 11:22 ilogtail_config.json
drwxr-xr-x 2 root root    4096 7月  12 09:55 config
```

- 获取阿里云AK，并进行配置。

```shell
$ cat ilogtail_config.json
{
   "default_access_key_id": "xxxxxx",
   "default_access_key": "yyyyy"
}
```

- 采集配置

在`config/local`创建针对`access_log`、`error_log`分别创建两个采集配置，两个采集配置分别将日志采集到`SLS`不同`logstore` 及`Kafka`不同的`Topic`中。双写适用于从`Kafka`迁移到SLS的场景，如果迁移完成稳定后，可以删除`flusher_kafka`，只保留`flusher_sls`即可。

```yaml
# 访问日志采集配置
$ cat config/local/access_log.yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /root/bin/input_data/access.log
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
  - Type: flusher_sls
    Region: cn-hangzhou
    Endpoint: cn-hangzhou.log.aliyuncs.com
    Project: test-ilogtail
    Logstore: access-log
  - Type: flusher_kafka
    Brokers:
      - localhost:9092
    Topic: access-log
```

```yaml
# 错误日志采集配置
$ cat config/local/error_log.yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /root/bin/input_data/error.log
flushers:
  - Type: flusher_sls
    Endpoint: cn-hangzhou.log.aliyuncs.com
    Project: test-ilogtail
    Logstore: access-log
  - Type: flusher_kafka
    Brokers:
      - localhost:9092
    Topic: error-log
```

```shell
$ tree config/local/
config/local/
─ access_log.yaml
└── error_log.yaml
```

- 启动

```shell
nohup ./ilogtail > stdout.log 2> stderr.log &
```

### 验证

- 访问日志验证，查看logstore数据正常。

```shell
# 写入访问日志
echo '127.0.0.1 - - [10/Aug/2017:14:57:51 +0800] "POST /PutData?Category=YunOsAccountOpLog HTTP/1.1" 0.024 18204 200 37 "-" "aliyun-sdk-java"' >> /root/bin/input_data/access.log
```

![](<../.gitbook/assets/getting-started/collect-to-sls/sls-access-log.png>)

- 错误日志验证，查看logstore数据正常。

```shell
# 写入错误日志
echo -e '2022-07-12 10:00:00 ERROR This is a error!\n2022-07-12 10:00:00 ERROR This is a new error!' >> /root/bin/input_data/error.log
```

![](<../.gitbook/assets/getting-started/collect-to-sls/sls-error-log.png>)

## 总结

以上，我们介绍了使用iLogtail社区版将日志采集到SLS的方法。如果想体验企业版iLogtail与SLS更深度的集成能力，欢迎使用iLogtail企业版，并配合SLS构建可观测平台。

## 关于iLogtail

iLogtail作为阿里云SLS提供的可观测数据采集器，可以运行在服务器、容器、K8s、嵌入式等多种环境，支持采集数百种可观测数据（日志、监控、Trace、事件等），已经有千万级的安装量。目前，iLogtail已正式开源，欢迎使用及参与共建。

- GitHub：[https://github.com/alibaba/ilogtail](https://github.com/alibaba/ilogtail)

- 社区版文档：[https://ilogtail.gitbook.io/ilogtail-docs/about/readme](https://ilogtail.gitbook.io/ilogtail-docs/about/readme)

- 交流群请扫描

![](../.gitbook/assets/chatgroup.png)
