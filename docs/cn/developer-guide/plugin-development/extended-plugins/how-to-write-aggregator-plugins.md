# 如何开发 Aggregator 插件

Aggregator 按插件定义的规则聚合 `models.PipelineGroupEvents`，将结果交给下游 Flusher。接口定义见 [`pkg/pipeline/aggregator.go`](../../../../../pkg/pipeline/aggregator.go)。

## Aggregator 接口定义

- `Init` 初始化插件并返回 `GetResult` 的调用周期（毫秒）。返回 `0` 时使用采集配置的全局默认值。
- `Description` 返回插件描述。
- `Record` 接收一个事件组，可通过 `ctx.Collector().Collect` 或 `CollectList` 立即提交聚合完成的事件组。
- `GetResult` 在定时触发和采集配置停止时提交剩余事件组。`Record` 和 `GetResult` 可能并发调用，插件需要保护共享状态。
- `Reset` 清空待提交的状态。

```go
package pipeline

import "github.com/alibaba/ilogtail/pkg/models"

type Aggregator interface {
	Init(Context) (int, error)
	Description() string
	Record(*models.PipelineGroupEvents, PipelineContext) error
	GetResult(PipelineContext) error
	Reset()
}
```

## Aggregator 开发流程

1. **创建 Issue**：描述插件能力与使用场景，由社区讨论可行性与排期；review 通过后再进入实现。
2. **实现接口**：实现 **Aggregator**。可参考 [`aggregator_base`](../../../../../plugins/aggregator/baseagg/aggregator_base.go) 如何缓存和提交事件组。
3. **注册插件**：在 `init` 中通过 [`pipeline.AddAggregatorCreator`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/plugin.go) 注册，或直接向 `pipeline.Aggregators` 写入构造函数。采集配置里的 **Type**（`plugin_type`）必须以 **`aggregator_`** 开头。
4. **纳入构建**：将本插件所在包加入 [插件引用配置文件](https://github.com/alibaba/loongcollector/blob/main/plugins.yml) 的 **`common`** 节；若仅运行于指定系统，请写入 **`linux`** 或 **`windows`**。保存后执行 `make all`（或你常用的构建目标），由工具生成 `plugins/all/*.go`。**请勿手改**生成文件。
5. **测试**：编写或补充单元测试、E2E 场景，具体流程与约定见 [单元测试](../../test/unit-test.md) 与 [E2E测试](../../test/e2e-test.md)。
6. **代码规范**：使用 `make lint` 检查代码风格与静态检查项。
7. **提交变更**：提交 Pull Request，并在描述中说明行为、配置示例与测试结果（如有）。
