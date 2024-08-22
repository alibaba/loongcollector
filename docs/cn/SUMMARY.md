# Table of contents

## 关于 <a href="#about" id="about"></a>

* [什么是iLogtail](README.md)
* [发展历史](about/brief-history.md)
* [产品优势](about/benefits.md)
* [开源协议](about/license.md)
* [社区版和企业版的对比说明](about/compare-editions.md)

## 社区活动 <a href="#events" id="event">></a>

* [开源之夏 2024](events/summer-ospp-2024/README.md)
  * [iLogtail 社区项目介绍](events/summer-ospp-2024/projects/README.md)
    * [iLogtail 数据吞吐性能优化](events/summer-ospp-2024/projects/ilogtail-io.md)
    * [ConfigServer 能力升级 + 体验优化（全栈）](events/summer-ospp-2024/projects/config-server.md)

## 安装 <a href="#installation" id="installation"></a>

* [快速开始](installation/quick-start.md)
* [Docker使用](installation/start-with-container.md)
* [Kubernetes使用](installation/start-with-k8s.md)
* [守护进程](installation/daemon.md)
* [发布记录](installation/release-notes.md)
* [发布记录(1.x版本)](installation/release-notes-1.md)
* [支持的操作系统](installation/os.md)
* [源代码](installation/sources/README.md)
  * [下载](installation/sources/download.md)
  * [编译](installation/sources/build.md)
  * [Docker镜像](installation/sources/docker-image.md)
  * [编译依赖](installation/sources/dependencies.md)
* [镜像站](installation/mirrors.md)

## 概念 <a href="#concepts" id="concepts"></a>

* [关键概念](concepts/key-concepts.md)

## 配置 <a href="#configuration" id="configuration"></a>

* [采集配置](configuration/collection-config.md)
* [系统参数](configuration/system-config.md)
* [日志](configuration/logging.md)

## 插件 <a href="#plugins" id="plugins"></a>

* [概览](plugins/overview.md)
* [版本管理](plugins/stability-level.md)
* [输入](plugins/input/README.md)
  * [文本日志](plugins/input/input-file.md)
  * [容器标准输出（原生插件）](plugins/input/input-container-stdlog.md)
  * [脚本执行数据](plugins/input/input-command.md)
  * [容器标准输出](plugins/input/service-docker-stdout.md)
  * [文本日志（debug）](plugins/input/metric-debug-file.md)
  * [MetricInput示例插件](plugins/input/metric-input-example.md)
  * [主机Meta数据](plugins/input/metric-meta-host.md)
  * [Mock数据-Metric](plugins/input/metric-mock.md)
  * [eBPF网络调用数据](plugins/input/metric-observer.md)
  * [主机监控数据](plugins/input/metric-system.md)
  * [MySQL Binlog](plugins/input/service-canal.md)
  * [GO Profile](plugins/input/service-goprofile.md)
  * [GPU数据](plugins/input/service-gpu.md)
  * [HTTP数据](plugins/input/service-http-server.md)
  * [ServiceInput示例插件](plugins/input/service-input-example.md)
  * [Journal数据](plugins/input/service-journal.md)
  * [Kafka](plugins/input/service-kafka.md)
  * [Mock数据-Service](plugins/input/service-mock.md)
  * [SqlServer 查询数据](plugins/input/service-mssql.md)
  * [OTLP数据](plugins/input/service-otlp.md)
  * [PostgreSQL 查询数据](plugins/input/service-pgsql.md)
  * [Syslog数据](plugins/input/service-syslog.md)
  * [eBPF文件安全数据](plugins/input/input-ebpf-file-security.md)
  * [eBPF网络可观测数据](plugins/input/input-ebpf-network-observer.md)
  * [eBPF网络安全数据](plugins/input/input-ebpf-network-security.md)
  * [eBPF进程安全数据](plugins/input/input-ebpf-process-security.md)
* [处理](plugins/processor/README.md)
  * [原生插件](plugins/processor/native/README.md)
    * [正则解析](plugins/processor/native/processor-parse-regex-native.md)
    * [分隔符解析](plugins/processor/native/processor-parse-delimiter-native.md)
    * [Json解析](plugins/processor/native/processor-parse-json-native.md)
    * [时间解析](plugins/processor/native/processor-parse-timestamp-native.md)
    * [过滤](plugins/processor/native/processor-filter-regex-native.md)
    * [脱敏](plugins/processor/native/processor-desensitize-native.md)
  * [扩展插件](plugins/processor/extended/README.md)
    * [添加字段](plugins/processor/extended/processor-add-fields.md)
    * [添加云资产信息](plugins/processor/extended/processor-cloudmeta.md)
    * [原始数据](plugins/processor/extended/processor-default.md)
    * [数据脱敏](plugins/processor/extended/processor-desensitize.md)
    * [丢弃字段](plugins/processor/extended/processor-drop.md)
    * [字段加密](plugins/processor/extended/processor-encrypy.md)
    * [条件字段处理](plugins/processor/extended/processor-fields-with-condition.md)
    * [日志过滤](plugins/processor/extended/processor-filter-regex.md)
    * [Go时间格式解析](plugins/processor/extended/processor-gotime.md)
    * [Grok](plugins/processor/extended/processor-grok.md)
    * [Json](plugins/processor/extended/processor-json.md)
    * [日志转SLS Metric](plugins/processor/extended/processor-log-to-sls-metric.md)
    * [正则](plugins/processor/extended/processor-regex.md)
    * [重命名字段](plugins/processor/extended/processor-rename.md)
    * [分隔符](plugins/processor/extended/processor-delimiter.md)
    * [键值对](plugins/processor/extended/processor-split-key-value.md)
    * [多行切分](plugins/processor/extended/processor-split-log-regex.md)
    * [字符串替换](plugins/processor/extended/processor-string-replace.md)
