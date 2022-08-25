# Table of contents

## 关于 <a href="#about" id="about"></a>

* [什么是iLogtail](README.md)
* [发展历史](about/brief-history.md)
* [产品优势](about/benefits.md)
* [开源协议](about/license.md)
* [社区版和企业版的对比说明](about/compare-editions.md)

## 安装 <a href="#installation" id="installation"></a>

* [快速开始](installation/quick-start.md)
* [Docker使用](installation/start-with-container.md)
* [Kubernetes使用](installation/start-with-k8s.md)
* [使用Supervised管理](installation/supervised.md)
* [发布记录](installation/release-notes.md)
* [支持的操作系统](installation/os.md)
* [源代码](installation/sources/README.md)
  * [下载](installation/sources/download.md)
  * [编译](installation/sources/build.md)
  * [Docker镜像](installation/sources/docker-image.md)
  * [编译依赖](installation/sources/dependencies.md)
* [镜像站](installation/mirrors.md)

## 概念 <a href="#concepts" id="concepts"></a>

* [关键概念](concepts/key-concepts.md)
* [数据流水线](concepts/data-pipeline.md)

## 配置 <a href="#configuration" id="configuration"></a>

* [采集配置](configuration/collection-config.md)
* [系统参数](configuration/system-config.md)
* [日志](configuration/logging.md)

## 数据流水线 <a href="#data-pipeline" id="data-pipeline"></a>

* [概览](data-pipeline/overview.md)
* [输入](data-pipeline/input/README.md)
  * [文本日志](data-pipeline/input/file-log.md)
  * [容器标准输出](data-pipeline/input/input-docker-stdout.md)
  * [文本日志（debug）](data-pipeline/input/metric-debug-file.md)
  * [MetricInput示例插件](data-pipeline/input/metric-example.md)
  * [主机Meta数据](data-pipeline/input/metric-meta-host.md)
  * [Mock数据-Metric](data-pipeline/input/metric-mock.md)
  * [MySQL Binlog](data-pipeline/input/service-canal.md)
  * [ServiceInput示例插件](data-pipeline/input/service-example.md)
  * [Journal数据](data-pipeline/input/service-journal.md)
  * [Kafka](data-pipeline/input/service-kafka.md)
  * [Mock数据-Service](data-pipeline/input/service-mock.md)
  * [SqlServer 查询数据](data-pipeline/input/service-mssql.md)
  * [PostgreSQL 查询数据](data-pipeline/input/service-pgsql.md)
  * [Syslog数据](data-pipeline/input/service-syslog.md)
* [处理](data-pipeline/processor/README.md)
  * [添加字段](data-pipeline/processor/processor-add-fields.md)
  * [原始数据](data-pipeline/processor/default.md)
  * [丢弃字段](data-pipeline/processor/processor-drop.md)
  * [条件字段处理](data-pipeline/processor/fields-with-condition.md)
  * [日志过滤](data-pipeline/processor/processor-filter-regex.md)
  * [Json](data-pipeline/processor/json.md)
  * [正则](data-pipeline/processor/regex.md)
  * [重命名字段](data-pipeline/processor/processor-rename.md)
  * [分隔符](data-pipeline/processor/delimiter.md)
  * [键值对](data-pipeline/processor/processor-split-key-value.md)
  * [多行切分](data-pipeline/processor/split-log-regex.md)
* [聚合](data-pipeline/aggregator.md)
* [输出](data-pipeline/flusher.md)
  * [Kafka](data-pipeline/flusher/kafka.md)
  * [SLS](data-pipeline/flusher/sls.md)
  * [标准输出/文件](data-pipeline/flusher/stdout.md)
* [加速](data-pipeline/accelerator/README.md)
  * [分隔符加速](data-pipeline/accelerator/delimiter-accelerate.md)
  * [Json加速](data-pipeline/accelerator/json-accelerate.md)
  * [正则加速](data-pipeline/accelerator/regex-accelerate.md)

## 工作原理 <a href="#principle" id="principle"></a>

* [文件发现](principle/file-discovery.md)
* [插件系统](principle/plugin-system.md)

## 可观测性 <a href="#observability" id="observability"></a>

* [日志](observability/logs.md)

## 开发者指南 <a href="#developer-guide" id="developer-guide"></a>

* [开发环境](developer-guide/development-environment.md)
* [代码风格](developer-guide/codestyle.md)
* [数据结构](developer-guide/data-structure.md)
* [插件开发](developer-guide/plugin-development/README.md)
  * [Checkpoint接口](developer-guide/plugin-development/checkpoint-api.md)
  * [Logger接口](developer-guide/plugin-development/logger-api.md)
  * [如何开发Input插件](developer-guide/plugin-development/how-to-write-input-plugins.md)
  * [如何开发Processor插件](developer-guide/plugin-development/how-to-write-processor-plugins.md)
  * [如何开发Aggregator插件](developer-guide/plugin-development/how-to-write-aggregator-plugins.md)
  * [如何开发Flusher插件](developer-guide/plugin-development/how-to-write-flusher-plugins.md)
  * [如何生成插件文档](developer-guide/plugin-development/how-to-genernate-plugin-docs.md)
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

## Awesome iLogtail <a href="#awesome-ilogtail" id="awesome-ilogtail"></a>

* [走近iLogtail社区版](awesome-ilogtail/ilogtail.md)
* [iLogtail社区版使用入门](awesome-ilogtail/getting-started.md)
* [iLogtail社区版开发者指南](awesome-ilogtail/developer-guide.md)
* [iLogtail社区版使用案例](awesome-ilogtail/use-cases.md)
