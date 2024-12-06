# 产品优势

在可观测领域，Fluent Bit、OpenTelemetry Collector 及 Vector 都是备受推崇的可观测数据采集器。其中，FluentBit 小巧精悍，以性能著称；OpenTelemetry Collector 背靠 CNCF，借助 Opentelemetry 概念构建了丰富的生态体系；而 Vector 在 Datadog 加持下，则通过 Observability Pipelines 与 VRL 的组合，为数据处理提供了新的选择。

LoongCollector 则立足日志场景，通过持续完善指标、跟踪等场景实现更全面的 OneAgent 采集能力；依托性能、稳定性、Pipeline灵活性、可编程性优势，打造核心能力的差异化；同时，借助强大的管控能力，提供了大规模采集配置管理能力。更多详见下表，绿色部分为优势项。

| 大类 | 子类 | LoongCollector | FluentBit | OpenTelemetry Collector | Vector |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 采集能力 | 日志 | 强。<br/>采集、处理插件丰富。尤其是K8s友好，在Stdout采集、AutoTagging方面表现优异。 | 强 | 中 | 中 |
| | 指标 | 较强。<br/>主机等场景原生支持、Prometheus抓取。<br/>后续通过eBPF能力持续增强。 | 中。刚起步。 | 较强。<br/>数据源较全，但是较多处于Alpha阶段。 | 中 |
| | 跟踪 | 中。主要作为代理场景。 | 中。主要作为代理场景。 | 强 | 中 |
| 性能与可靠性 | 性能与资源开销 | 性能：高。日志场景极简单核200M/s。<br/>资源开销：低 | 性能：高<br/>资源开销：低 | 性能：中<br/>资源开销：高 | 性能：中<br/>资源开销：中 |
| | 可靠性 | 完善的checkpoint机制<br/>多级高低水位反馈队列<br/>多租隔离<br/>整体资源控制 | 可选的磁盘缓冲队列<br/>完善的checkpoint机制 | 可选的磁盘缓冲队列<br/>插件统一发送重试框架 | 缓冲区模型<br/>事件确认机制 |
| 可编程能力 | 插件开发语言 | C++、Go | C++、Go、Lua、WebAssembly | Go | Rust |
| | 高级处理语法 | SPL 处理/编排能力强、性能高 | 基于 SQL 的 Stream Processor | OpenTelemetry Transformation Language（OTTL） | VRL |
| | Pipeline 能力 | 多语言 Pipeline，可组合性高 | 基于 Tag Match 实现 | 基于 Connector 插件 | 基于 Inputs 参数指定上游插件 |
| 管控 | 全局管控 | 开放的管控协议<br/>支持机器组、心跳管理<br/>配置热加载能力<br/>ConfigServer实现 | 无 | OpAMP Server | 无 |
| | K8s Operator<br/>与 CRD | 商业版支持，开源敬请期待 | Fluent Operator  | OpenTelemetry Operator | 无 |
