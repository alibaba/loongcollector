# 如何开发Processor插件

Processor 插件对输入数据进行二次加工，以下将从接口定义与案例2方面指导如何开发Processor 插件

## Processor 接口定义

Processor 插件接口方法非常容易理解，除 Init/Description 这两个标准方法外，只有一个 ProcessLogs 接口，它的输入输出都是 []*Log，即接受一个 Log 列表，经过处理后返回一个 Log 列表。很自然地，processor 插件实例之间通过此接口构成了一个串行的处理链。

```go
// Processor also can be a filter
type Processor interface {
    // Init called for init some system resources, like socket, mutex...
    Init(Context) error

    // Description returns a one-sentence description on the Input
    Description() string

    // Apply the filter to the given metric
    ProcessLogs(logArray []*protocol.Log) []*protocol.Log
}
```

## Processor 开发

Processor 的开发分为以下步骤:

1. 创建Issue，描述开发插件功能，会有社区同学参与讨论插件开发的可行性，如果社区review 通过，请参考步骤2继续进行。
2. 实现Processor 接口，这里我们使用样例模式进行介绍，详细样例请查看[processor/addfields](https://github.com/alibaba/ilogtail/blob/main/plugins/processor/addfields/processor_add_fields.go)，你可以使用 [example.json](https://github.com/alibaba/ilogtail/blob/main/plugins/processor/addfields/example.json) 试验此插件功能。
3. 通过init将插件注册到[Processors](https://github.com/alibaba/ilogtail/blob/main/plugin.go)，Processor插件的注册名（即json配置中的plugin_type）必须以"processor_"开头，详细样例请查看[processor/addfields](https://github.com/alibaba/ilogtail/blob/main/plugins/processor/addfields/processor_add_fields.go)。
4. 将插件加入[全局插件定义中心](https://github.com/alibaba/ilogtail/blob/main/plugins/all/all.go), 如果仅运行于指定系统，请添加到[Linux插件定义中心](https://github.com/alibaba/ilogtail/blob/main/plugins/all/all_linux.go) 或 [Windows插件定义中心](https://github.com/alibaba/ilogtail/blob/main/plugins/all/all_windows.go).
5. 进行单测或者E2E测试，请参考[如何使用单测](../test/unit-test.md) 与 [如何使用E2E测试](../test/e2e-test.md).
6. 使用 *make lint* 检查代码规范。
7. 提交Pull Request。
