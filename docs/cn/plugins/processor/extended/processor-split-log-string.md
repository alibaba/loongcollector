# 日志字符串切分

## 简介

`processor_split_log_string`插件可以按照指定分隔符，将单条日志的某个字段切分为多条日志。默认分隔符为换行符 `\n`（未配置时使用 `\0`）。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/processor/split/logstring/split_log_string.go)

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| v1 接口 | v2 接口 | Log | Metric | Span |
| --- | --- | --- | --- | --- |
| ✓ | ✓ | v1&v2 | 透传 | 透传 |

> 该插件仅切分 Log 事件（v2 场景下输入 Log、输出多条 Log）；Metric/Span 事件原样透传，不做处理。

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.0.27 及以上

## 配置参数

| 参数           | 类型   | 是否必选 | 说明                                                                                 |
| -------------- | ------ | -------- | ------------------------------------------------------------------------------------ |
| Type           | string | 是       | 插件类型，固定为 `processor_split_log_string`。                                       |
| SplitKey       | string | 是       | 待切分的字段名。                                                                      |
| SplitSep       | string | 否       | 分隔符。未配置时默认使用 `\0`。                                                       |
| PreserveOthers | bool   | 否       | 是否保留其它字段。默认 `false`。                                                      |
| NoKeyError     | bool   | 否       | 找不到 SplitKey 时是否告警。默认 `false`，表示不告警。                                |

## 样例

采集 `/home/test-log/` 路径下的 `test.log` 文件，将字段 `content` 按换行符切分为多条日志。

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
    SplitKey: content
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
