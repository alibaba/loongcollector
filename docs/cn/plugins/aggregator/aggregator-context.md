# 上下文聚合

## 简介

`aggregator_context` `aggregator`插件可以实现根据日志来源对单条日志进行聚合。

## 版本

[Beta](../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.2.0 及以上

## 配置参数

| 参数 | 类型 | 是否必选 | 说明 |
| --- | --- | --- | --- |
| Type | String | 是 | 插件类型，指定为`aggregator_context`。 |
| MaxLogGroupCount | Int | 否 | 在执行Flush之前，每一个日志来源所允许存在的最大LogGroup数量。如果未添加该参数，则默认每一个日志来源最多允许存在2个LogGroup。 |
| MaxLogCount | Int | 否 | 每个LogGroup最多可包含的日志条数。如果未添加该参数，则默认每个LogGroup最多可包含1024条日志。 |
| PackFlag | Boolean | 否 | 是否需要在LogGroup的LogTag中添加__pack_id__字段。如果未添加改参数，则默认在LogGroup的LogTag中添加__pack_id__字段。 |
| Topic | String | 否 | 额外设置的LogGroup的Topic名。如果未添加该参数，则每个LogGroup的Topic名默认值如下：<li>空，如果input插件不提供设置Topic的能力；<li>input插件中设置的topic名称，如果input插件提供设置Topic的能力。 |

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，使用上下文聚合功能，将来源于同一文件的日志聚合在一起，并将采集结果发送到SLS。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
aggregators:
  - Type: aggregator_context
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```