* [聚合](plugins/aggregator/README.md)
  * [基础](plugins/aggregator/aggregator-base.md)
  * [上下文](plugins/aggregator/aggregator-context.md)
  * [按Key分组](plugins/aggregator/aggregator-content-value-group.md)
  * [按GroupMetadata分组](plugins/aggregator/aggregator-metadata-group.md)
* [输出](plugins/flusher/README.md)
  * [Kafka（Deprecated）](plugins/flusher/flusher-kafka.md)
  * [kafkaV2](plugins/flusher/flusher-kafka_v2.md)
  * [ClickHouse](plugins/flusher/flusher-clickhouse.md)
  * [ElasticSearch](plugins/flusher/flusher-elasticsearch.md)
  * [SLS](plugins/flusher/flusher-sls.md)
  * [标准输出/文件](plugins/flusher/flusher-stdout.md)
  * [OTLP日志](plugins/flusher/flusher-otlp.md)
  * [Pulsar](plugins/flusher/flusher-pulsar.md)
  * [HTTP](plugins/flusher/flusher-http.md)
  * [Loki](plugins/flusher/loki.md)
* [扩展](plugins/extension/README.md)
  * [BasicAuth鉴权](plugins/extension/ext-basicauth.md)
  * [协议解码/反序列化](plugins/extension/ext-default-decoder.md)
  * [协议编码/序列化](plugins/extension/ext-default-encoder.md)
  * [数据筛选](plugins/extension/ext-groupinfo-filter.md)
  * [请求熔断](plugins/extension/ext-request-breaker.md)

## 工作原理 <a href="#principle" id="principle"></a>

* [文件发现](principle/file-discovery.md)
* [插件系统](principle/plugin-system.md)

## 可观测性 <a href="#observability" id="observability"></a>

* [日志](observability/logs.md)

## 开发者指南 <a href="#developer-guide" id="developer-guide"></a>

* [开发环境](developer-guide/development-environment.md)
* [日志协议](developer-guide/log-protocol/README.md)
  * [协议转换](developer-guide/log-protocol/converter.md)
  * [增加新的日志协议](developer-guide/log-protocol/How-to-add-new-protocol.md)
  * [协议](developer-guide/log-protocol)
    * [sls协议](developer-guide/log-protocol/protocol-spec/sls.md)
    * [单条协议](developer-guide/log-protocol/protocol-spec/single.md)
* [代码风格](developer-guide/codestyle.md)

* [数据模型](developer-guide/data-model.md)
* [插件开发](developer-guide/plugin-development/README.md)
  * [开源插件开发引导](docs/cn/developer-guide/plugin-development/plugin-development-guide.md)
  * [Checkpoint接口](developer-guide/plugin-development/checkpoint-api.md)
  * [Logger接口](developer-guide/plugin-development/logger-api.md)
  * [自监控指标接口](developer-guide/plugin-development/plugin-self-monitor-guide.md)
  * [如何开发Input插件](developer-guide/plugin-development/how-to-write-input-plugins.md)
  * [如何开发Processor插件](developer-guide/plugin-development/how-to-write-processor-plugins.md)
  * [如何开发Aggregator插件](developer-guide/plugin-development/how-to-write-aggregator-plugins.md)
  * [如何开发Flusher插件](developer-guide/plugin-development/how-to-write-flusher-plugins.md)
  * [如何生成插件文档](developer-guide/plugin-development/how-to-genernate-plugin-docs.md)
  * [插件文档规范](docs/cn/developer-guide/plugin-development/plugin-doc-templete.md)
  * [纯插件模式启动](developer-guide/plugin-development/pure-plugin-start.md)
* [测试](developer-guide/test/README.md)
  * [单元测试](developer-guide/test/unit-test.md)
  * [E2E测试](developer-guide/test/e2e-test.md)
* [代码检查](developer-guide/code-check/README.md)
  * [检查代码规范](developer-guide/code-check/check-codestyle.md)
  * [检查文件许可证](developer-guide/code-check/check-license.md)
  * [检查依赖包许可证](developer-guide/code-check/check-dependency-license.md)

## 贡献指南 <a href="#controbuting-guide" id="controbuting-guide"></a>

* [贡献指南](contributing/CONTRIBUTING.md)
* [开发者](contributing/developer.md)
* [成就](contributing/achievement.md)

## 性能测试 <a href="#benchmark" id="benchmark"></a>

* [容器场景iLogtail与Filebeat性能对比测试](benchmark/performance-compare-with-filebeat.md)

## 管控工具 <a href="#config-server" id="config-server"></a>

* [使用介绍](config-server/quick-start.md)
* [通信协议](config-server/communication-protocol.md)
* [开发指南](config-server/developer-guide.md)

## Awesome iLogtail <a href="#awesome-ilogtail" id="awesome-ilogtail"></a>

* [走近iLogtail社区版](awesome-ilogtail/ilogtail.md)
* [iLogtail社区版使用入门](awesome-ilogtail/getting-started.md)
* [iLogtail社区版开发者指南](awesome-ilogtail/developer-guide.md)
* [iLogtail社区版使用案例](awesome-ilogtail/use-cases.md)
