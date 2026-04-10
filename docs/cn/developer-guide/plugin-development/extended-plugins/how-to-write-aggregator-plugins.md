# 如何开发 Aggregator 插件

Aggregator 将多条事件聚合成 **LogGroup** / **PipelineGroupEvents** 再交给 Flusher。定义见 [`pkg/pipeline/aggregator.go`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/aggregator.go)。

## 公共接口 `Aggregator`

* **Init(Context, LogGroupQueue) (int, error)**：第一个返回值为 **Flush 调用周期（毫秒）**；为 `0` 时用全局默认。第二个返回值表示初始化错误。**LogGroupQueue** 定义见同目录 [`loggroup_queue.go`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/loggroup_queue.go)：`Add` / `AddWithWait` 可在聚合完成后 **立刻** 把 `LogGroup` 推入队列，而不必等到周期 `Flush`。
* **Description**：插件一句描述。
* **Reset**：清空内部状态；与 `RunningAggregator` 的并发约定见源码注释。

```go
type Aggregator interface {
    Init(Context, LogGroupQueue) (int, error)
    Description() string
    Reset()
}
```

## AggregatorV1（`protocol.Log` / `LogGroup`）

* **Add(log *protocol.Log, ctx map[string]interface{}) error**：写入 **一条数据**（V1 路径下的单条记录）。
* **Flush() []*protocol.LogGroup**：取出当前聚合结果。

```go
type AggregatorV1 interface {
    Aggregator
    Add(log *protocol.Log, ctx map[string]interface{}) error
    Flush() []*protocol.LogGroup
}
```

注册入口参考 [`plugins/aggregator/aggregator_default.go`](https://github.com/alibaba/loongcollector/blob/main/plugins/aggregator/aggregator_default.go)（默认实现委托给 `plugins/aggregator/context`）。

## AggregatorV2（`PipelineGroupEvents`）

* **Record(*models.PipelineGroupEvents, PipelineContext) error**
* **GetResult(PipelineContext) error**

```go
type AggregatorV2 interface {
    Aggregator
    Record(*models.PipelineGroupEvents, PipelineContext) error
    GetResult(PipelineContext) error
}
```

## LogGroupQueue 说明

队列满时 **`Add` 会立即返回错误**，通常表示下游 Flusher 阻塞（如网络异常）。`AddWithWait` 可在队列满时短暂等待，不确定时不要使用。

## Aggregator 开发流程

1. **创建 Issue**：描述插件能力与使用场景，由社区讨论可行性与排期；review 通过后再进入实现。
2. **实现接口**：实现 **Aggregator**，并任选 **AggregatorV1** 或 **AggregatorV2**（或按框架实际调用路径实现对应方法）。可参考上文默认聚合实现与源码注释。
3. **注册插件**：在 `init` 中通过 [`pipeline.AddAggregatorCreator`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/plugin.go) 注册，或直接向 `pipeline.Aggregators` 写入构造函数。采集配置里的 **Type**（`plugin_type`）必须以 **`aggregator_`** 开头。
4. **纳入构建**：将本插件所在包加入 [插件引用配置文件](https://github.com/alibaba/loongcollector/blob/main/plugins.yml) 的 **`common`** 节；若仅运行于指定系统，请写入 **`linux`** 或 **`windows`**。保存后执行 `make all`（或你常用的构建目标），由工具生成 `plugins/all/*.go`。**请勿手改**生成文件。
5. **测试**：编写或补充单元测试、E2E 场景，具体流程与约定见 [单元测试](../../test/unit-test.md) 与 [E2E测试](../../test/e2e-test.md)。
6. **代码规范**：使用 `make lint` 检查代码风格与静态检查项。
7. **提交变更**：提交 Pull Request，并在描述中说明行为、配置示例与测试结果（如有）。
