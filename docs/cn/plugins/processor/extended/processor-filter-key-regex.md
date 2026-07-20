# 按字段名过滤

## 简介

`processor_filter_key_regex`插件可以按字段名（Key）的正则表达式对日志进行过滤，只有满足 Include 且不满足 Exclude 条件的日志才会被保留。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/processor/filter/keyregex/processor_filter_key_regex.go)

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| v1 接口 | v2 接口 | Log | Metric | Span |
| --- | --- | --- | --- | --- |
| ✓ | ✓ | v1&v2 | 透传 | 透传 |

| LogGroup(v1) | EventTypeLogging | EventTypeMetric | EventTypeSpan |
| ------------ | ---------------- | --------------- | ------------- |
|      ✅      |      ✅           |      透传        |      透传      |

> v2 场景下按字段名正则对 Log 事件进行过滤，不匹配的 Log 事件会被丢弃，Metric/Span 事件显式透传（不处理也不丢弃）。

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.0.27 及以上

## 配置参数

| 参数    | 类型       | 是否必选 | 说明                                                                                             |
| ------- | ---------- | -------- | ------------------------------------------------------------------------------------------------ |
| Type    | string     | 是       | 插件类型，固定为 `processor_filter_key_regex`。                                                   |
| Include | string数组 | 否       | 保留条件。日志需对列表中每个正则都能匹配到至少一个字段名，否则该日志被丢弃。默认不限制。          |
| Exclude | string数组 | 否       | 排除条件。日志中只要有任一字段名匹配列表中任一正则，该日志即被丢弃。默认不排除。                  |

## 样例

采集 `/home/test-log/` 路径下的 `test.log` 文件，只保留包含以 `key` 开头字段名的日志。

* 输入

```bash
echo '{"key1":"a","key2":"b"}' >> /home/test-log/test.log
echo '{"other":"c"}' >> /home/test-log/test.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths:
      - /home/test-log/*.log
processors:
  - Type: processor_json
    SourceKey: content
    KeepSource: false
    ExpandDepth: 1
    ExpandConnector: ""
  - Type: processor_filter_key_regex
    Include:
      - "key.*"
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/test.log",
    "key1": "a",
    "key2": "b",
    "__time__": "1657354602"
}
```

> 说明：仅第一条日志（包含 `key1`、`key2` 字段）满足 Include 条件被保留，第二条日志（仅含 `other` 字段）被丢弃。
