# Table of contents

## 关于 <a href="#about" id="about"></a>

* [什么是LoongCollector](README.md)
* [发展历史](about/brief-history.md)
* [产品优势](about/benefits.md)
* [开源协议](about/license.md)
* [社区版和企业版的对比说明](about/compare-editions.md)

## 安装 <a href="#installation" id="installation"></a>

* [快速开始](installation/quick-start.md)
* [Docker使用](installation/start-with-container.md)
* [Kubernetes使用](installation/start-with-k8s.md)
* [守护进程](installation/daemon.md)
* [目录结构说明](installation/loongcollector-dir.md)
* [iLogtail 兼容模式使用指南](installation/logtail-mode.md)
* [支持的操作系统](installation/os.md)
* 源代码
  * [下载](installation/sources/download.md)
  * [编译](installation/sources/build.md)
  * [Docker镜像](installation/sources/docker-image.md)
  * [编译依赖](installation/sources/dependencies.md)
* 发布记录
  * [LoongCollector 发布记录](installation/release-notes/release-notes.md)
  * [iLogtail 发布记录(2.x版本)](installation/release-notes/release-notes-ilogtail-2x.md)
  * [iLogtail 发布记录(1.x版本)](installation/release-notes/release-notes-ilogtail-1x.md)

## 配置 <a href="#configuration" id="configuration"></a>

* [采集配置](configuration/collection-config.md)
* [系统参数](configuration/system-config.md)
* [日志](configuration/logging.md)

## 插件 <a href="#plugins" id="plugins"></a>

* [概览](plugins/overview.md)
* [版本管理](plugins/stability-level.md)
* 输入
  * [什么是输入插件](plugins/input/README.md)
  * 原生插件
    * [文本日志](plugins/input/native/input-file.md)
    * [容器标准输出](plugins/input/native/input-container-stdio.md)
    * [eBPF网络可观测数据](plugins/input/native/input-network-observer.md)
    * [eBPF进程安全数据](plugins/input/native/input-process-security.md)
    * [eBPF网络安全数据](plugins/input/native/input-network-security.md)
    * [eBPF文件安全数据](plugins/input/native/input-file-security.md)
    * [eBPF网络调用数据](plugins/input/native/metric-observer.md)
  * 扩展插件
    * [容器标准输出](plugins/input/extended/service-docker-stdout.md)
    * [脚本执行数据](plugins/input/extended/input-command.md)
    * [Kubernetes元信息采集](plugins/input/extended/service-kubernetesmeta-v2.md)
    * [主机Meta数据](plugins/input/extended/metric-meta-host.md)
    * [主机监控数据](plugins/input/extended/metric-system.md)
    * [MySQL Binlog](plugins/input/extended/service-canal.md)
    * [GO Profile](plugins/input/extended/service-goprofile.md)
    * [GPU数据](plugins/input/extended/service-gpu.md)
    * [HTTP数据](plugins/input/extended/service-http-server.md)
    * [Journal数据](plugins/input/extended/service-journal.md)
    * [Kafka](plugins/input/extended/service-kafka.md)
    * [SqlServer 查询数据](plugins/input/extended/service-mssql.md)
    * [OTLP数据](plugins/input/extended/service-otlp.md)
    * [PostgreSQL 查询数据](plugins/input/extended/service-pgsql.md)
    * [Syslog数据](plugins/input/extended/service-syslog.md)
    * [【示例】MetricInput](plugins/input/extended/metric-input-example.md)
    * [【示例】ServiceInput](plugins/input/extended/service-input-example.md)
    * [【Debug】Mock数据-Metric](plugins/input/extended/metric-mock.md)
    * [【Debug】Mock数据-Service](plugins/input/extended/service-mock.md)
    * [【Debug】文本日志](plugins/input/extended/metric-debug-file.md)
