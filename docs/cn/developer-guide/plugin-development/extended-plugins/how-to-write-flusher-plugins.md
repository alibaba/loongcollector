# 如何开发 Flusher 插件

Flusher 插件将事件组发送到外部系统。事件结构见 [Golang 侧数据模型](../../data-model-golang.md)。

## Flusher 接口定义

- `Init` 初始化插件并创建连接等资源。
- `Description` 返回插件描述。
- `IsReady` 检查目标是否能接收更多数据。插件系统在 `Export` 前调用此方法，返回 `false` 时等待后重试。
- `Export` 接收一批 `models.PipelineGroupEvents` 及 `PipelineContext`，按目标协议发送数据。需要编码时，可参考 [协议转换](../../log-protocol/converter.md)。
- `SetUrgent` 通知插件即将停止，以便调整就绪判断和发送策略。
- `Stop` 发送尚未写出的缓存数据并释放资源。

接口定义见 [`pkg/pipeline/flusher.go`](../../../../../pkg/pipeline/flusher.go)。

```go
package pipeline

import "github.com/alibaba/ilogtail/pkg/models"

type Flusher interface {
	Init(Context) error
	Description() string
	IsReady(projectName string, logstoreName string, logstoreKey int64) bool
	SetUrgent(flag bool)
	Stop() error
	Export([]*models.PipelineGroupEvents, PipelineContext) error
}
```

## Flusher 开发

1. **创建 Issue**：描述插件能力与使用场景，由社区讨论可行性与排期；review 通过后再进入实现。
2. **实现接口**：实现 **Flusher**，在 `Export` 中按目标协议处理事件组。可从 [flusher/stdout](https://github.com/alibaba/loongcollector/blob/main/plugins/flusher/stdout/flusher_stdout.go) 或 [flusher/http](https://github.com/alibaba/loongcollector/blob/main/plugins/flusher/http/flusher_http.go) 对照阅读。
3. **注册插件**：在 `init` 中通过 [`pipeline.AddFlusherCreator`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/plugin.go) 注册，或向 `pipeline.Flushers` 写入构造函数。采集配置里的 **Type** 必须以 **`flusher_`** 开头。
4. **纳入构建**：将本插件所在包加入 [插件引用配置文件](https://github.com/alibaba/loongcollector/blob/main/plugins.yml) 的 **`common`** / **`linux`** / **`windows`** 等节，执行构建以更新 `plugins/all`。**请勿手改**生成文件。
5. **测试**：编写或补充单元测试、E2E 场景，具体流程与约定见 [单元测试](../../test/unit-test.md) 与 [E2E测试](../../test/e2e-test.md)。
6. **代码规范**：使用 `make lint` 检查代码风格与静态检查项。
7. **提交变更**：提交 Pull Request，并在描述中说明行为、配置示例与测试结果（如有）。
