# 如何开发Extension插件

Extension 插件可以用来注册自定义的能力和接口实现，这些能力和接口实现，可以在其他插件(input、processor、aggregator、flusher)中引用。

## Extension 接口定义

Extension 插件的作用是提供一个通用的注册特定能力的方式(通常为特定接口的实现)，Extension 被注册后，在pipeline中的其他插件内可以被依赖并type-cast成特定的接口。

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

Extension 的开发分为以下步骤：

1. 创建Issue，描述开发插件功能，会有社区同学参与讨论插件开发的可行性，如果社区review 通过，请参考步骤2继续进行。
2. 实现 Extension 接口(注：这里除实现上述通用的Extension接口外，还应实现该 Extension 希望提供的能力对应的接口方法)，这里我们使用样例模式进行介绍，如 [extension/basicauth](https://github.com/alibaba/ilogtail/blob/main/plugins/extension/basicauth/basicauth.go) 这个 Extension 实现了 [extensions.ClientAuthenticator](https://github.com/alibaba/ilogtail/blob/main/pkg/pipeline/extensions/authenticator.go) 这个接口。
3. 通过init将插件注册到[Extensions](https://github.com/alibaba/ilogtail/blob/main/plugin.go)，Extension 插件的注册名（即json配置中的plugin_type）必须以"ext_"开头，详细样例请查看[extension/basicauth](https://github.com/alibaba/ilogtail/blob/main/plugins/extension/basicauth/basicauth.go)。
4. 将插件加入[插件引用配置文件](https://github.com/alibaba/ilogtail/blob/main/plugins.yml)的`common`配置节, 如果仅运行于指定系统，请添加到`linux`或`windows`配置节.
5. 进行单测或者E2E测试，请参考[如何使用单测](../test/unit-test.md) 与 [如何使用E2E测试](../test/e2e-test.md).
6. 使用 *make lint* 检查代码规范。
7. 提交Pull Request。
