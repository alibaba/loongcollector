# 输出插件

输出插件（Flusher）位于采集配置末端，负责将聚合后的 **Event / EventGroup** 写出到下游：日志服务、消息队列、时序库、搜索与分析引擎、本地文件或标准输出等。**每个采集配置**必须至少配置 **一个** Flusher；也可配置 **多个** Flusher，实现**一条数据**多路投递。

LoongCollector 提供两类输出插件：

- **原生输出插件（C++）**：与核心进程同构，路径短、默认集成度高，适合作为云上默认链路与高稳定场景。
- **扩展输出插件（Golang）**：由插件运行时加载，易于对接开源与自建系统，功能覆盖面广。

## 插件类型介绍

### 原生插件

原生 Flusher 采用 C++ 实现，常见特点：

- 与主进程共享运行时，默认链路上延迟与资源占用更可控
- 适合云上默认写入（如 `flusher_sls`）、调试丢弃（`flusher_blackhole`）、本地落盘（`flusher_file`）、Kafka C++ 实现（`flusher_kafka_native`）等
- **路由** 并非独立「网络输出」，而是由 `router` 与若干支持分流的 Flusher 协同完成多路输出；适用条件见 [多 Flusher 路由](native/router.md)

| 名称 | 提供方 | 功能简介 |
| --- | --- | --- |
| `flusher_blackhole`<br>[黑洞](native/flusher-blackhole.md) | SLS 官方 | 丢弃事件，常用于压测或占位。 |
| `flusher_file`<br>[本地文件](native/flusher-file.md) | SLS 官方 | 将数据写入本地文件（如自监控指标落盘）。 |
| `flusher_kafka_native`<br>[Kafka](native/flusher-kafka.md) | [ChaoEcho](https://github.com/ChaoEcho) | 使用 C++ 实现将数据输出到 Kafka。 |
| `flusher_sls`<br>[SLS](native/flusher-sls.md) | SLS 官方 | 将数据写入阿里云日志服务（SLS）。 |
| `router`<br>[多 Flusher 路由](native/router.md) | SLS 官方 | 在原生处理链路与支持的 Flusher 上按事件类型或 Tag 分流。 |

上表按插件 **Type** 字典序排列。

### 扩展插件

扩展 Flusher 基于 Golang 实现，特点包括：

- 对接种类多（HTTP、ClickHouse、Doris、ElasticSearch、Kafka、Loki、OTLP、Prometheus Remote Write、Pulsar 等）
- 参数与批处理、重试、认证等多在各篇文档中单独说明，需按目标系统调优
- 与 **扩展机制**（如 `ext_basicauth`、`ext_request_breaker` 等）组合使用时，见 [插件概览 - 扩展](../overview.md#扩展)

| 名称 | 提供方 | 功能简介 |
| --- | --- | --- |
| `flusher_clickhouse`<br>[ClickHouse](extended/flusher-clickhouse.md) | 社区<br>[kl7sn](https://github.com/kl7sn) | 写入 ClickHouse。 |
| `flusher_doris`<br>[Apache Doris](extended/flusher-doris.md) | 社区 | Stream Load 写入 Apache Doris。 |
| `flusher_elasticsearch`<br>[ElasticSearch](extended/flusher-elasticsearch.md) | 社区<br>[joeCarf](https://github.com/joeCarf) | 写入 ElasticSearch。 |
| `flusher_http`<br>[HTTP](extended/flusher-http.md) | 社区<br>[snakorse](https://github.com/snakorse) | 以 HTTP 方式推送到自定义后端。 |
| `flusher_kafka`<br>[Kafka](extended/flusher-kafka.md) | 社区 | 输出到 Kafka（旧版实现）。 |
| `flusher_kafka_v2`<br>[Kafka V2](extended/flusher-kafka-v2.md) | 社区<br>[shalousun](https://github.com/shalousun) | 输出到 Kafka，优先推荐。 |
| `flusher_loki`<br>[Loki](extended/flusher-loki.md) | 社区<br>[abingcbc](https://github.com/abingcbc) | 推送到 Grafana Loki。 |
| `flusher_otlp_log`<br>[OTLP 日志](extended/flusher-otlp.md) | 社区<br>[liuhaoyang](https://github.com/liuhaoyang) | 兼容 OpenTelemetry Logs 的后端。 |
| `flusher_prometheus`<br>[Prometheus](extended/flusher-prometheus.md) | 社区 | Remote Write 写入 Prometheus 兼容时序库。 |
| `flusher_pulsar`<br>[Pulsar](extended/flusher-pulsar.md) | 社区<br>[shalousun](https://github.com/shalousun) | 写入 Apache Pulsar。 |
| `flusher_stdout`<br>[标准输出/文件](extended/flusher-stdout.md) | SLS 官方 | 标准输出或文件，便于本地调试与排障。 |

上表按插件 **Type** 字典序排列。完整稳定性等级与其它交叉引用仍以 [插件概览](../overview.md) 为准。

## 插件特性对比

| 特性 | 原生 Flusher | 扩展 Flusher |
| --- | --- | --- |
| 实现语言 | C++ | Golang |
| 典型用途 | 默认上云、路由、本地文件/黑洞、C++ Kafka | 开源栈与自定义 HTTP/SDK 对接 |
| 加载方式 | 核心内建 | 插件运行时动态加载 |
| 与 router 配合 | `router` 仅在与原生处理链及支持的 Flusher 组合下生效 | 视具体 Flusher 是否支持 `Match` 等字段，见单篇文档 |

## 使用说明

- **必填**：**每个采集配置**的 `flushers` 列表至少包含一项；Aggregation 完成后数据由 Flusher 消费并写出。一级字段与约束见 [采集配置](../../configuration/collection-config.md)。
- **与 Input / Processor 的衔接**：仅扩展 Input 时，处理链须遵守 [处理插件](../processor/processors.md) 中的组合规则（仅扩展 Processor 或级联模式等）；Flusher 侧通常同时受数据形态与下游协议影响，需在单篇 Flusher 文档中核对 **TelemetryType**、**Format** 等参数。
- **路由**：需要按事件类型或 Tag 拆流到不同下游时，在支持的场景下使用 [router](native/router.md)，并为各 Flusher 配置匹配规则。
- **认证与加密**：云上 AK/SK 等常通过 [系统参数](../../configuration/system-config.md) 与 `flusher_sls` 配合；HTTP 类扩展 Flusher 可使用概览中的 **扩展** 表内认证、熔断等能力。

## 选型建议

1. **写入阿里云 SLS**：优先使用原生 `flusher_sls`，并正确配置 Endpoint、Project、Logstore（及 `TelemetryType` 等）。
2. **本地调试或 CI**：使用 `flusher_stdout` 观察序列化结果；仅需吞吐压测、不需下游时使用 `flusher_blackhole`。
3. **Kafka**：新部署优先评估 `flusher_kafka_v2`；若需与核心进程最短路径对接，可了解 `flusher_kafka_native`（C++）。
4. **ClickHouse / Doris / ES / Loki / Pulsar / HTTP / OTLP / Prometheus**：选用对应扩展 Flusher，并按目标系统的批大小、重试、TLS、认证段落逐项配置。
5. **多副本、多环境投递**：在符合条件时用 `router` 配合多个 Flusher，避免在应用外再写多套采集配置重复采同一源（仍以实际支持与单篇说明为准）。
