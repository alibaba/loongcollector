# 如何开发 Processor 插件

Processor 按事件组处理采集数据。接口定义见 [pkg/pipeline/processor.go](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/processor.go)。

```go
type Processor interface {
    Init(Context) error
    Description() string
    Process(*models.PipelineGroupEvents, PipelineContext)
}
```

同一采集配置中的 Processor 串行处理事件组。使用 `context.Collector()` 提交处理结果。参考 [processor_add_fields](https://github.com/alibaba/loongcollector/blob/main/plugins/processor/addfields/processor_add_fields.go)。

同一批 `in.Events` 可能混有 Log / Metric / Span。`Process` **必须遍历全部事件**（或调用 `pipeline.ProcessLogEventsOnly` / `pipeline.CollectGroupEvents`），按 `event.GetType()` 处理或透传。

**禁止**只取 `Events[0]` 并假定其为 `*models.Log`：Metric/Span 会 panic，同批其余事件会被静默丢掉。

```go
// 错误：假定 Events[0] 为 Log
log := in.Events[0].(*models.Log)

// 正确：Log-only 插件
pipeline.ProcessLogEventsOnly(in, context, p.processLogEvent)

// 正确：需要按事件种类分支时遍历
for _, event := range in.Events {
    if event.GetType() == models.EventTypeLogging {
        p.processLogEvent(event.(*models.Log))
    }
}
```

CI Static Check 通过 `python3 scripts/lint_process_events0.py` 强制该规则（Issue #2604 / Discussion #1928 B5）。存量例外写入 `scripts/lint_process_events0_allowlist.txt`，新增违规不得加豁免。

## Processor 开发流程

1. **创建 Issue**：描述插件能力与使用场景，由社区讨论可行性与排期；review 通过后再进入实现。
2. **实现接口**：实现 **Processor** 的初始化、说明和事件处理方法。
3. **注册插件**：在 `init` 中通过 [`pipeline.AddProcessorCreator`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/plugin.go) 注册，或向 `pipeline.Processors` 写入构造函数。采集配置里的 **Type** 必须以 **`processor_`** 开头。
4. **纳入构建**：将本插件所在包加入 [插件引用配置文件](https://github.com/alibaba/loongcollector/blob/main/plugins.yml) 的 **`common`** / **`linux`** / **`windows`** 等节，执行构建以更新 `plugins/all`。**请勿手改**生成文件。
5. **测试**：编写或补充单元测试、E2E 场景，具体流程与约定见 [单元测试](../../test/unit-test.md) 与 [E2E测试](../../test/e2e-test.md)。
6. **代码规范**：使用 `make lint` 检查代码风格与静态检查项。`Process` 还须通过 `python3 scripts/lint_process_events0.py`（禁止假定 `Events[0]` 为 Log）。
7. **提交变更**：提交 Pull Request，并在描述中说明行为、配置示例与测试结果（如有）。

## Processor 准入性能规范

* **基础 case 参考**：512 随机字符作为内容，完整执行一次 processor 中的逻辑。
* **处理速度准入条件**：4w/s。

## 依赖路径

Go 模块名以仓库根目录 `go.mod` 为准（当前为 `github.com/alibaba/ilogtail`）。
