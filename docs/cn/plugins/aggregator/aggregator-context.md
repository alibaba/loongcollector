# 上下文聚合

## 简介

`aggregator_context` `aggregator` 插件可以根据数据来源对**单条数据**进行聚合。


## 事件能力

列含义见 [概览 · 事件能力列说明](../overview.md#事件能力列说明)。

| Log | Metric | Span |
| --- | --- | --- |
| ✓ | ✓ | ✓ |

## 版本

[Beta](../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.2.0 及以上

## 配置参数

| 参数 | 类型 | 是否必选 | 说明 |
| --- | --- | --- | --- |
| Type | string | 是 | 插件类型，指定为`aggregator_context`。 |
| MaxLogGroupCount | int | 否 | 每个来源在单次提交前可缓存的最大事件组数，默认 2。 |
| MaxLogCount | int | 否 | 每个事件组允许的最大事件数，默认 1024。 |
| PackFlag | bool | 否 | 是否为事件组添加 `__pack_id__` Tag，默认开启。 |
| Topic | string | 否 | 输出事件组的 Topic。输入组的 `__topic__` Tag 或 `topic` Metadata 优先于此配置。 |

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
