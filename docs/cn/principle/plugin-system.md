# 插件系统

## 背景

作为阿里云日志服务的采集 Agent ， LoongCollector （原 iLogtail） 目前已运行于 100W+ 机器上，为万级别的应用提供服务，为了能够更好地实现与开源生态的融合并满足用户不断丰富的接入需求，我们为当时的 iLogtail 引入了插件系统，支持了若干数据源，包括 HTTP、MySQL Query、MySQL Binlog 等。

iLogtail 的插件系统完全使用 Go 语言实现（现在这些插件叫作**扩展插件**），通过动态库与 iLogtail C++ 本体交互，虽然易于扩展和开发，但是却牺牲了性能，且和 C++ 本体存在一些行为和定义上的不一致，例如部分功能原本在 C++ 模块实现，可一旦使用插件系统，就不得不在 Go 模块以插件的形式重新实现一遍，带来了重复的开发不说，维护两边功能的一致性也是一个挑战（例如[sls 协议](../developer-guide/log-protocol/protocol-spec/sls.md)中 Tag 的处理，Go 的[正则解析插件](plugins/processor/extended/processor-regex.md)是对原 iLogtail 的 C++ 部分解析能力的复制）。

因此，在 LoongCollector 中，我们对插件系统进行了升级，将 C++ 模块的主要功能也实现了插件化（现在这些插件叫作**原生插件**），例如[文件采集插件](../plugins/input/native/input-file.md)。此外，我们还将很多原先常用的 Go 插件实现了 C++ 版本，对性能和可靠性进行了升级，例如[容器标准输出](../plugins/input/native/input-container-stdio.md)、[正则解析](../plugins/processor/native/processor-parse-regex-native.md)等。在 LoongCollector 中，C++ 插件与 Go 插件可以混合搭配使用，从而实现了一个完整的插件系统。

本文将对 LoongCollector 插件系统进行简要介绍，包括插件系统的实现原理、插件系统的组成及设计。

## 整体介绍

LoongCollector 的插件分为五类：

- [Input 插件](../plugins/input/inputs.md)：负责数据采集/接收。
- [Processor 插件](../plugins/processor/processors.md)：负责数据处理。
- [Aggregator 插件](../plugins/aggregator/aggregators.md)：负责数据聚合，仅在使用扩展输出插件（Go Flusher 插件）时存在。
- [Flusher 插件](../plugins/flusher/flushers.md)：负责数据输出。
- [Extension 插件](../plugins/extension/extensions.md)：在其他插件中使用，增加特殊功能。

