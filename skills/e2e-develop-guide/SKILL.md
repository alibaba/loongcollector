---
name: e2e-develop-guide
description: LoongCollector E2E Testing Framework Development Guide. Use this rule when you need to understand E2E testing framework architecture, develop new test behavior functions, or extend testing capabilities. Contains detailed BDD testing framework usage, test behavior function development and registration workflows, and comprehensive reference of existing testing capabilities. Example: When adding custom test behaviors for new plugins, use this rule to understand how to write and register new test functions. Example: When developing E2E tests, use this rule to understand the e2e framework.
---
# LoongCollector E2E 本地开发手册

## 背景

E2E测试采用行为驱动开发（Behavior-Driven Development）的设计思路，通过定义一系列测试行为，并通过配置文件的方式来描述测试场景，从而实现对插件的集成测试。测试引擎会根据配置文件中的内容，正则匹配对应的函数，并解析配置文件中的参数，传递给对应的函数。从而完成自动创建测试环境、启动iLogtail、触发日志生成、验证日志内容等一系列操作，最终输出测试报告。

## 开发（编写配置文件）

对于每一个新的功能，您都需要在 test 不同目录下创建一个新的目录，其中包含了 feature 格式的配置文件。每个配置文件中可以包含多个测试场景，每个测试场景由一个或多个步骤组成。

* e2e/test\_cases：开源 Go 插件测试（Docker Compose 环境运行）

* e2e\_enterprise/test\_cases：商业版测试（主机+K8s 环境运行，静态，不涉及环境变化）

* e2e\_enterprise/regression：商业版测试（主机+K8s 环境运行，动态，涉及环境变化）

```protobuf
test
  e2e
    ...
  e2e_enterprise
    test_cases
      input_file
        case.feature
  regression
    test_cases
      ...
```

配置文件的基本框架如下所示：

```plaintext
@input
Feature: input file
  Test input file

  @e2e @host
  Scenario: TestInputFileWithRegexSingle
    Given {host} environment
    Given subcribe data from {sls} with config
    """
    """
    Given {regex_single} local config as below
    """
    enable: true
    inputs:
      - Type: input_file
        FilePaths:
          - /tmp/ilogtail/**/regex_single.log*
    processors:
      - Type: processor_parse_regex_native
        SourceKey: content
        Regex: (\S+)\s(\w+):(\d+)\s(\S+)\s-\s\[([^]]+)]\s"(\w+)\s(\S+)\s([^"]+)"\s(\d+)\s(\d+)\s"([^"]+)"\s(.*)
        Keys:
          - mark
          - file
          - logNo
          - ip
          - time
          - method
          - url
          - http
          - status
          - size
          - userAgent
          - msg
    """
    When generate {100} regex logs to file {/tmp/ilogtail/regex_single.log}, with interval {100}ms
    Then there is {100} logs
    Then the log fields match regex single
```

* `Feature`定义了一个测试功能，下面为这个功能的描述信息。在`Feature`下可以定义多个测试场景。

* `Scenario`定义了一个测试场景。在`Scenario`下可以定义多个行为。

* 行为定义分为三类：

  * `Given`：定义了一些准备测试条件的行为。

  * `When`：定义了一些触发测试条件的行为。

  * `Then`：定义了一些验证测试条件的行为。

  * 行为中使用`{}`作为标识符，该部分内容将作为参数，传递给对应的Go函数。

* `@`表示一个tag，在运行测试时，会根据tag的不同，分别运行。除了自定义的tag外，测试框架定义了一些默认的tag：

  * `@e2e`：表示该测试场景为E2E测试。

  * `@regression`：表示该测试场景为回归测试。

  * `@host`：表示该测试场景在host环境下运行。

  * `@k8s`：表示该测试场景在k8s环境下运行。

  * `@docker-compose`：表示该测试场景在本地启动docker-compose运行

现有的测试能力可以参考附录中的表格。如果现有的行为不能满足需求，可以参考后续的"如何添加新的测试行为"添加自己需要的函数。

## 如何添加新的测试行为

在某些情况下，需要对engine中的测试行为进行拓展，可以参考下面的添加指南。

### 1. 编写行为函数

如果您需要添加新的行为函数，可以在`engine`目录下添加一个Go函数。不同目录下的行为函数的职责有所不同：

