# 基础聚合

## 简介

`aggregator_base` `aggregator` 插件可以实现对**单条数据**的聚合。


## 事件能力

列含义见 [概览 · 事件能力列说明](../overview.md#事件能力列说明)。

| Log | Metric | Span |
| --- | --- | --- |
| ✓ | ✓ | ✓ |

## 版本

[Stable](../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.0.27 及以上

## 配置参数

| 参数 | 类型 | 是否必选 | 说明 |
| --- | --- | --- | --- |
| Type | string | 是 | 插件类型，指定为`aggregator_base`。 |
| MaxLogGroupCount | int | 否 | 单次提交前缓存的最大事件组数，默认 4。 |
| MaxLogCount | int | 否 | 每个事件组允许的最大事件数，默认 1024。 |
| PackFlag | bool | 否 | 是否为事件组添加 `__pack_id__` Tag，默认开启。 |
| Topic | string | 否 | 输出事件组的 `__topic__` Tag，默认沿用输入事件组中的值。 |

## 样例

采集`/home/test-log/`路径下的所有文件名匹配`*.log`规则的文件，使用基础聚合功能，设定Topic为“file”，并将采集结果发送到SLS。

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/*.log
aggregators:
  - Type: aggregator_base
    Topic: file
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```
