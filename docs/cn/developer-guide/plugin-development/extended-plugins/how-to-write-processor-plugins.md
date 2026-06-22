# 如何开发 Processor 插件

Processor 对**采集配置**（collection pipeline）内的数据做加工。接口定义见 [`pkg/pipeline/processor.go`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/processor.go)：在公共的 **Processor** 之上，分为 **ProcessorV1** 与 **ProcessorV2**，新插件建议优先实现 **V2**。

## Processor（公共）

```go
type Processor interface {
    Init(pipelineContext Context) error
    Description() string
}
```

## ProcessorV1（旧数据路径）

输入输出均为 `[]*protocol.Log`，多个 Processor 实例在链路上 **串行** 调用 `ProcessLogs`。

```go
type ProcessorV1 interface {
    Processor
    ProcessLogs(logArray []*protocol.Log) []*protocol.Log
}
```

参考实现：[plugins/processor/addfields](https://github.com/alibaba/loongcollector/blob/main/plugins/processor/addfields/processor_add_fields.go)，试验配置 [example.json](https://github.com/alibaba/loongcollector/blob/main/plugins/processor/addfields/example.json)。

## ProcessorV2（新数据路径）

按 **PipelineGroupEvents** 批处理，并接收 **PipelineContext**：

```go
type ProcessorV2 interface {
    Processor
    Process(in *models.PipelineGroupEvents, context PipelineContext)
}
```

## Processor 开发流程

1. **创建 Issue**：描述插件能力与使用场景，由社区讨论可行性与排期；review 通过后再进入实现。
2. **实现接口**：实现 **Processor**，并任选 **ProcessorV1** 或 **ProcessorV2**（以框架对插件类型的调用为准；老插件多仍为 V1）。
3. **注册插件**：在 `init` 中通过 [`pipeline.AddProcessorCreator`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/plugin.go) 注册，或向 `pipeline.Processors` 写入构造函数。采集配置里的 **Type** 必须以 **`processor_`** 开头。
4. **纳入构建**：将本插件所在包加入 [插件引用配置文件](https://github.com/alibaba/loongcollector/blob/main/plugins.yml) 的 **`common`** / **`linux`** / **`windows`** 等节，执行构建以更新 `plugins/all`。**请勿手改**生成文件。
5. **测试**：编写或补充单元测试、E2E 场景，具体流程与约定见 [单元测试](../../test/unit-test.md) 与 [E2E测试](../../test/e2e-test.md)。
6. **代码规范**：使用 `make lint` 检查代码风格与静态检查项。
7. **提交变更**：提交 Pull Request，并在描述中说明行为、配置示例与测试结果（如有）。

## Processor 准入性能规范

* **基础 case 参考**：512 随机字符作为内容，完整执行一次 processor 中的逻辑。
* **处理速度准入条件**：4w/s。

## 依赖路径

Go 模块名以仓库根目录 `go.mod` 为准（当前为 `github.com/alibaba/ilogtail`）。
