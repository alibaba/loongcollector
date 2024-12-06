# 如何开发 Flusher 插件

Flusher 插件与外部系统进行交互，将数据发送到外部，以下将从接口定义与案例2方面指导如何开发 Flusher 插件

## Flusher 接口定义

- Init: 插件初始化接口，对于Flusher 主要作用是创建外部资源链接
- Description： 插件描述
- IsReady：插件系统在调用 Flush 前会调用该接口检查当前 flusher 是否仍能够处理更多数据，如果答案为否的话，它会等待一段时间再重试。
- Flush 接口是插件系统向 flusher 插件实例提交数据的入口，用于将数据输出到外部系统。为了映射到日志服务的概念中，我们增加了三个 string 参数，它代表这个 flusher 实例所属的
  project/logstore/config。详细解释请参与[数据结构](../data-structure.md) 与 [基本结构](../../../principle/plugin-system.md) 。

- SetUrgent： 标识iLogtail 即将退出，将系统状态传递给具体Flusher 插件，可以供Flusher 插件自动适应系统状态，比如加快输出速率等。(SetUrgent调用发生在其他类型插件的Stop之前，当前尚无有意义的实现)
- Stop：停止Flusher 插件，比如断开与外部系统交互的链接

```go
type Flusher interface {
   // Init called for init some system resources, like socket, mutex...
   Init(Context) error

   // Description returns a one-sentence description on the Input.
   Description() string

   // IsReady checks if flusher is ready to accept more data.
   // @projectName, @logstoreName, @logstoreKey: meta of the corresponding data.
   // Note: If SetUrgent is called, please make some adjustment so that IsReady
   //   can return true to accept more data in time and config instance can be
   //   stopped gracefully.
   IsReady(projectName string, logstoreName string, logstoreKey int64) bool

   // Flush flushes data to destination, such as SLS, console, file, etc.
   // It is expected to return no error at most time because IsReady will be called
   // before it to make sure there is space for next data.
   Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error

   // SetUrgent indicates the flusher that it will be destroyed soon.
   // @flag indicates if main program (Logtail mostly) will exit after calling this.
   //
   // Note: there might be more data to flush after SetUrgent is called, and if flag
   //   is true, these data will be passed to flusher through IsReady/Flush before
   //   program exits.
   //
   // Recommendation: set some state flags in this method to guide the behavior
   //   of other methods.
   SetUrgent(flag bool)

   // Stop stops flusher and release resources.
   // It is time for flusher to do cleaning jobs, includes:
   // 1. Flush cached but not flushed data. For flushers that contain some kinds of
   //   aggregation or buffering, it is important to flush cached out now, otherwise
   //   data will lost.
   // 2. Release opened resources: goroutines, file handles, connections, etc.
   // 3. Maybe more, it depends.
   // In a word, flusher should only have things that can be recycled by GC after this.
   Stop() error
}
```

## Flusher 开发

Flusher 的开发分为以下步骤:

1. 创建Issue，描述开发插件功能，会有社区同学参与讨论插件开发的可行性，如果社区review 通过，请参考步骤2继续进行。
2. 实现 Flusher 接口，这里我们使用样例模式进行介绍，详细样例请查看[flusher/stdout](https://github.com/alibaba/loongcollector/blob/main/plugins/flusher/stdout/flusher_stdout.go)
3. 通过init将插件注册到[Flushers](https://github.com/alibaba/loongcollector/blob/main/plugin.go)，Flusher插件的注册名（即json配置中的plugin_type）必须以"flusher_"开头，详细样例请查看[flusher/stdout](https://github.com/alibaba/loongcollector/blob/main/plugins/flusher/stdout/flusher_stdout.go)。
   ，你可以使用 [example.json](https://github.com/alibaba/loongcollector/blob/main/plugins/processor/addfields/example.json) 试验此插件功能。
4. 将插件加入[插件引用配置文件](https://github.com/alibaba/loongcollector/blob/main/plugins.yml)的`common`配置节, 如果仅运行于指定系统，请添加到`linux`或`windows`配置节.
5. 进行单测或者E2E测试，请参考[如何使用单测](../../test/unit-test.md) 与 [如何使用E2E测试](../../test/e2e-test.md).
6. 使用 *make lint* 检查代码规范。
7. 提交Pull Request。
