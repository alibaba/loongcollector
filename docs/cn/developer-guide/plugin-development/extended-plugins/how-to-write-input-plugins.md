# 如何开发 Input 插件

Go Input 使用 `PipelineContext.Collector()` 提交 `models.PipelineEvent`。接口定义见 [pkg/pipeline/input.go](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/input.go)。

* `MetricInput` 由框架按周期调用 `Read`，适合定时拉取指标等场景。
* `ServiceInput` 通过 `StartService` 启动持续运行的采集服务，适合接收推送或监听连接。

内置 `metric_container` 可作为定时事件采集的参考。`InputContainer.Read(PipelineContext) error` 将容器配置快照构造为 `models.Log`，再通过 `ctx.Collector().Collect` 逐个提交。实现见 [`self_telemetry_container_config.go`](../../../../../pluginmanager/self_telemetry_container_config.go)。

## MetricInput

`Init` 返回采集周期（毫秒）。返回 0 使用全局默认周期。每次调度调用 `Read`，通过传入的 PipelineContext 提交事件。

```go
type MetricInput interface {
    Init(Context) (int, error)
    Description() string
    Read(PipelineContext) error
}
```

## MetricInput 开发流程

1. **创建 Issue**：描述插件能力与使用场景，由社区讨论可行性与排期；review 通过后再进入实现。
2. **实现接口**：实现 **MetricInput**，并实现事件方法 `Read(PipelineContext)`。可参考 [示例 `metric_example.go`](https://github.com/alibaba/loongcollector/blob/main/plugins/input/example/metric_example.go)，用同目录 [`metric_example_input.json`](https://github.com/alibaba/loongcollector/blob/main/plugins/input/example/metric_example_input.json) 做本地试验。
3. **注册插件**：在 `init` 中通过 [`pipeline.AddMetricCreator`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/plugin.go) 注册，或写入 `pipeline.MetricInputs`。采集配置里的 **Type**（`plugin_type`）必须以 **`metric_`** 开头。
4. **纳入构建**：将本插件所在包加入 [插件引用配置文件](https://github.com/alibaba/loongcollector/blob/main/plugins.yml) 的 **`common`** / **`linux`** / **`windows`** 等节，执行构建以更新 `plugins/all`。**请勿手改**生成文件。
5. **测试**：编写或补充单元测试、E2E 场景，具体流程与约定见 [单元测试](../../test/unit-test.md) 与 [E2E测试](../../test/e2e-test.md)。
6. **代码规范**：使用 `make lint` 检查代码风格与静态检查项。
7. **提交变更**：提交 Pull Request，并在描述中说明行为、配置示例与测试结果（如有）。

## ServiceInput

`StartService` 启动采集并使用 PipelineContext 提交事件。`Stop` 负责关闭连接及采集协程。ServiceInput 的 Init 返回值不参与周期调度。

```go
type ServiceInput interface {
    Init(Context) (int, error)
    Description() string
    StartService(PipelineContext) error
    Stop() error
}
```

## ServiceInput 开发流程

1. **创建 Issue**：描述插件能力与使用场景，由社区讨论可行性与排期；review 通过后再进入实现。
2. **实现接口**：实现 **ServiceInput**，并实现事件方法 `StartService(PipelineContext)`。可参考 [示例 `service_example.go`](https://github.com/alibaba/loongcollector/blob/main/plugins/input/example/service_example.go) 与同目录 [`service_example_input.json`](https://github.com/alibaba/loongcollector/blob/main/plugins/input/example/service_example_input.json)。
3. **注册插件**：在 `init` 中通过 [`pipeline.AddServiceCreator`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/plugin.go) 注册，或写入 `pipeline.ServiceInputs`。采集配置里的 **Type** 必须以 **`service_`** 开头。
4. **纳入构建**：将本插件所在包加入 [插件引用配置文件](https://github.com/alibaba/loongcollector/blob/main/plugins.yml) 的相应节，执行构建以更新 `plugins/all`。**请勿手改**生成文件。
5. **测试**：编写或补充单元测试、E2E 场景，具体流程与约定见 [单元测试](../../test/unit-test.md) 与 [E2E测试](../../test/e2e-test.md)。
6. **代码规范**：使用 `make lint` 检查代码风格与静态检查项。
7. **提交变更**：提交 Pull Request，并在描述中说明行为、配置示例与测试结果（如有）。

## 依赖与仓库命名

主仓库根目录 [`go.mod`](https://github.com/alibaba/loongcollector/blob/main/go.mod) 的 Go **module** 当前为 `github.com/alibaba/ilogtail`；插件代码中 `import` 须与该 module 一致（与 GitHub 仓库显示名 LoongCollector 可以不同）。
