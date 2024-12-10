# 什么是LoongCollector

![logo](<https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/readme/loongcollector-icon.png>)

LoongCollector 是一款集卓越性能、超强稳定性和灵活可编程性于一身的数据采集器，专为构建下一代可观测 Pipeline 设计。源自阿里云可观测性团队所开源的 iLogtail 项目，在继承了 iLogtail 强大的日志采集与处理能力的基础上，进行了全面的功能升级与扩展。从原来单一日志场景，逐步扩展为可观测数据采集、本地计算、服务发现的统一体。

[![GitHub contributors](https://img.shields.io/github/contributors/alibaba/ilogtail)](https://github.com/alibaba/loongcollector/contributors)
[![GitHub stars](https://img.shields.io/github/stars/alibaba/ilogtail)](https://github.com/alibaba/loongcollector/stargazers)
[![GitHub issues](https://img.shields.io/github/issues/alibaba/ilogtail)](https://github.com/alibaba/loongcollector/issues)
[![GitHub license](https://img.shields.io/github/license/alibaba/ilogtail)](https://github.com/alibaba/loongcollector/blob/main/LICENSE)
[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/28764.svg)](https://scan.coverity.com/projects/alibaba-ilogtail)
[![Coverage Status](https://codecov.io/gh/alibaba/ilogtail/branch/main/graph/badge.svg)](https://codecov.io/gh/alibaba/ilogtail)
[![Go Report Card](https://goreportcard.com/badge/github.com/alibaba/loongcollector)](https://goreportcard.com/report/github.com/alibaba/loongcollector)

## 品牌寓意

LoongCollector，灵感源于东方神话中的“中国龙”形象，Logo 中两个字母 O 犹如龙灵动的双眼，充满灵性。

龙的眼睛具有敏锐的洞察力，正如 LoongCollector 能够全面精准地采集和解析每一条可观测数据；龙的灵活身躯代表了对多变环境高度的适应能力，映射出 LoongCollector 广泛的系统兼容性与灵活的可编程性，可以满足各种复杂的业务需求；龙的强大力量与智慧象征了在高强度负载下卓越的性能和无与伦比的稳定性。最后，期待 LoongCollector 犹如遨游九天的中国龙，不断突破技术边界，引领可观测采集的新高度。

## 核心优势

LoongCollector 社区将紧密围绕既定的愿景蓝图，专注于核心价值与竞争力提升。主要表现在如下方面：降低机器资源成本，提高稳定性；打造畅通的数据链路，丰富上下游；增益数据价值，自动贴标，灵活处理；降低接入运维人力成本，易配置，有管控。

### 性能可靠，无懈可击 Uncompromised Performance and Reliability

LoongCollector 始终将追求极致的采集性能和超强可靠性放在首位，坚信这是实践长期主义理念的根基。我们深知，LoongCollector 核心价值在于为大规模分布式系统提供稳固、高效的可观测性数据统一采集 Agent 与端到端 Pipeline。不管在过去、现在、未来，LoongCollector 都将持续通过技术革新与优化，实现资源利用效率的提升与在极端场景下的稳定运行。

![uncompromised_performance_and_reliability](<https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/readme/uncompromised_performance_and_reliability.png>)

### 遥测数据，无限边界 Unlimited Telemetry Data

![unlimited_telemetry_data](<https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/readme/unlimited_telemetry_data.png>)

LoongCollector 坚信 All-in-One 的设计理念，致力于所有的采集工作用一个 Agent 实现 Logs、Metric、Traces、Events、Profiles 的采集、处理、路由、发送等功能。展望未来，LoongCollector 将着重强化其 Prometheus 抓取能力，深度融入 eBPF（Extended Berkeley Packet Filter）技术以实现无侵入式采集，提供原生的指标采集功能，做到真正的 OneAgent。

同时，秉承开源、开放的原则，积极拥抱 OpenTelemetry、Prometheus 在内的开源标准。开源两年以来，也收获了 OpenTelemetry Flusher、ClickHouse Flusher、Kafka Flusher 等众多开源生态对接能力。作为可观测基础设施，LoongCollector 不断致力于完善在异构环境下的兼容能力，并积极致力于实现对主流操作系统环境的全面且深度的支持。

K8s 采集场景的能力一直都是 LoongCollector 的核心能力所在。众所周知在可观测领域，K8s 元数据（例如 Namespace、Pod、Container、Labels 等）对于可观测数据分析往往起着至关重要的作用。LoongCollector 基于标准 CRI API 与 Pod 的底层定义进行交互，实现 K8s 下各类元数据信息获取，从而无侵入的实现采集时的 K8s 元信息 AutoTagging 能力。

### 编程管道，无与伦比 Unrestricted Programmable Pipeline

LoongCollector 通过 SPL 与多语言 Plugin 双引擎加持，构建完善的可编程体系。

* 不同引擎都可以相互打通，通过灵活的组合实现预期的计算能力。
* 设计通用的 Event 数据模型，可扩展表达 Logs、Metric、Traces、Events、Profiles 等在内的多种可观测类型，为通用计算提供便捷。

![unrestricted_programmable_pipeline](<https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/readme/unrestricted_programmable_pipeline.png>)

开发者可以根据自身需求灵活选择可编程引擎。如果看重执行效率，可以选择原生插件；如果看重算子全面性，需要处理复杂数据，可以选择 SPL 引擎；如果强调低门槛的自身定制化，可以选择扩展插件，采用 Golang 进行编程。

| 可编程引擎 | 分类 | 特点 | 开发门槛 |
| -- | -- | -- | -- |
| 多语言 Plugin 引擎 | 原生插件 | C++实现<br>性能高，资源开销极低<br>较完善的算子能力 | C++，开发门槛中等<br>可灵活定制 |
| | 扩展插件 | Golang实现<br>较高的性能，资源开销低<br>较完善的算子能力 | Golang，开发门槛低<br>可灵活定制 |
| SPL 引擎 | SPL 引擎 | C++实现<br>列式模型，向量化执行<br>性能高，资源开销低<br>全面的算子能力<br>管道式设计，灵活组合，可以处理复杂数据 | 暂未开源<br>强调无代码，通过语法能力可解决大部分问题。 |

### 配置管理，无忧无虑 Unburdened Config Management

在分布式系统复杂的生产环境中，管理成千上万节点的配置接入是一项严峻挑战，这尤其凸显了在行业内缺乏一套统一且高效的管控规范的问题。针对这一痛点，LoongCollector 社区设计并推行了一套详尽的 Agent 管控协议。此协议旨在为不同来源与架构的 Agent 提供一个标准化、可互操作的框架，从而促进配置管理的自动化。

在此基础上，社区进一步研发 ConfigServer 服务平台实现 ConfigServer 服务，可以管控任意符合该协议的 Agent。这一机制显著提升了大规模分布式系统中配置策略的统一性、实时性和可追溯性。ConfigServer 作为一款可观测 Agent 的管控服务，支持以下功能：

* 以 Agent 组的形式对采集 Agent 进行统一管理。
* 远程批量配置采集 Agent 的采集配置。
* 监控采集 Agent 运行状态，汇总告警信息。

同时，对于存储适配层进行了抽象，便于开发者对接符合自己环境需求的持久化存储。

![config_server](<https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/readme/config_server.png>)

LoongCollector 极大地完善了自身可观测性的建设。不管是 LoongCollector 自身运行状态，还是采集 Pipeline 节点都有完整指标。开发者只需要将这些指标对接到可观测系统，即可体验对 LoongCollector 运行状态的清晰洞察。

![self_monitor](<https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/readme/self_monitor.png>)

## 核心场景：不仅仅是 Agent

![not_only_agent](<https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/readme/not_only_agent.png>)

作为一款高性能的可观测数据采集与处理 Pipeline，LoongCollector 的部署模式在很大程度上能够被灵活定制以满足各种不同的业务需求和技术架构。

* Agent 模式：As An Agent
  * LoongCollector 作为 Agent 运行在各类基础架构节点上，包括主机或 K8s 环境。每个 LoongCollector 实例专注于采集所在节点的多维度可观测性数据。
  * 可以充分利用本地计算资源，实现在数据源头的即时处理，降低了数据传输带来的延迟和网络流量，提升了数据处理的时效性。
  * 具备随节点动态扩展的自适应能力，确保在集群规模演变时，可观测数据的采集与处理能力无缝弹性伸缩。
* 集群模式：As A Service
  * LoongCollector 部署于一个或多个核心数据处理节点，以多副本部署及支持扩缩容，用于接收来自系统内 Agent 或 开源协议的数据，并进行转换、汇集等操作。
  * 作为中心化服务，便于掌握整个系统的上下文，强化了集群元数据的关联分析能力，为深入理解系统状态与数据流向奠定了基础。
  * 作为集中式服务枢纽，提供 Prometheus 指标抓取等集群数据抓取和处理能力。
* 轻量流计算模式：As A Stream Consumer
  * LoongCollector 与消息队列配合，利用消息队列的天然缓冲特性实现数据流的平滑处理，有效应对流量峰值与低谷，保障了对数据流的实时捕获、灵活处理及高效分发能力。
  * 借助 SPL 或多语言 Plugin 引擎的处理能力，使轻量级的数据处理、流式数据聚合、过滤、分发成为可能。

## 快速开始

由于C++编译环境较为复杂，LoongCollector的编译依赖docker。如果想从源码编译LoongCollector，可以执行下面的命令：

``` bash
make
cp example/quick_start/* output
cd output
./loongcollector
# 现在LoongCollector已经开始采集output/simple.log文件并输出到标准输出了
```

如果你对细节感兴趣，请参见文档[编译](installation/sources/build.md)。

## RoadMap

未来，LoongCollector 社区将持续围绕长期主义进行建设，打造核心竞争力。同时，也期待更多小伙伴的加入。

![roadmap](<https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/readme/roadmap.png>)

* 通过框架能力增强，构建高性能、高可靠的基础底座。
  * 通用发送重构框架
  * Golang & C++ Pipeline 全链路打通
  * 多目标发送 Fan-out 能力
  * 采集配置独立加载
  * 不同发送目标的流水线间故障隔离
  * 不同类型输入插件的流水线间根据优先级调度
  * 整体内存控制，防止 OOM 和加剧系统不稳定
* 通过采集能力丰富，打造 All In One 采集器。
  * Stdout C++版日志采集上线
  * 文件采集插件支持多路径
  * eBPF 支持网络监控、HTTP监控
  * eBPF 支持进程指纹采集
  * eBPF 支持 Profiling
  * 原生 Node 级指标采集能力
  * Prometheus Exporter 指标抓取能力完善
* 通过可编程性，提升通用计算能力。
  * Tag 处理能力
  * Golang 插件 V2 Pipeline 补齐
  * 通用 logtometric 插件
  * SPL 支持 Agg 等更多通用算子
* 通过管控与可观测能力，提升易用性。
  * 全新 Configserver 管控协议
  * 托管版 Configserver
  * 开源版 Configserver 扩展更多分布式能力
  * 框架类（队列资源、线程数等）指标完善
  * 可观测数据支持 Prometheus Exporter
* 下游生态支持：与更多开源消息队列、存储分析系统集成。

## 贡献

* [贡献指南](contributing/CONTRIBUTING.md)
* [开发者](contributing/developer.md)
* [成就](contributing/achievement.md)

## 我们的用户

目前来自阿里云、石墨文档、同程旅行、小红书、字节跳动、哔哩哔哩、嘀嗒出行的多位同学在参与 LoongCollector 社区的共建。

## Licence

[Apache 2.0 许可证](about/license.md)

## 联系我们

您可以通过[Github Issues](https://github.com/alibaba/loongcollector/issues) 或 [Github Discussions](https://github.com/alibaba/loongcollector/discussions) 报告bug、提出建议或参与讨论。也可以通过以下方式联系我们：

* 微信公众号：阿里云可观测
* 哔哩哔哩：[阿里云可观测](https://space.bilibili.com/630680534)
* 知乎：[iLogtail社区](https://www.zhihu.com/column/c_1533139823409270785)
* 扫描二维码加入微信/钉钉交流群

![chatgroup](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/chatgroup/chatgroup.png)
