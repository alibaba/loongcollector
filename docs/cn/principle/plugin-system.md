# 插件系统

> **与当前仓库的关系**：下文中的 libPluginAdaptor、libPluginBase，以及 Input / Processor / Aggregator / Flusher 分层，与今日 **LoongCollector** 主线一致；**插件系统**随主版本演进，是采集 Agent 的核心组成部分。  
> **版本与名称**：**3.x 及之后**的开源采集组件与主进程统称 **LoongCollector**（可执行文件多为 `loongcollector`）；**2.x 及之前**对应交付形态 **iLogtail**。本文在叙述**当前架构与行为**时优先使用 **LoongCollector**（或「主进程」指其 C++ 核心）；仅在说明 **2.x 兼容**、**历史资料**，或引用图中至今仍含 `ilogtail` 等字样的 **URL** 时保留 **iLogtail**。**一个采集配置**允许的 **Input 数量** 与类型约束见 [采集配置说明](../configuration/collection-config.md)。

## 背景

LoongCollector 作为日志服务的采集 Agent，目前已运行于 100W+ 机器上，为万级别的应用提供服务。为融入开源生态并满足持续丰富的接入需求，我们从 **iLogtail（2.x 及以前）** 起演进插件系统；今日 **LoongCollector** 在统一数据模型下接入多种数据源与处理逻辑，已覆盖 HTTP、MySQL 查询、Binlog 等场景（以当前版本已实现的插件为准）。

下文说明插件系统的实现原理、模块组成与若干设计要点。

## 实现原理

本节流程图来自历史公开物料，链接路径中可能仍带有 `ilogtail` 字样，图示内容仍可用于理解 **adaptor / base 与主进程**的协作关系。