* 处理
  * [什么是处理插件](plugins/processor/README.md)
  * SPL 处理
    * [SPL 处理](plugins/processor/spl/processor-spl-native.md)
  * 原生插件
    * [正则解析](plugins/processor/native/processor-parse-regex-native.md)
    * [分隔符解析](plugins/processor/native/processor-parse-delimiter-native.md)
    * [Json解析](plugins/processor/native/processor-parse-json-native.md)
    * [时间解析](plugins/processor/native/processor-parse-timestamp-native.md)
    * [过滤](plugins/processor/native/processor-filter-regex-native.md)
    * [脱敏](plugins/processor/native/processor-desensitize-native.md)
  * 扩展插件
    * [添加字段](plugins/processor/extended/processor-add-fields.md)
    * [添加云资产信息](plugins/processor/extended/processor-cloudmeta.md)
    * [原始数据](plugins/processor/extended/processor-default.md)
    * [分隔符](plugins/processor/extended/processor-delimiter.md)
    * [数据脱敏](plugins/processor/extended/processor-desensitize.md)
    * [丢弃字段](plugins/processor/extended/processor-drop.md)
    * [字段加密](plugins/processor/extended/processor-encrypy.md)
    * [条件字段处理](plugins/processor/extended/processor-fields-with-condition.md)
    * [日志过滤](plugins/processor/extended/processor-filter-regex.md)
    * [Go时间格式解析](plugins/processor/extended/processor-gotime.md)
    * [Grok](plugins/processor/extended/processor-grok.md)
    * [Json](plugins/processor/extended/processor-json.md)
    * [日志转SLS Metric](plugins/processor/extended/processor-log-to-sls-metric.md)
    * [otel Metric格式转换](plugins/processor/extended/processor-otel-metric.md)
    * [otel Trace格式转换](plugins/processor/extended/processor-otel-trace.md)
    * [日志限速](plugins/processor/extended/processor-rate-limit.md)
    * [正则](plugins/processor/extended/processor-regex.md)
    * [重命名字段](plugins/processor/extended/processor-rename.md)
    * [键值对](plugins/processor/extended/processor-split-key-value.md)
    * [多行切分](plugins/processor/extended/processor-split-log-regex.md)
    * [字符串替换](plugins/processor/extended/processor-string-replace.md)
* 聚合
  * [什么是聚合插件](plugins/aggregator/README.md)
  * [基础聚合](plugins/aggregator/aggregator-base.md)
  * [按上下文分组](plugins/aggregator/aggregator-context.md)
  * [按Key分组](plugins/aggregator/aggregator-content-value-group.md)
  * [按GroupMetadata分组](plugins/aggregator/aggregator-metadata-group.md)
* 输出
  * [什么是输出插件](plugins/flusher/README.md)
  * 原生插件
    * [SLS](plugins/flusher/native/flusher-sls.md)
    * [【Debug】Blackhole](plugins/flusher/native/flusher-blackhole.md)
    * [多Flusher路由](plugins/flusher/native/router.md)
  * 扩展插件
    * [ClickHouse](plugins/flusher/extended/flusher-clickhouse.md)
    * [ElasticSearch](plugins/flusher/extended/flusher-elasticsearch.md)
    * [HTTP](plugins/flusher/extended/flusher-http.md)
    * [kafkaV2](plugins/flusher/extended/flusher-kafka_v2.md)
    * [Kafka（Deprecated）](plugins/flusher/extended/flusher-kafka.md)
    * [OTLP日志](plugins/flusher/extended/flusher-otlp.md)
    * [Prometheus](plugins/flusher/extended/flusher-prometheus.md)
    * [Pulsar](plugins/flusher/extended/flusher-pulsar.md)
    * [标准输出/文件](plugins/flusher/extended/flusher-stdout.md)
    * [Loki](plugins/flusher/extended/loki.md)
* 扩展
  * [什么是扩展插件](plugins/extension/README.md)
  * [BasicAuth鉴权](plugins/extension/ext-basicauth.md)
  * [协议解码/反序列化](plugins/extension/ext-default-decoder.md)
  * [协议编码/序列化](plugins/extension/ext-default-encoder.md)
  * [数据筛选](plugins/extension/ext-groupinfo-filter.md)
  * [请求熔断](plugins/extension/ext-request-breaker.md)

## 工作原理 <a href="#principle" id="principle"></a>

* [插件系统](principle/plugin-system.md)

## 开发者指南 <a href="#developer-guide" id="developer-guide"></a>

