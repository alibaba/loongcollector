# E2E测试——如何添加新的测试行为

iLogtail提供了一个完整的E2E测试引擎，方便您快速开展集成测试，从而进一步保证代码质量。在大部分情况下，您只需要编写一个配置文件来定义测试行为，即可轻松完成测试。

## 目前支持的测试行为

| 行为类型 | 模板 | 参数 | 说明 |
| --- | --- | --- | --- |
| Given | ^\{(\S+)\} environment$ | 环境类型 | 初始化远程测试环境 |
| Given | ^iLogtail depends on containers \{(.*)\} | 容器 | iLogtail依赖容器，可多次执行，累积添加 |
| Given | ^iLogtail expose port \{(.*)\} to \{(.*)\} | 端口号 | iLogtail暴露端口，可多次执行，累积添加 |
| Given | ^\{(.*)\} local config as below | 1. 配置名 2. 配置文件内容 | 添加本地配置 |
| Given | ^\{(.*)\} http config as below | 1. 配置名 2. 配置文件内容 | 通过http添加配置 |
| Given | ^remove http config \{(.*)\} | 配置名 | 通过http移除配置 |
| Given | ^subcribe data from \{(\S+)\} with config | 1. 数据源 2. 配置文件内容 | 订阅数据源 |
| When | ^generate \{(\d+)\} regex logs, with interval \{(\d+)\}ms$ | 1. 生成日志数量 2. 生成日志间隔 | 生成正则文本日志（路径为/tmp/ilogtail/regex_single.log） |
| When | ^generate \{(\d+)\} http logs, with interval \{(\d+)\}ms, url: \{(.*)\}, method: \{(.*)\}, body: | 1. 生成日志数量 2. 生成日志间隔 3. url 4. method 5. body | 生成http日志，发送到iLogtail input_http_server |
| When | ^add k8s label \{(.*)\} | k8s标签 | 为k8s资源添加标签 |
| When | ^remove k8s label \{(.*)\} | k8s标签 | 为k8s资源移除标签 |
| When | ^start docker-compose dependencies \{(\S+)\} | 依赖服务 | 启动docker-compose依赖服务 |
| Then | ^there is \{(\d+)\} logs$ | 日志数量 | 验证日志数量 |
| Then | ^there is at least \{(\d+)\} logs$ | 日志数量 | 验证日志数量 |
| Then | ^there is at least \{(\d+)\} logs with filter key \{(.*)\} value \{(.*)\}$ | 1. 日志数量 2. 过滤键 3. 过滤值 | 验证日志数量 |
| Then | ^the log contents match regex single |  | 验证正则文本日志内容 |
| Then | ^the log fields match kv | kv键值对列表 | 验证kv日志内容 |
| Then | ^the log tags match kv | kv键值对列表 | 验证kv日志标签内容 |
| Then | ^the context of log is valid$ | | 验证日志上下文 |
| Then | ^the log fields match | 日志字段列表 | 验证日志字段内容 |
| Then | ^the log labels match | 日志标签列表 | 验证日志标签内容 |
| Then | ^the logtail log contains \{(\d+)\} times of \{(.*)\}$ | 1. 匹配次数 2. 匹配内容 | 验证日志内容 |
| Then | wait \{(\d+)\} seconds | 等待时间 | 等待时间 |

## 如何添加新的测试行为

在某些情况下，需要对engine中的测试行为进行拓展，可以参考下面的添加指南。

### 1. 编写行为函数

如果您需要添加新的行为函数，可以在`engine`目录下添加一个Go函数。不同目录下的行为函数的职责有所不同：
- `cleanup`：清理测试环境，其中的测试函数会默认在测试结束后执行。无需在配置文件中显式声明使用。
- `control`：管控相关的行为函数，如初始化环境、添加配置等。
- `setup`：初始化测试环境，并提供远程调用的相关功能。
- `trigger`：数据生成相关的行为函数，如生成日志等。
- `verify`：验证相关的行为函数，如验证日志数量、验证日志内容等。

每个行为函数的接口定义如下所示：

```go
func LogCount(ctx context.Context, expect int) (context.Context, error) {
    // your code here
}
```

函数的第一个参数必须为`context.Context`。除此之外，后续可添加任意多个参数。函数的返回值为`context.Context`和`error`，其中`context.Context`为传递给下一个行为函数的参数，`error`为错误信息。一些需要在多个行为函数之间传递的参数，可以通过`context.Context`来传递。

```go
return context.WithValue(ctx, key, value), nil
```

### 2. 注册行为函数

在`test/cases/core/main_test.go`中，您需要注册您的行为函数。注册函数的格式如下所示：

```go
func scenarioInitializer(ctx *godog.ScenarioContext) {
	// Given

	// When

	// Then
	ctx.Then(`^there is \{(\d+)\} logs$`, verify.LogCount)
}
```

您需要根据行为的类型，将行为函数注册到对应的位置。在`Given`中注册`setup`中的行为函数，在`When`中注册`trigger`中的行为函数，在`Then`中注册`verify`中的行为函数。`control`中的行为函数比较灵活，根据函数职责不同，注册到不同的类型中。

在注册时，您需要定义一个正则表达式，用于匹配配置文件中的行为。在正则表达式中，您可以使用`{}`来标识参数，这些参数将会传递给行为函数。例如：

```go
ctx.Then(`^there is \{(\d+)\} logs$`, verify.LogCount)
```

能够从配置文件中匹配到`there is {100} logs`这样的行为，并将`100`作为参数传递给`LogCount`函数。

### 3. 在配置文件中使用

在配置文件中，您可以直接使用您定义的行为函数。例如：

```plain
Then there is {100} logs
```

在运行测试时，测试框架会根据配置文件中的行为，调用对应的行为函数，并传递参数。