![插件系统实现原理图](https://sls-opensource.oss-us-west-1.aliyuncs.com/ilogtail/ilogtail-adapter-cgo.png?versionId=CAEQNBiBgMCl8rPG7RciIDdlMTUwYWE3MTk0YzRkY2ViN2E3MjgxYjlmODQzNDQx)

为支持插件系统，引入 **libPluginAdaptor** 与 **libPluginBase**（下称 adaptor 与 base）两个动态库。它们与 **LoongCollector 主进程**的关系可概括为：

- **主进程**在二进制层面不静态链接上述库；初始化时通过动态加载（如 `dlopen`）尝试载入并解析符号。加载失败时，通常仅影响扩展插件能力，**不阻断**主进程既有的 C++ 采集/发送路径。
- **Adaptor** 位于中间：**主进程**与 **base** 均依赖 adaptor。**主进程**先向 adaptor 注册回调，adaptor 再向 base 暴露接口，使 base 能通过 **SendPb** 等回调把数据交回**主进程侧发送队列**，继而由 **Flusher**（如 `flusher_sls`）等按采集配置（collection pipeline）写出到目标。
- **Base** 承载插件系统主体：采集、处理、聚合，以及经由 adaptor 与主进程协作的投递路径。对插件系统的扩展，本质上是对 **base**（及对应 Go 扩展）的扩展。

结合流程图，典型步骤如下：

1. **主进程**初始化时加载 adaptor，注册回调。
2. **主进程**加载 base，解析 **LoadConfig** 等符号；base 依赖 adaptor，加载过程中会拉起 adaptor。
3. **主进程**从控制面或本地来源读取**采集配置**定义（含插件类型；持续 / 一次性目录见 [采集配置说明](../configuration/collection-config.md)）。
4. **主进程**通过 **LoadConfig** 将配置下发给 base。
5. base 应用配置并拉起各插件，从数据源拉取数据。
6. base 通过 adaptor 将数据递交回**主进程**。
7. **主进程**将数据纳入发送/处理路径，最终由 **Flusher** 等按配置落盘或发送至下游。

上述「主进程」在 **iLogtail 2.x** 时代即已存在同一套分工；**LoongCollector 3.x** 继承该架构并继续迭代。

## 系统组成及设计

本节聚焦 **libPluginBase**，说明插件系统整体架构与部分设计目标。

### 整体架构

![log插件系统整体架构图](https://sls-opensource.oss-us-west-1.aliyuncs.com/ilogtail/logtail-libPluginBase.png?versionId=CAEQMxiBgIDM6YCk6BciIDBjYmVkZjQ2Yjg5NzQwY2NhZjI4MmFmZDA2M2MwZTU2)

针对**每一个采集配置**（collection pipeline），Input、Processor、Aggregator、Flusher 之间的关系如下：

- **Input**：输入层；**同一采集配置**内可并行运行多个 Input，产出交给 Processor。
- **Processor**：处理层；可配置多个，**串行**执行，前一节的输出作为后一节的输入；最后一节输出交给 Aggregator。
- **Aggregator**：聚合层；可按多种策略组批、统计等；可即时刷新到 Flusher，也可按间隔周期刷新。按当前约定，**每个采集配置最多一个** Aggregator（与**多个** Flusher 配合）；详见 [采集配置说明](../configuration/collection-config.md)。
- **Flusher**：输出层；将数据写到文件、标准输出或通过协议发往远端等；**同一采集配置**可配置多个 Flusher，实现多路投递。扩展侧还可通过 adaptor 与**主进程**协作完成投递。

此外，插件系统通过 **Checkpoint**、**Statistics**、**Alarm** 等能力，以 **context** 形式提供给各插件（创建/加载检查点、上报统计与告警等），插件无需关心底层实现细节。

### 系统设计

设计目标概览：

- **易扩展、易开发**：扩展侧以 **Go** 实现。
- **兼容延续**：与 **iLogtail 2.x** 以来形成的配置热更新、Alarm / Statistics 等运维习惯保持一致。
- **通用能力**：为插件提供 **Checkpoint** 等基础能力。

#### 使用 Go 语言实现

- **并发与隔离**：线上常有**多个采集配置**并行运行，Go 的并发模型有利于在进程内隔离各插件实例。
- **动态库**：通过将扩展实现编译为动态库并由**主进程**加载，控制对 C++ 核心的侵入面；这与 **LoongCollector** 大规模部署、渐进升级的需求相符。
- **贴近社区**：Telegraf、Beats 等 Agent 普遍采用 Go，便于社区贡献者上手。
- **Go plugin**：可持续评估 **Go plugin** 等更彻底的动态扩展形态。
- **开发效率**：插件逻辑迭代频繁时，Go 开发效率通常高于**主进程**所用 C++ 核心路径。

### 动态更新配置

在日志服务，采集配置常由控制台下发至机器侧 Agent；本地与自管环境则多依赖 `continuous_pipeline_config` 等目录中的文件热加载。**LoongCollector** 在采集配置变更时需要尽快 _apply_ 插件相关段落，插件系统必须具备**与主进程一致的热更新**能力。

实现上，以**采集配置**为粒度管理各 input、processor 等子插件；**libPluginBase** 提供 **HoldOn / Resume / LoadConfig** 等接口，典型流程为：

1. **主进程**判断变更是否涉及插件系统；若涉及，调用 **HoldOn** 暂停插件侧。
2. **HoldOn** 过程中按采集配置有序停止插件侧状态，并持久化必要检查点。
3. **主进程**调用 **LoadConfig** 加载新增或变更的采集配置定义；新状态覆盖旧状态时，尽量迁移未完成数据，避免丢失。
4. **主进程**调用 **Resume** 恢复运行，热更新结束。

（**iLogtail 2.x** 起已采用同类流程；**LoongCollector** 沿用并扩展。）

### 统一 Statistics、Alarm

**主进程**内建有 Statistics 与 Alarm，便于排障与容量观测。各业务采集配置内的插件也可能产生统计与告警事件，需要与**全局机制**统一。为此，除用户可见的采集配置外，主进程侧还会构造**内部统计 / 告警采集配置**（结构上与普通采集配置类似），专门经 channel 接收各插件上报的统计与告警数据，再经内置 Input、处理链与 Flusher 写回主进程通道。**说明**：这与磁盘上的 `instance_config`（实例级 JSON）不是同一概念；`instance_config` 见 [系统参数](../configuration/system-config.md)。

![image.png](https://sls-opensource.oss-us-west-1.aliyuncs.com/ilogtail/logtail-uni-alarm.png?versionId=CAEQMxiBgMCwsYyk6BciIDZhZDY0OWQ0NTg1ZTQ1YWRhYWNhODRjMDc5NzM4MmJk)

如图所示：右侧为 Statistics / Alarm 两条内部采集配置，结构与普通业务采集配置类似（内置 Input 从 channel 读入，经处理后由 Flusher 交回主进程）；左侧任意业务插件均可向 channel 投递统计或告警事件。

### Checkpoint

为在崩溃、升级、重启后尽量不丢数据，采集进度常依赖检查点。例如 Binlog 类插件需记录文件与位点。插件系统在 **context** 上提供键值及对象形态的 Checkpoint API，例如：

```go
type Context interface {
    SaveCheckPoint(key string, value []byte) error
    GetCheckPoint(key string) (value []byte, exist bool)
    SaveCheckPointObject(key string, obj interface{}) error
    GetCheckPointObject(key string, obj interface{}) (exist bool)

    // ...
}
```