数据流转的链路为：Input -> Processors -> Aggregator -> Flusher。目前，仅支持从 C++ 传输数据至 Go 插件，反之还未实现，但是支持已经在计划中，参见社区 Discussion：[C++ Pipeline 需求和演进](https://github.com/alibaba/loongcollector/discussions/855)。距离来说，目前的插件系统能力：

- 【支持】C++ Input -> C++ Processors -> C++ Flusher
- 【支持】C++ Input -> C++ Processors -> Go Aggregator -> Go Flusher
- 【支持】C++ Input -> Go Processors -> Go Aggregator -> Go Flusher
- 【支持】Go Input -> Go Processors -> Go Aggregator -> Go Flusher
- 【不支持】Go Input -> Go Processors -> C++ Flusher
- 【不支持】Go Input -> C++ Processors -> C++ Flusher
- 【不支持】C++ Input -> Go Processors -> C++ Flusher
- 【不支持】Go Input -> C++ Processors -> Go Aggregator -> Go Flusher

下面，我们会介绍 C++ 和 Go 部分的插件系统实现，以及他们之间如何联系在一起。

## C++部分

### C++ 插件如何运行

LoongCollector 的 C++ 部分，会对Pipeline配置进行解析并创建对应插件的实例，如果存在 Go 插件，会将对应的配置转换为Josn 格式，并传递给 Go 模块加载。C++ 部分的插件，则使用总线模式，统一执行。

关于 C++ 插件的结构与开发，请参见[开源插件开发引导](../developer-guide/plugin-development/plugin-development-guide.md)中的“原生插件开发流程（C++语言）”。

为了 C++ 部分数据处理的统一和连贯性，我们对 C++ 的数据结构也进行了更新，参见[数据模型（C++）](../developer-guide/data-model-cpp.md)。

LoongCollector 使用总线模式作为线程模型，所谓总线模式，就是每个功能模块一个线程。根据流水线各个组件的功能划分，一般可以将流水线划分为三大模块：输入模块、处理模块和发送模块，各个模块之间通过缓冲队列进行连接。由于功能模块的数量是恒定的，因此能够保证整个采集器所需要使用的线程数是恒定的。LoongCollector 继承了 iLogtail 的线程模型，并在此基础上进一步演进。

LoongCollector的总线模式示意图如下：

![Bus mode](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/principle/bus_mode.png)

可以看到，在总线模式下，有如下几个固定的工作线程：

- Input Runner线程：负责从指定的数据源获取数据，然后将数据组装成事件组放入处理队列中。Input Runner是所有线程里最复杂的，一般是一种输入插件类型一个Input Runner线程，也可多个输入插件类型共享同一个Input Runner线程。例如，下图展示了三个流水线及相应的线程模型，其中InputFile插件和InputContainerStdio插件由于都属于文件采集，因此共享同一个File Input Runner线程，InputPrometheus插件独享Prometheus Input Runner线程。
  
  当输入插件启动时，可通过调用Input Runner类的函数向其注册插件配置；类似地，当输入插件停止时，也可通过调用Input Runner类的函数向其注销插件配置。通过这种注册方式成功地建立起Input Runner和输入插件之间的联系。

  我们在向Input Runner类注册插件配置的时候，可以额外传递整个流水线的上下文信息，其中就包含处理队列的信息。如此一来，当我们从输入插件指定的数据源获取到数据之后，就可以很方便地知道应该推送到哪个处理队列中。

![Input Runner](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/principle/input_runner.png)

- Processor Runner线程：负责从所有流水线的处理队列中获取事件组，并根据事件组所属的流水线配置对事件组进行处理，最后将处理后的事件组推送到流水线指定的各个Flusher的发送队列中。该线程可以启动多个，默认为1个。

  Processor Runner线程的主要工作流程如下：
    1. 从任意处理队列中获取一个待处理的事件组；
    2. 获取该事件组对应的流水线配置；
    3. 依次调用流水线中的内置处理插件和处理插件对事件组进行处理；
    4. 根据流水线的路由器信息决定将事件组发送到哪些输出插件；
    5. 对于每一个输出插件，根据插件配置对事件组执行批聚合、序列化和压缩等操作，并将结果推送到对应的发送队列中。

  这里比较关键的是步骤2和步骤3。由于每个流水线仅包含一个处理队列，因此我们在从处理队列中获取事件组的同时，可以同时获得相应的流水线名称。然后，我们就可以通过下列函数获得具体的流水线信息。由于处理插件是流水线级别的资源，因此在获取流水线实体以后，就可以直接对事件组进行处理。然而，内置处理插件是与输入插件绑定的，因此我们必须还要知道当前事件组是流水线的哪个输入插件产生的。为此，我们可以在处理队列元素中额外存放输入插件在流水线中的下标，这样我们就能方便地获取事件组对应的内置处理插件了。

- Flusher Runner线程：负责从所有流水线Flusher的发送队列中获取事件组，并根据事件组所属Flusher配置对事件组进行批聚合、序列化和压缩等操作，最后根据Flusher的属性将待发送的请求推送到相应Sink的队列中。

  Flusher Runner线程的主要工作流程如下：
    1. 从任意发送队列中获取一个待发送的数据包；
    2. 获取该事件组对应的输出插件配置；
    3. 根据输出插件的类型将数据包打包成请求，并推送到相应的Sink队列中。

  由于每个流水线可包含多个输出插件，因此为了实现步骤2，仅记录流水线的名字是不够的，还需要记录输出插件在流水线中的下标。由于`PipelineManager::FindConfigByName`函数是需要加锁查表的，因此从效率的角度以及方便的角度考虑，我们直接在发送队列元素中记录指向Flusher的指针，这样就能快速找到输出插件对应的配置

- Sink线程：负责从Sink队列中获取请求并执行实际的发送操作。该线程的个数取决于Sink的类型数，目前已知的包括
  - Http Sink：所有涉及Http发送的Flusher的请求都会推送到该Sink
  - File Sink：所有向本地磁盘写数据的Flusher的请求都会推送到Sink

## Go 部分

我们以如下流程图来说明 LoongCollector Go 部分插件系统的实现原理。

### 如何与 LoongCollector C++ 本体交互

![插件系统实现原理图](https://sls-opensource.oss-us-west-1.aliyuncs.com/ilogtail/ilogtail-adapter-cgo.png?versionId=CAEQNBiBgMCl8rPG7RciIDdlMTUwYWE3MTk0YzRkY2ViN2E3MjgxYjlmODQzNDQx)

为了支持插件系统，我们引入了 libPluginAdaptor 和 libPluginBase（以下简称 adaptor 和 base）这两个动态库，它们与 LoongCollector 之间的关系如下：

- LoongCollector 动态依赖于这两个动态库（即 binary 中不依赖），在初始化时， LoongCollector 会尝试使用动态库接口（如 dlopen）动态加载它们，获取所需的符号。这种设计的好处在于：即使动态加载失败，也仅仅是插件功能不可用，不影响 LoongCollector 本身已有的功能。
- Adaptor 充当一个中间层，LoongCollector 和 base 均依赖它，LoongCollector 首先注册回调，adpator 将这些回调记录下来，以接口的形式暴露给 base 使用。这间接构成了 base 调用 LoongCollector 方法，从而 base 不会依赖 LoongCollector。这些回调中的一个关键方法为 `SendPb` 和 `SendPBV2`，base 可以通过它将数据添加到 LoongCollector 的发送队列，进而递交给 C++ 的发送队列。
- Base 是插件系统的主体，它包含插件系统所必须的采集、处理、聚合以及输出等功能。因此，对 Go 插件系统的扩展实质上是对 base 的扩展。

回到流程图，我们来简要说明下其中的各个步骤：

- LoongCollector 初始化阶段，动态加载 adaptor，并通过得到的符号注册回调。
- LoongCollector 初始化阶段，动态加载 base，获取符号，包括本流程中所用到的 `LoadPipeline`，`ProcessLog` 符号。Base 在 binary 上依赖了 adaptor，所以这一步它会加载 adaptor。
- LoongCollector 从本地/ConfigServer 处获取用户的配置（插件类型）。
- LoongCollector 通过 `LoadPipeline` 将配置中的 Go 插件部分传递给 base。
- Base 应用配置并启动配置，Go Input 插件从指定的数据源获取数据，或 LoongCollector C++ 部分通过 `ProcessLog` 发送数据到 Go 插件处理/发送。
- 如有需要，Base 调用 adaptor 提供的 `SendPb` 或 `SendPBV2` 接口来进行数据递交。
- Adaptor 将 base 递交的数据通过回调直接交给 LoongCollector。
- LoongCollector 将数据加入发送队列。

### 插件如何运行

所有的 Pipeline 信息被 C++ 通过 `LoadPipeline` 传递给 Go，通过`LoadLogstoreConfig`方法创建了一个`LogstoreConfig`对象。根据是否包含 Go Input 插件，也会分为`ToStartPipelineConfigWithInput`和`ToStartPipelineConfigWithInput`两类。

`LogstoreConfig`初始化时，会根据 Pipeline 版本，使用对应的`PluginRunner`，而`PluginRunner`的内部，会创建 Pipeline 中定义的各种插件，并调用对应的方法。我们以一个包含 Go Input 插件的 V2 Pipeline 为例。

- LoongCollector C++ 部分通过 `LoadPipeline` 将配置（`jsonStr`）传递给 Go 插件。（`plugin_main/plugin_export.go`）

  ```go
  //export LoadPipeline
  func LoadPipeline(project string, logstore string, configName string, logstoreKey int64, jsonStr string) int {
    ...
    err := pluginmanager.LoadLogstoreConfig(util.StringDeepCopy(project),util.StringDeepCopy(logstore), util.StringDeepCopy(configName),logstoreKey, jsonStr)
    ...
  }
  ```

- 在`pluginmanager.LoadLogstoreConfig`方法中，会创建对应的`LogstoreConfig`对象，解析`jsonStr`配置，初始化对应的 Context、PluginRunner，并初始化对应的插件实例。（`pluginmanager/logstore_config.go`）

  ```go
  func createLogstoreConfig(...) {
    ...
    contextImp.InitContext(project, logstore, configName)
    ...
    if logstoreC.PluginRunner, err = initPluginRunner(logstoreC); err != nil {
      return nil, err
    }
    ...
    if err = logstoreC.PluginRunner.Init(logQueueSize, logGroupSize); err != nil {
      return nil, err
    }
    ...
    // parse plugins
  }
  ```

- 如果`LogstoreConfig`初始化成功，LoongCollector C++ 部分会通过 `Start` 方法，启动`PluginRunner`，即会调用`PluginRunner.Run()`方法，代表 Go 插件开始工作。

  ```go
  func (lc *LogstoreConfig) Start() {
    ...
    lc.PluginRunner.Run()
    ...
  }
  ```

- `PluginRunner`会按数据流转的顺序，倒序启动所有插件，在里面调用各种类型插件的方法，开始采集/处理/发送，例如`processor.ProcessV2(*models.PipelineGroupEvents)`，`flusher.Export([]*models.PipelineGroupEvents, pipeline.PipelineContext)`。具体的接口可以参考[开源插件开发引导](../developer-guide/plugin-development/plugin-development-guide.md)中，“扩展插件开发流程”内不同插件的开发指引。每一个run内部都是一个协程，不同协程间通过chan通信。

  ```go
  func (p *pluginv2Runner) Run() {
    p.runFlusher()
    p.runAggregator()
    p.runProcessor()
    p.runInput()
  }
  ```

## 其他

### 动态更新配置

目前，日志服务支持通过控制台进行采集配置，然后自动下发到 LoongCollector 进行应用执行，当配置发生变化时，LoongCollector 也会及时地获取这些配置并进行动态更新。插件系统作为配置内容的一部分，同样要具有这种动态更新配置的能力。

在插件系统中，我们以配置为单位对插件进行了组织，每个配置实例根据配置的具体内容，创建并维护其中的 input、processor 等插件的实例。为了实现这种动态更新配置的能力，libPluginBase 提供了相应的 HoldOn/Resume/LoadPipeline 接口进行支持，更新流程如下：

1. LoongCollector 检查配置更新的内容是否有关于插件系统的更新，如果有，调用 HoldOn 暂停插件系统；
2. 插件系统执行 HoldOn 时，会依次停止当前的所有配置实例，这个过程中会存储必要的检查点数据；
3. 在 HoldOn 调用完毕后，LoongCollector 通过 LoadPipeline 加载发生更新或者新增的配置，这个调用会在插件系统内创建新的配置实例，新实例会覆盖之前的实例。 此处为了避免数据的丢失，新创建的实例会将被覆盖的实例中的数据进行转移过来，从而在恢复后能够继续处理这些数据；
4. 当配置更新完毕后，LoongCollector 调用 Resume 恢复插件系统的运行，动态更新配置完毕。

### 统一的自监控指标/告警机制

当前，LoongCollector 对 C++ 和 Go 部分的指标和告警进行了统一暴露，Go 的指标/告警数据会通过C/Go接口统一由 LoongCollector C++ 部分拉取上报，参见：

- [自监控指标说明](../developer-guide/self-monitor/metrics/internal-metrics-description.md)
- 自监控告警说明（开发中）