* [开发环境](developer-guide/development-environment.md)
* [代码风格](developer-guide/codestyle.md)
* [数据模型](developer-guide/data-model.md)
* 日志协议
  * [什么是日志协议](developer-guide/log-protocol/README.md)
  * [协议转换](developer-guide/log-protocol/converter.md)
  * [增加新的日志协议](developer-guide/log-protocol/How-to-add-new-protocol.md)
  * 协议
    * [sls协议](developer-guide/log-protocol/protocol-spec/sls.md)
    * [单条协议](developer-guide/log-protocol/protocol-spec/single.md)
    * [raw协议](developer-guide/log-protocol/protocol-spec/raw.md)
* 插件开发
  * [开源插件开发引导](developer-guide/plugin-development/plugin-development-guide.md)
  * 原生插件开发
    * [如何开发原生Input插件](developer-guide/plugin-development/native-plugins/how-to-write-native-input-plugins.md)
    * [如何开发原生Flusher插件](developer-guide/plugin-development/native-plugins/how-to-write-native-flusher-plugins.md)
  * 扩展插件开发
    * [如何开发扩展Input插件](developer-guide/plugin-development/extended-plugins/how-to-write-input-plugins.md)
    * [如何开发扩展Processor插件](developer-guide/plugin-development/extended-plugins/how-to-write-processor-plugins.md)
    * [如何开发扩展Aggregator插件](developer-guide/plugin-development/extended-plugins/how-to-write-aggregator-plugins.md)
    * [如何开发扩展Flusher插件](developer-guide/plugin-development/extended-plugins/how-to-write-flusher-plugins.md)
    * [如何开发Extension插件](developer-guide/plugin-development/extended-plugins/how-to-write-extension-plugins.md)
    * [如何开发外部私有插件](developer-guide/plugin-development/extended-plugins/how-to-write-external-plugins.md)
    * [如何自定义构建产物中默认包含的插件](developer-guide/plugin-development/extended-plugins/how-to-custom-builtin-plugins.md)
    * [插件配置项基本原则](developer-guide/plugin-development/extended-plugins/principles-of-plugin-configuration.md)
  * 插件文档开发
    * [如何生成插件文档](developer-guide/plugin-development/plugin-docs/how-to-genernate-plugin-docs.md)
    * [插件文档规范](developer-guide/plugin-development/plugin-docs/plugin-doc-templete.md)
  * 插件调试
    * [Logger接口](developer-guide/plugin-development/plugin-debug/logger-api.md)
    * [自监控指标接口](developer-guide/plugin-development/plugin-debug/plugin-self-monitor-guide.md)
    * [纯插件模式启动](developer-guide/plugin-development/plugin-debug/pure-plugin-start.md)
* 测试
  * [单元测试](developer-guide/test/unit-test.md)
  * [E2E测试](developer-guide/test/e2e-test.md)
  * [E2E测试——如何添加新的测试行为](developer-guide/test/e2e-test-step.md)
  * [E2E测试——如何编写Subscriber插件](developer-guide/test/How-to-add-subscriber.md)
  * [Benchmark测试](developer-guide/test/benchmark.md)
* 代码检查
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

## 社区活动 <a href="#events" id="event"></a>

* 开源之夏 2024
  * [开源之夏 2024 活动介绍](events/summer-ospp-2024/README.md)
  * 项目
    * [iLogtail 社区项目介绍](events/summer-ospp-2024/projects/README.md)
    * [iLogtail 数据吞吐性能优化](events/summer-ospp-2024/projects/ilogtail-io.md)
    * [ConfigServer 能力升级 + 体验优化（全栈）](events/summer-ospp-2024/projects/config-server.md)

## Awesome iLogtail <a href="#awesome-ilogtail" id="awesome-ilogtail"></a>

* [走近iLogtail社区版](awesome-ilogtail/ilogtail.md)
* [iLogtail社区版使用入门](awesome-ilogtail/getting-started.md)
* [iLogtail社区版开发者指南](awesome-ilogtail/developer-guide.md)
* [iLogtail社区版使用案例](awesome-ilogtail/use-cases.md)
