# CSV

## 简介

`processor_csv`插件可以将指定字段的值按 CSV 格式解析，并将解析出的各字段以指定的键名写入日志。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/processor/csv/processor_csv.go)

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| v1 接口 | v2 接口 | Log | Metric | Span |
| --- | --- | --- | --- | --- |
| ✓ | ✓ | v1&v2 | 透传 | 透传 |

| LogGroup(v1) | EventTypeLogging | EventTypeMetric | EventTypeSpan |
| ------------ | ---------------- | --------------- | ------------- |
|      ✅      |      ✅           |      透传        |      透传      |

> v2 场景下对每条 Log 事件将 SourceKey 值按 CSV 解析为多个字段（输入 Log 输出 Log），Metric/Span 事件显式透传（不处理但不丢弃）。

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.1.0 及以上

## 配置参数

| 参数             | 类型     | 是否必选 | 说明                                                                                                   |
| ---------------- | -------- | -------- | ------------------------------------------------------------------------------------------------------ |
| Type             | string   | 是       | 插件类型，固定为 `processor_csv`。                                                                      |
| SourceKey        | string   | 是       | 待解析 CSV 记录的原始字段名。                                                                           |
| SplitKeys        | string数组 | 是     | 与解析出的 CSV 字段一一对应的键名列表。                                                                 |
| SplitSep         | string   | 否       | CSV 分隔符，必须为单个字符。默认 `,`。                                                                  |
| NoKeyError       | bool     | 否       | 找不到 SourceKey 时是否告警。默认 `false`，表示不告警。                                                 |
| TrimLeadingSpace | bool     | 否       | 是否忽略每个 CSV 字段前的空白字符。默认 `false`。                                                       |
| PreserveOthers   | bool     | 否       | 当 CSV 字段数多于 SplitKeys 时，是否保留多余部分。默认 `false`。                                        |
| ExpandOthers     | bool     | 否       | 当 `PreserveOthers=true` 时，是否将多余字段继续展开。默认 `false`（不展开则整体存入 `_decode_preserve_`）。 |
| ExpandKeyPrefix  | string   | 否       | 当 `ExpandOthers=true` 时必填，作为存放多余字段的键名前缀（如 `expand_1`、`expand_2`）。                |
| KeepSource       | bool     | 否       | 解析成功后是否保留原始字段。默认 `false`。                                                              |

## 样例

采集 `/home/test-log/` 路径下的 `test.log` 文件，将字段 `content` 按逗号解析为 `time`、`level`、`msg` 三个字段。

* 输入

```bash
echo '2022-07-09,error,hello world' >> /home/test-log/test.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths:
      - /home/test-log/*.log
processors:
  - Type: processor_csv
    SourceKey: content
    SplitSep: ","
    SplitKeys:
      - time
      - level
      - msg
    KeepSource: true
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/test.log",
    "content": "2022-07-09,error,hello world",
    "time": "2022-07-09",
    "level": "error",
    "msg": "hello world",
    "__time__": "1657354602"
}
```
