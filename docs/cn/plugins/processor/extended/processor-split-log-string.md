# 日志字符串切分

## 简介

`processor_split_log_string` 按指定分隔符切分日志正文。正文为空时，使用 `SplitKey` 指定的事件标签。每个非空片段写入一条新日志的正文，其他日志内容随每条输出保留。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/processor/split/logstring/split_log_string.go)

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| v1 接口 | v2 接口 | Log | Metric | Span |
| --- | --- | --- | --- | --- |
| ✓ | ✓ | v1&v2 | 透传 | 透传 |

> 该插件仅切分 Log 事件（从一条 Log 输出多条 Log）；Metric/Span 事件原样透传，不做处理。

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.0.27 及以上

## 配置参数

| 参数                  | 类型   | 是否必选 | 说明 |
| --------------------- | ------ | -------- | ---- |
| Type                  | string | 是       | 插件类型，固定为 `processor_split_log_string`。 |
| SplitKey              | string | 否       | 正文为空时，用作切分来源的事件标签名。 |
| SplitSep              | string | 否       | 分隔符。默认使用换行符 `\n`，显式配置空字符串时使用 `\0`。 |
| PreserveOthers        | bool   | 否       | 是否保留未用作切分来源的事件标签。默认 `true`。正文和标签都无法提供待切分内容时，开启则保留日志，关闭则丢弃。 |
| NoKeyError            | bool   | 否       | 无待切分内容时是否告警。默认 `false`。 |
| EnableLogPositionMeta | bool   | 否       | 旧版 V1 选项。V2 始终输出从零开始的相对字节偏移，仅累加非空片段及其分隔符的长度。 |

## 样例

采集 `/home/test-log/` 路径下的 `test.log` 文件，将日志正文按换行符切分为多条日志。

* 输入

```bash
printf 'line1\nline2\nline3' >> /home/test-log/test.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths:
      - /home/test-log/*.log
processors:
  - Type: processor_split_log_string
    SplitSep: "\n"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{"content": "line1", "__time__": "1657354602"}
{"content": "line2", "__time__": "1657354602"}
{"content": "line3", "__time__": "1657354602"}
```
