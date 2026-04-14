# 如何开发Extension插件

Extension 插件可以用来注册自定义的能力和接口实现，这些能力和接口实现，可以在其他插件(input、processor、aggregator、flusher)中引用。

## Extension 接口定义

Extension 插件用于以统一方式注册**可选能力**（多为某业务接口的实现）。在采集配置里放在 `extensions` 段后，**同一采集配置内**的其他插件（Input / Processor / Aggregator / Flusher）可通过上下文 **`GetExtension`** 按 **Type** 取出 `any`，再安全断言为你实现的接口（如 [`pkg/pipeline/extensions/authenticator.go`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/extensions/authenticator.go) 中的 `ClientAuthenticator`）。具体字段名以各插件文档为准（常见如 HTTP Flusher 的鉴权配置引用某 `ext_*`）。

- Description: 插件描述
- Init: 插件初始化接口，对于 Extension 来讲，可以是任何其所提供的能力的必要的初始化动作
- Stop: 停止插件，比如断开与外部系统交互的连接等

```go
type Extension interface {
 // Description returns a one-sentence description on the Extension
 Description() string

 // Init called for init some system resources, like socket, mutex...
 Init(Context) error

 // Stop stops the services and release resources
 Stop() error
}
```

## Extension 开发

1. **创建 Issue**：描述插件能力与使用场景，由社区讨论可行性与排期；review 通过后再进入实现。
2. **实现接口**：除上文通用 **Extension** 接口外，还需实现本扩展对外提供的**业务能力接口**（由其它插件 `GetExtension` 后断言使用）。可参考 [extension/basicauth](https://github.com/alibaba/loongcollector/blob/main/plugins/extension/basicauth/basicauth.go)，其同时实现 [`extensions.ClientAuthenticator`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/extensions/authenticator.go)。
3. **注册插件**：在 `init` 中通过 [`pipeline.AddExtensionCreator`](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/plugin.go) 注册，或向 `pipeline.Extensions` 写入构造函数。采集配置里的 **Type** 必须以 **`ext_`** 开头。
4. **纳入构建**：将本插件所在包加入 [插件引用配置文件](https://github.com/alibaba/loongcollector/blob/main/plugins.yml) 的 **`common`** / **`linux`** / **`windows`** 等节，执行构建以更新 `plugins/all`。**请勿手改**生成文件。
5. **测试**：编写或补充单元测试、E2E 场景，具体流程与约定见 [单元测试](../../test/unit-test.md) 与 [E2E测试](../../test/e2e-test.md)。
6. **代码规范**：使用 `make lint` 检查代码风格与静态检查项。
7. **提交变更**：提交 Pull Request，并在描述中说明扩展用途、配置示例与测试结果（如有）。
