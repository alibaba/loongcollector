# 如何开发 Flusher 插件

Flusher 插件与外部系统进行交互，将数据发送到外部，以下将从接口定义与案例2方面指导如何开发 Flusher 插件

## Flusher 接口定义

- Init: 插件初始化接口，对于Flusher 主要作用是创建外部资源链接
- Description： 插件描述
- IsReady：插件系统在调用 **Flush / Export** 前会调用该接口检查当前 flusher 是否仍能够处理更多数据；若返回否，会等待后重试。
- **数据出口**：分为 **FlusherV1** 的 `Flush` 与 **FlusherV2** 的 `Export`（见下方接口）。V1 的 `Flush` 使用 `projectName` / `logstoreName` / `configName` 及 `[]*protocol.LogGroup`；V2 的 `Export` 使用 `[]*models.PipelineGroupEvents` 与 `PipelineContext`。详见 [Golang 侧数据模型](../../data-model-golang.md) 与 [插件系统](../../../principle/plugin-system.md)。

- **SetUrgent**：标识 LoongCollector 即将退出，用于让 Flusher 放宽 `IsReady`、加快吐出等。注释中与 V2 对接的方法名为 **Export**（取代旧描述里的 Flush 字样）。
- Stop：停止Flusher 插件，比如断开与外部系统交互的链接

公共接口 **Flusher** 与两种出口扩展（源码 [`pkg/pipeline/flusher.go`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/flusher.go)）：

```go
type Flusher interface {
    Init(Context) error
    Description() string
    IsReady(projectName string, logstoreName string, logstoreKey int64) bool
    SetUrgent(flag bool)
    Stop() error
}

type FlusherV1 interface {
    Flusher
    Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error
}

type FlusherV2 interface {
    Flusher
    Export([]*models.PipelineGroupEvents, PipelineContext) error
}
```

## Flusher 开发

1. **创建 Issue**：描述插件能力与使用场景，由社区讨论可行性与排期；review 通过后再进入实现。
2. **实现接口**：实现 **Flusher**，并任选 **FlusherV1**（`Flush`）或 **FlusherV2**（`Export`）。可从 [flusher/stdout](https://github.com/alibaba/loongcollector/blob/main/plugins/flusher/stdout/flusher_stdout.go) 或 [flusher/http](https://github.com/alibaba/loongcollector/blob/main/plugins/flusher/http/flusher_http.go) 对照阅读。
3. **注册插件**：在 `init` 中通过 [`pipeline.AddFlusherCreator`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/plugin.go) 注册，或向 `pipeline.Flushers` 写入构造函数。采集配置里的 **Type** 必须以 **`flusher_`** 开头。
4. **纳入构建**：将本插件所在包加入 [插件引用配置文件](https://github.com/alibaba/loongcollector/blob/main/plugins.yml) 的 **`common`** / **`linux`** / **`windows`** 等节，执行构建以更新 `plugins/all`。**请勿手改**生成文件。
5. **测试**：编写或补充单元测试、E2E 场景，具体流程与约定见 [单元测试](../../test/unit-test.md) 与 [E2E测试](../../test/e2e-test.md)。
6. **代码规范**：使用 `make lint` 检查代码风格与静态检查项。
7. **提交变更**：提交 Pull Request，并在描述中说明行为、配置示例与测试结果（如有）。
