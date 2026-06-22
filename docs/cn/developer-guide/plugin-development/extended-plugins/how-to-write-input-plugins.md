# 如何开发 Input 插件

开发前请先阅读 [`pkg/pipeline/input.go`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/input.go) 中的接口分层：**公共接口**（`MetricInput` / `ServiceInput`）与 **V1 / V2 扩展**（采集路径不同）是分开定义的。

目前 Input 分为两类：

* **MetricInput**：由插件框架按周期调度，适合定时拉取指标等场景。
* **ServiceInput**：在独立 goroutine 中常驻运行，适合接受外部推送（如 HTTP 接收 trace）等场景。

实现 **V2** 时，采集数据使用 `PipelineContext` 与 `models` 中的新模型；实现 **V1** 时仍使用 `Collector` 与 `protocol.Log`。新插件建议优先对齐 **V2**，与老代码/示例并存时可继续用 **V1**。

## MetricInput：公共接口与 V1 / V2

### 公共部分（`MetricInput`）

* **Init**：参数校验与资源准备；返回值为 **调用周期（毫秒）** 与 **error**。周期为 `0` 时使用全局默认周期。若 `Init` 返回非 nil error，该插件实例会被丢弃。
* **Description**：一句插件说明。

### V1：`MetricInputV1`

* **Collect(Collector) error**：在每到的一个周期内被调用，将数据写入传入的 `Collector`。

### V2：`MetricInputV2`

* **Read(PipelineContext) error**：在每到的一个周期内被调用，通过 `PipelineContext` 访问 `Collector()` 等能力写入数据。

```go
type MetricInput interface {
    Init(Context) (int, error)
    Description() string
}

type MetricInputV1 interface {
    MetricInput
    Collect(Collector) error
}

type MetricInputV2 interface {
    MetricInput
    Read(PipelineContext) error
}
```

## MetricInput 开发流程

1. **创建 Issue**：描述插件能力与使用场景，由社区讨论可行性与排期；review 通过后再进入实现。
2. **实现接口**：实现 **MetricInput**，并任选 **MetricInputV1**（`Collect`）或 **MetricInputV2**（`Read`）。可参考 [示例 `metric_example.go`](https://github.com/alibaba/loongcollector/blob/main/plugins/input/example/metric_example.go)，用同目录 [`metric_example_input.json`](https://github.com/alibaba/loongcollector/blob/main/plugins/input/example/metric_example_input.json) 做本地试验。
3. **注册插件**：在 `init` 中通过 [`pipeline.AddMetricCreator`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/plugin.go) 注册，或写入 `pipeline.MetricInputs`。采集配置里的 **Type**（`plugin_type`）必须以 **`metric_`** 开头。
4. **纳入构建**：将本插件所在包加入 [插件引用配置文件](https://github.com/alibaba/loongcollector/blob/main/plugins.yml) 的 **`common`** / **`linux`** / **`windows`** 等节，执行构建以更新 `plugins/all`。**请勿手改**生成文件。
5. **测试**：编写或补充单元测试、E2E 场景，具体流程与约定见 [单元测试](../../test/unit-test.md) 与 [E2E测试](../../test/e2e-test.md)。
6. **代码规范**：使用 `make lint` 检查代码风格与静态检查项。
7. **提交变更**：提交 Pull Request，并在描述中说明行为、配置示例与测试结果（如有）。

## ServiceInput：公共接口与 V1 / V2

### 公共部分（`ServiceInput`）

* **Init** / **Description**：与 MetricInput 相同。**注意**：Init 返回的周期对 ServiceInput **无意义**（不会被按周期调度）。
* **Stop**：必须可靠退出，避免阻塞进程退出。

### V1：`ServiceInputV1`

* **Start(Collector) error**：在专属 goroutine 中启动采集逻辑。

### V2：`ServiceInputV2`

* **StartService(PipelineContext) error**：在专属 goroutine 中启动；通过 `PipelineContext` 获取 `Collector()` 等。

```go
type ServiceInput interface {
    Init(Context) (int, error)
    Description() string
    Stop() error
}

type ServiceInputV1 interface {
    ServiceInput
    Start(Collector) error
}

type ServiceInputV2 interface {
    ServiceInput
    StartService(PipelineContext) error
}
```

## ServiceInput 开发流程

1. **创建 Issue**：描述插件能力与使用场景，由社区讨论可行性与排期；review 通过后再进入实现。
2. **实现接口**：实现 **ServiceInput**，并任选 **ServiceInputV1**（`Start`）或 **ServiceInputV2**（`StartService`）。可参考 [示例 `service_example.go`](https://github.com/alibaba/loongcollector/blob/main/plugins/input/example/service_example.go) 与同目录 [`service_example_input.json`](https://github.com/alibaba/loongcollector/blob/main/plugins/input/example/service_example_input.json)。
3. **注册插件**：在 `init` 中通过 [`pipeline.AddServiceCreator`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/plugin.go) 注册，或写入 `pipeline.ServiceInputs`。采集配置里的 **Type** 必须以 **`service_`** 开头。
4. **纳入构建**：将本插件所在包加入 [插件引用配置文件](https://github.com/alibaba/loongcollector/blob/main/plugins.yml) 的相应节，执行构建以更新 `plugins/all`。**请勿手改**生成文件。
5. **测试**：编写或补充单元测试、E2E 场景，具体流程与约定见 [单元测试](../../test/unit-test.md) 与 [E2E测试](../../test/e2e-test.md)。
6. **代码规范**：使用 `make lint` 检查代码风格与静态检查项。
7. **提交变更**：提交 Pull Request，并在描述中说明行为、配置示例与测试结果（如有）。

## 依赖与仓库命名

主仓库根目录 [`go.mod`](https://github.com/alibaba/loongcollector/blob/main/go.mod) 的 Go **module** 当前为 `github.com/alibaba/ilogtail`；插件代码中 `import` 须与该 module 一致（与 GitHub 仓库显示名 LoongCollector 可以不同）。
