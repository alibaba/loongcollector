# 如何开发Input插件

了解如何开发Input插件前你需要先了解Input 插件接口，目前的Input 插件分为2类，分别为MetricInput 与ServiceInput，分别为MetricInput可以由全局控制采集频率，适合Metric指标拉取等情况，ServiceInput 更适合接收外部输入情况，比如接收SkyWalking的Trace 数据推送。

## MetricInput 接口定义

- Init：插件初始化接口，主要供插件做一些参数检查，并为其提供上下文对象 Context。
返回值的第一个参数表示调用周期（毫秒），插件系统会以该值作为数据获取周期。如果返回 0 的话，插件系统会使用全局的数据获取周期。
返回值的第二个参数表示初始化中发生的错误，一旦发生错误该插件实例将被忽略，插件系统不会向获取数据。
- Description：插件自描述接口。
- Collect： MetricInput 最关键的接口，插件系统通过此接口从插件实例中获取数据。插件系统会周期性地调用此接口，将数据收集到传入 Collector 中。

```go
// MetricInput ...
type MetricInput interface {
    // Init called for init some system resources, like socket, mutex...
    // return call interval(ms) and error flag, if interval is 0, use default interval
    Init(Context) (int, error)

    // Description returns a one-sentence description on the Input
    Description() string

    // Collect takes in an accumulator and adds the metrics that the Input
    // gathers. This is called every "interval"
    Collect(Collector) error
}
```

## MetricInput 开发

MetricInput 的开发分为以下步骤:

1. 创建Issue，描述开发插件功能，会有社区同学参与讨论插件开发的可行性，如果社区review 通过，请参考步骤2继续进行。
2. 实现MetricInput 接口，这里我们使用样例模式进行介绍，详细样例请查看[input/example/metric](https://github.com/alibaba/ilogtail/blob/main/plugins/input/example/metric_example.go)，你可以使用 [example.json](https://github.com/alibaba/ilogtail/blob/main/plugins/input/example/metric_example_input.json) 试验此插件功能。
3. 通过init将插件注册到[MetricInputs](https://github.com/alibaba/ilogtail/blob/main/plugin.go)，MetricInputs插件的注册名（即json配置中的plugin_type）必须以"metric_"开头，详细样例请查看[input/example/metric](https://github.com/alibaba/ilogtail/blob/main/plugins/input/example/metric_example.go)。
4. 将插件加入[全局插件定义中心](https://github.com/alibaba/ilogtail/blob/main/plugins/all/all.go), 如果仅运行于指定系统，请添加到[Linux插件定义中心](https://github.com/alibaba/ilogtail/blob/main/plugins/all/all_linux.go) 或 [Windows插件定义中心](https://github.com/alibaba/ilogtail/blob/main/plugins/all/all_windows.go).
5. 进行单测或者E2E测试，请参考[如何使用单测](../test/unit-test.md) 与 [如何使用E2E测试](../test/e2e-test.md).
6. 使用 *make lint* 检查代码规范。
7. 提交Pull Request。

## ServiceInput 接口定义

- Init/Description：同 MetricInput 接口，但是 Init 的一个返回值对于 ServiceInput 没有意义，因为它不会被周期性调用。
- Start：由于 ServiceInput 的定位是常驻型输入，所以插件系统为此类型的插件实例都创建了独立的 goroutine，在 goroutine 中调用此接口来开始数据收集。Collector 的作用依旧是充当插件实例和插件系统之间的数据管道。
- Stop：提供终止插件的能力。插件必须正确地实现此接口，以避免 hang 住插件系统导致 Logtail 也无法正确退出。

```go

// ServiceInput ...
type ServiceInput interface {
    // Init called for init some system resources, like socket, mutex...
    // return interval(ms) and error flag, if interval is 0, use default interval
    Init(Context) (int, error)

    // Description returns a one-sentence description on the Input
    Description() string

    // Start starts the ServiceInput's service, whatever that may be
    Start(Collector) error

    // Stop stops the services and closes any necessary channels and connections
    Stop() error
}
```

## ServiceInput 开发

ServiceInput 的开发分为以下步骤:

1. 创建Issue，描述开发插件功能，会有社区同学参与讨论插件开发的可行性，如果社区review 通过，请参考步骤2继续进行。
2. 实现 ServiceInput 接口，这里我们使用样例模式进行介绍，详细样例请查看[input/example/service](https://github.com/alibaba/ilogtail/blob/main/plugins/input/example/service_example.go), 你可以使用 [example.json](https://github.com/alibaba/ilogtail/blob/main/plugins/input/example/service_example_input.json) 试验此插件功能。
3. 通过init将插件注册到[ServiceInputs](https://github.com/alibaba/ilogtail/blob/main/plugin.go)，ServiceInputs插件的注册名（即json配置中的plugin_type）必须以"service_"开头，详细样例请查看[input/example/service](https://github.com/alibaba/ilogtail/blob/main/plugins/input/example/service_example.go)。
4. 将插件加入[插件引用配置文件](https://github.com/alibaba/ilogtail/blob/main/plugins.yml)的`common`配置节, 如果仅运行于指定系统，请添加到`linux`或`windows`配置节.
5. 进行单测或者E2E测试，请参考[如何使用单测](../test/unit-test.md) 与 [如何使用E2E测试](../test/e2e-test.md).
6. 使用 *make lint* 检查代码规范。
7. 提交Pull Request。
