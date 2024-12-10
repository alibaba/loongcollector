# LoongCollector 发布记录

## Alpha 版（0.2）

经过几个月的努力与技术演进，现在 LoongCollector Alpha 版（ 0.2 ）发布，我们诚挚邀请广大开发者下载试用，并希望基于实际使用体验向我们提供宝贵的反馈意见。感谢您的支持！

LoongCollector 源自阿里云可观测性团队所开源的 iLogtail 项目，在继承了 iLogtail 强大的日志采集与处理能力的基础上，进行了全面的功能升级与扩展。从原来单一日志场景，逐步扩展为可观测数据采集、本地计算、服务发现的统一体。

LoongCollector 是一款集卓越性能、超强稳定性和灵活可编程性于一身的数据采集器，专为构建下一代可观测 Pipeline 设计。愿景是：打造业界领先的“统一可观测 Agent（Unified Observability Agent）”与“端到端可观测 Pipeline（End-to-End Observability Pipeline）”。

### 新特性

1. 从日志到全面数据可观测

    相对于 iLogtail 专注于日志采集，LoongCollector 将实现采集能力的全面升级，做到可观测采集的 OneAgent 化。

    |  Input 插件（含文档）  |  描述  |  状态（含提案，已发布的可不提供）  |
    | --- | --- | --- |
    |  [input\_container\_stdio](../../plugins/input/native/input-container-stdio.md)  |  高性能、高可靠容器标准输出。  |  已发布  |
    |  [service\_kubernetes\_meta](../../plugins/input/extended/service-kubernetesmeta-v2.md)  |  定时采集Kubernetes元数据，包括Pod、Deployment等资源及其之间的关系。  |  已发布   |
    |  input\_prometheus  |  高性能 Prometheus 抓取。  |  开发中 [Prometheus采集能力提案](https://github.com/alibaba/loongcollector/discussions/1920)  |
    |  input\_file\_security  input\_process\_security  input\_network\_security   |  基于 eBPF 采集文件、进程、网络相关安全事件。  |  开发中  |
    |  input\_network\_observer   |  基于 eBPF 采集网络可观测数据。  |  已发布 [网络可观测提案链接](https://github.com/alibaba/loongcollector/discussions/1919)  |
    |  主机指标  |  定时采集主机系统指标（CPU、内存等）和主机、进程元数据（工作目录、进程语言等）  |  开发中 [主机指标提案链接](https://github.com/alibaba/loongcollector/discussions/1921)  |

2. 性能与稳定性

    * K8s 标准输出采集性能、稳定性提升（已发布）
        * 通过重构标准输出采集插件，推出了新版插件 input\_container\_stdio，该插件支持日志轮转队列，显著增强了标准输出采集的稳定性。
        * 在性能方面，新插件表现优异，在 containerd 和 Docker 环境下，单行日志的平均采集速度分别达到了 250MB/s 和 150MB/s，相比旧版新版插件 service\_docker\_stdout，提升了至少100%。
    * 日志典型场景：极简模式、多行切分性能提升 （已发布）
        * 通过 Event 池化与内存零拷贝等手段，对框架组件的性能进行调优。
        * 单行日志与多行日志的平均采集性能分别达到 300MB/s 和 125MB/s，单行日志采集相比 iLogtail 提升了100%。
    * 基础稳定性 （已发布）
        * 支持采集配置热加载隔离，避免了单个采集配置变更导致其它采集配置短时暂停。
        * C++ 处理、发送队列实现 Pipeline 级隔离。

3. 更灵活的编程管道

    * C++ 全面插件化（已发布）：同时提供了充足的组件可供插件自由组合，极大地方便社区新增高性能的输入和输出能力，C++原生插件开发指南详见[如何开发原生Input插件](../../developer-guide/plugin-development/native-plugins/how-to-write-native-input-plugins.md)和[如何开发原生Flusher插件](../../developer-guide/plugin-development/native-plugins/how-to-write-native-flusher-plugins.md)。
    * C++ Input 可使用原生 Processor（已发布）：C++ Input插件能够与原生及扩展的Processor插件配合使用，并支持SPL插件。这意味着C++ Input插件不仅可以利用原生Processor提供的高性能来解析日志，还能通过丰富的扩展Processor功能进一步处理日志，具体详情请参阅文档[什么是处理插件](../../plugins/processor/README.md)和[什么是输入插件](../../plugins/input/README.md)。
    * Golang Input 可使用原生 Processor （开发中）：Go Input 支持多种灵活的数据源输入，而原生处理插件提供了高性能的数据处理。结合两者的优势，可以构建出既高效又能适应多种应用场景的数据处理流水线。详见 [Issue](https://github.com/alibaba/loongcollector/issues/1917)。
    * SPL 处理模式（已发布）：SPL 处理模式支持用户通过 SPL 语句实现对数据的处理。无需编写代码开发插件，极大地拓展了 LoongCollector 可应用的场景。详见文档 [SPL 处理](../../plugins/processor/spl/processor-spl-native.md)。

4. [全新的管控协议](https://github.com/alibaba/loongcollector/blob/main/config_server/protocol/v2/README.md)

    * 协议修订：约定注册、心跳、配置拉取、状态上报等核心管控流程。
    * 功能增强：支持心跳数据压缩，支持配置结果反馈上报，使用标志位支持功能兼容扩展。
    * 管控范围：从采集配置管控扩展为通用常驻任务、一次性任务和进程级配置。
    * 整体进展：协议完成、客户端支持、服务端开发中。 [\[Discussion\] ConfigServer v2 Implementation](https://github.com/alibaba/loongcollector/discussions/1916)

5. [LoongCollector 目录结构优化](../loongcollector-dir.md)（已发布）

    iLogtail 开源初期有较多历史包袱，造成了工作目录结构不清晰。LoongCollector 专门进行了目录结构优化，将配置、日志、运行数据等进行分离。

6. [自监控全面优化](https://github.com/alibaba/loongcollector/discussions/1928)，更完整、清晰展示 Loongcollector 自身运行状态

    针对 Loongcollector 的自监控指标，进行了全面的优化改进，使得自身运行状态的展示更加完整和清晰。用户可以直观地了解到各项指标的实时变化，快速定位潜在问题。此外，通过合理的数据分类和分级展示，用户可以更方便地进行系统调优和资源管理，从而提高整体运行效率。我们将自监控指标改造为了内置 Pipeline，可以将自监控指标自定义发送到不同的位置。

7. [全新的官网](https://open.observability.cn/project/loongcollector/about/#_top) （已发布）

    鉴于 gitbook 网络不稳定，将官网移植到“可观测中文社区”，以便享受更便捷的文档服务。

可观测中文社区作为一个以“运维可观测”为核心的开放、包容、分享的技术社区，旨在聚集运维专家、开发者和爱好者，共同探讨、学习和分享可观测最佳实践与最新技术。

### 限制说明

1. eBPF 相关 Pipeline 支持的最低内核版本为 4.19，支持的架构为 x86 64 位，操作系统支持 Linux。
2. Prometheus 指标数据采集依赖 Operator 提供的 TargetAllocator 能力，暂未开源。
3. Windows 版暂未推出，敬请期待。

### 不兼容变更说明

1. 文件目录布局与文件命名跟 iLogtail 2.0 版本有所变化，如果某些环境对特定目录、文件有所依赖，则需要注意该变化。
2. 部分自监控指标命名跟 iLogtail 2.0 版本不一致，LoongCollector 重新规范了所有自监控指标的命名和上报方式。
3. 开发镜像升级，新增部分依赖库。使用 iLogtail 开发镜像开发 Loongcollector 会出现依赖库链接错误。建议使用loongcollector 开发镜像 2.0.5 版本及以上。

### 版本发布时间

2024.11.27

### Download

| **Filename** | **OS** | **Arch** | **SHA256 Checksum** |
|  ----  | ----  | ----  | ----  |
|[loongcollector-0.2.0.linux-amd64.tar.gz](https://loongcollector-community-edition.oss-cn-shanghai.aliyuncs.com/0.2.0/loongcollector-0.2.0.linux-amd64.tar.gz)|Linux|x86-64|[loongcollector-0.2.0.linux-amd64.tar.gz.sha256](https://loongcollector-community-edition.oss-cn-shanghai.aliyuncs.com/0.2.0/loongcollector-0.2.0.linux-amd64.tar.gz.sha256)|
|[loongcollector-0.2.0.linux-arm64.tar.gz](https://loongcollector-community-edition.oss-cn-shanghai.aliyuncs.com/0.2.0/loongcollector-0.2.0.linux-arm64.tar.gz)|Linux|arm64|[loongcollector-0.2.0.linux-arm64.tar.gz.sha256](https://loongcollector-community-edition.oss-cn-shanghai.aliyuncs.com/0.2.0/loongcollector-0.2.0.linux-arm64.tar.gz.sha256)|

### Docker Image

Docker Pull Command

``` bash
docker pull sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector:0.2.0
```

## iLogtail 版本

[iLogtail 发布记录(2.x版本)](release-notes-ilogtail-2x.md)

[iLogtail 发布记录(1.x版本)](release-notes-ilogtail-1x.md)
