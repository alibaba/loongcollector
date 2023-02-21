# 如何开发Aggregator插件

Aggregator 插件对输入数据进行打包，以下将从接口定义与案例2方面指导如何开发 Aggregator 插件

## Aggregator 接口定义

Aggregator 插件的作用就是将一条条独立的 Log 根据一定规则聚合成 LogGroup，进而提交给下一级的 flusher 插件处理。

- Add 接口供外部输入 Log
- Flush 接口供外部获取聚合得到的 LogGroug
- Reset 接口目前仅在内部使用，可以忽略
- Init 接口，类似于 input 插件的 Init 接口，该接口返回值的第一个参数表示插件系统调用 Flush 接口的周期值，该值为 0 时使用全局参数，第二个参数表示一个初始化错误。 但不同于 input 插件，aggregator 插件的 Init 接口新增了一个 LogGroupQueue 类型的参数，该类型定义于 loggroup_queue.go 文件中，如下：

    ```go
    type LogGroupQueue interface {
        // Returns errAggAdd immediately if queue is full.
        Add(loggroup *LogGroup) error
        // Wait at most @duration if queue is full and returns errAggAdd if timeout.
        // Do not use this method if you are unsure.
        AddWithWait(loggroup *LogGroup, duration time.Duration) error
    }
    ```

  该接口对应的实例实际上充当了一个队列，aggregator 插件实例可以将聚合得到的 LogGroup 对象立即通过 AddXXX 接口插入队列。 这是一个可选项，从之前的描述可以看到，Add->Flush 是一个周期性调用的数据链路，而 LogGroupQueue 可以提供一个实时性更高的链路，在 aggregator 一得到新聚合的 LogGroup 后就直接提交，不必等到 Flush 被调用。

    但需要注意的是，在队列满的时候，Add 会返回错误，这一般意味着 flusher 发生了阻塞，比如网络异常。

```go
// Aggregator is an interface for implementing an Aggregator plugin.
// the RunningAggregator wraps this interface and guarantees that
// Add, Push, and Reset can not be called concurrently, so locking is not
// required when implementing an Aggregator plugin.
type Aggregator interface {
// Init called for init some system resources, like socket, mutex...
// return flush interval(ms) and error flag, if interval is 0, use default interval
Init(Context, LogGroupQueue) (int, error)

// Description returns a one-sentence description on the Input.
Description() string

// Add the metric to the aggregator.
Add(log *protocol.Log) error

// Flush pushes the current aggregates to the accumulator.
Flush() []*protocol.LogGroup

// Reset resets the aggregators caches and aggregates.
Reset()
}
```

## Aggregator 开发

Aggregator 的开发分为以下步骤:

1. 创建Issue，描述开发插件功能，会有社区同学参与讨论插件开发的可行性，如果社区review 通过，请参考步骤2继续进行。
2. 实现 Aggregator 接口，这里我们使用样例模式进行介绍，详细样例请查看[aggregator/defaultone](https://github.com/alibaba/ilogtail/blob/main/plugins/aggregator/defaultone/aggregator_default.go)。
3. 通过init将插件注册到[Aggregators](https://github.com/alibaba/ilogtail/blob/main/plugin.go)，Aggregator插件的注册名（即json配置中的plugin_type）必须以"aggregator_"开头，详细样例请查看[aggregator/defaultone](https://github.com/alibaba/ilogtail/blob/main/plugins/aggregator/defaultone/aggregator_default.go)。
4. 将插件加入[插件引用配置文件](https://github.com/alibaba/ilogtail/blob/main/plugins.yml)的`common`配置节, 如果仅运行于指定系统，请添加到`linux`或`windows`配置节.
5. 进行单测或者E2E测试，请参考[如何使用单测](../test/unit-test.md) 与 [如何使用E2E测试](../test/e2e-test.md).
6. 使用 *make lint* 检查代码规范。
7. 提交Pull Request。