* `cleanup`：清理测试环境，其中的测试函数会默认在测试结束后执行。无需在配置文件中显式声明使用。

* `control`：管控相关的行为函数，如初始化环境、添加配置等。

* `setup`：初始化测试环境，并提供远程调用的相关功能。

* `trigger`：数据生成相关的行为函数，如生成日志等。

* `verify`：验证相关的行为函数，如验证日志数量、验证日志内容等。

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

在`test/e2e_enterprise/main_test.go`中，您需要注册您的行为函数。注册函数的格式如下所示：

```go
func scenarioInitializer(ctx *godog.ScenarioContext) {
    // Given

    // When

    // Then
    ctx.Then(`^there is \{(\d+)\} logs$`, verify.LogCount)
}
```

您需要根据行为的类型，将行为函数注册到对应的位置。在`Given`中注册`setup`中的行为函数，在`When`中注册`trigger`中的行为函数，在`Then`中注册`verify`中的行为函数。`control`中的行为函数比较灵活，根据函数职责不同，注册到不同的类型中。

在注册时，您需要定义一个正则表达式，用于匹配配置文件中的行为。在正则表达式中，您可以使用\`{}\`来标识参数，这些参数将会传递给行为函数。例如：

```go
ctx.Then(`^there is \{(\d+)\} logs$`, verify.LogCount)
```

能够从配置文件中匹配到`there is {100} logs`这样的行为，并将`100`作为参数传递给`LogCount`函数。

### 3. 在配置文件中使用

在 feature 配置文件中，您可以直接使用您定义的行为函数。例如：

```protobuf
Then there is {100} logs
```

在运行测试时，测试框架会根据配置文件中的行为，调用对应的行为函数，并传递参数。

## 附录

现有测试行为（部分）。可以参考 `test/engine/steps.go` 中的定义，目前支持的测试行为如下：

| 行为类型 | 模板 | 参数 | 说明 |
| --- | --- | --- | --- |
| Given | ^{(\S+)} environment$ | 环境类型 | 初始化远程测试环境 |
| Given | ^{(.*)} local config as below | 1. 配置名 2. 配置文件内容 | 添加本地配置 |
| Given | ^subcribe data from {(\S+)} with config | 1. 数据源 2. 配置文件内容 | 订阅数据源 |
| When | ^begin trigger | 无 | 触发日志采集，记录开始时间 |
| When | ^generate {(\d+)} regex logs, with interval {(\d+)}ms$ | 1. 生成日志数量 2. 生成日志间隔 | 生成正则文本日志（路径为/tmp/ilogtail/regex\_single.log） |
| When | ^generate {(\d+)} http logs, with interval {(\d+)}ms, url: {(.)}, method: {(.)}, body: | 1. 生成日志数量 2. 生成日志间隔 3. url 4. method 5. body | 生成http日志，发送到LoongCollector input\_http\_server |
| Then | ^there is {(\d+)} logs$ | 日志数量 | 验证日志数量 |
| Then | ^there is at least {(\d+)} logs$ | 日志数量 | 验证日志数量 |
| Then | wait {(\d+)} seconds | 等待时间 | 等待时间 |
| When | ^execute \{(\d+)\} commands \{(.*)\} in sequence | 1.执行次数 2.执行命令 | 在数据来源机器上，执行指定次数的命令 |
| When | ^create the shell script file \{(\S+)\} with the following content | 1.shell文件名称（会自动添加.sh后缀）2.shell文件内容 | 创建shell文件，并赋权可执行 |
| When | delete the shell script file \{(\S+)\} | shell文件名称 | 删除shell文件 |
| When | execute \{(\d+)\} the shell script file \{(\S+)\} in parallel | 1.执行次数 2.执行的shell文件名称 | 执行shell脚本 |

⚠️注意：

（1）日志数量验证

对于日志数量的验证，最高为100。

| Then | ^there is {(\d+)} logs$ | 日志数量 | 验证日志数量 |
| --- | --- | --- | --- |

如果超过100，请用以下行为函数：

在查询的时候用count(1)，然后在结果验证的时候`Then the log fields match kv`。

```plaintext
    When query through {* | select count(1) as cnt from e2e where call_name = 'execve' and arguments = 'loongcollector-e2e-test' and binary = '/usr/bin/grep'}
    Then wait {10} seconds
    Then there is {1} logs
    Then the log fields match kv
    """
    cnt: "1000"
    """
```
