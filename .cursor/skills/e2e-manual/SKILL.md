---
name: e2e-manual
description: E2E Testing Standards Guide. Use this rule when you need to write or execute E2E tests, including test naming conventions, test workflows, and test behavior templates. This rule should be used for end-to-end integration testing scenarios to ensure test standardization and maintainability. Example: When writing new E2E tests, use this rule to ensure proper test format and workflow compliance.
---
# e2e测试指南

## 测试框架

- E2E（BDD Godog）

## 严格执行

- 禁止改变被测试方法的任何行为
- 禁止更改engine中已有的测试行为
- 若需要记录日志，一定要在生成日志之前启动触发器`When begin trigger`
- 模板在test/engine/steps.go中注册，请务必使用已经注册的测试行为，不要自己生成，且行为类型一定要对应，例如wait {10} seconds，是Then类型行为，而不是When类型行为，因此一定为`Then wait {10} seconds`

## 测试命名规范

- 每个案例必须使用 `@e2e` 注解标记测试方法
- 必须标注环境，`@host`：表示该测试场景在host环境下运行；`@k8s`：表示该测试场景在k8s环境下运行；`@docker-compose`：表示该测试场景在本地启动docker-compose运行。
- 格式：`Test${被测试功能名}${案例简单说明}`
- 示例：`TestInputFileWithBlackListDir`、`TestInputFileWithRegexSingle`

## 测试最佳实践

### 测试流程

- 创建测试环境，`Given {host} environment`，其中host代表构建主机环境，daemonset表示构建k8s环境，docker-compose表示容器环境（只有这个三个值，不要出现其他）
- 启动iLogtail，`Given subcribe data from {sls} with config`，订阅器（Subscribe）插件是测试引擎中用于接收数据的组件，sls表示生成输出到logstore。`Given {regex_single} local config as below`，添加本地配置，启动input插件、processor插件，其中regex_single是配置名称，可根据功能进行构造，此配置中确保含有`enable: true`，否则配置无法执行。
- 触发日志生成，启动日志记录`When begin trigger`，利用现有的框架生成一系列事件，例如`When generate {100} regex logs to file {/tmp/loongcollector/regex_single.log}, with interval {100}ms`为生成文件日志。
- 验证日志内容，框架会获取采集到的日志，通过查询语句获取确定日之后，根据`Then`测试行为进行日志验证。查询语句例如，`When query through {* | select * from e2e where call_name = 'tcp_connect' and arguments like '%http://www.baidu.com%'}`，查询日志call_name为tcp_connect，arguments包含<http://www.baidu.com。>

### 测试重点

- 根据功能要求设计查询语句，以便能够达到测试功能的目的
- 专注于查询的返回值，确保功能符合预期
- 保持测试的简洁性和可维护性
