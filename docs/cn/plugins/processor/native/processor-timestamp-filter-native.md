# 时间戳过滤原生处理插件

## 简介

`processor_timestamp_filter_native`插件根据时间戳过滤事件，丢弃不在指定时间范围内的事件。插件支持从事件的时间戳字段或事件自身的`__time__`字段获取时间戳，并根据配置的上界和下界进行过滤。

源代码：[ProcessorTimestampFilterNative.cpp](https://github.com/alibaba/loongcollector/blob/main/core/plugin/processor/ProcessorTimestampFilterNative.cpp)

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：【待发布】

## 配置参数

| **参数** | **类型** | **是否必填** | **默认值** | **说明** |
| --- | --- | --- | --- | --- |
| Type | string | 是 | / | 插件类型。固定为`processor_timestamp_filter_native`。 |
| SourceKey | string | 否 | `""` | 时间字段名。如果为空，则使用事件自身的`__time__`字段（即事件时间戳）。如果指定了字段名，则从该字段读取时间戳字符串。时间戳格式必须为数字（秒、毫秒或纳秒），处理时会进行检查。 |
| TimestampPrecision | string | 否 | `"second"` | 时间戳精度。可选值：<br>- `"second"`：秒级时间戳<br>- `"millisecond"`：毫秒级时间戳<br>- `"nanosecond"`：纳秒级时间戳<br>该参数同时影响`SourceKey`字段的时间戳解析和`UpperBound`、`LowerBound`的精度。 |
| UpperBound | Long | 否 | `max timestamp` | 上界时间戳。超过该时间的事件将被丢弃。默认值为最大时间戳值（即不过滤）。 |
| LowerBound | Long | 否 | `0` | 下界时间戳。早于该时间的事件将被丢弃。默认值为0。 |

## 使用说明

1. **时间戳来源**：
   * 如果`SourceKey`为空，插件使用事件自身的`__time__`字段（事件时间戳）进行过滤。
   * 如果`SourceKey`不为空，插件从指定字段读取时间戳字符串并解析。时间戳必须是纯数字格式。

2. **时间戳精度**：
   * `TimestampPrecision`参数同时影响`SourceKey`字段的时间戳解析和`UpperBound`、`LowerBound`的精度。
   * 例如，如果`TimestampPrecision`为`"millisecond"`，则`SourceKey`字段的值会被视为毫秒级时间戳，`UpperBound`和`LowerBound`的值也会被当作毫秒级时间戳处理。

3. **边界处理**：
   * 时间戳等于`LowerBound`或`UpperBound`的事件会被保留（闭区间）。
   * 时间戳小于`LowerBound`或大于`UpperBound`的事件会被丢弃。

4. **错误处理**：
   * 如果`SourceKey`字段不存在，事件会被保留（不丢弃）。
   * 如果`SourceKey`字段的值无法解析为有效的时间戳，事件会被保留（不丢弃）。

## 样例

### 样例1：使用事件时间戳过滤

采集文件`/home/test-log/app.log`，只保留时间戳在指定范围内的事件。

* 输入

```plain
2024-01-15 10:00:00 [INFO] Application started
2024-01-15 10:05:00 [INFO] Processing request
2024-01-15 10:10:00 [INFO] Request completed
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/app.log
processors:
  - Type: processor_parse_timestamp_native
    SourceKey: content
    SourceFormat: '%Y-%m-%d %H:%M:%S'
  - Type: processor_timestamp_filter_native
    LowerBound: 1705286400
    UpperBound: 1705287000
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/app.log",
    "content": "2024-01-15 10:05:00 [INFO] Processing request",
    "__time__": "1705286700"
}
```

### 样例2：使用字段时间戳过滤

采集包含时间戳字段的日志，根据时间戳字段进行过滤。

* 输入

```json
{"timestamp": "1705286400", "message": "Event 1", "level": "INFO"}
{"timestamp": "1705287000", "message": "Event 2", "level": "INFO"}
{"timestamp": "1705287600", "message": "Event 3", "level": "INFO"}
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/events.log
processors:
  - Type: processor_parse_json_native
    SourceKey: content
  - Type: processor_timestamp_filter_native
    SourceKey: timestamp
    TimestampPrecision: second
    LowerBound: 1705286400
    UpperBound: 1705287000
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/events.log",
    "timestamp": "1705286400",
    "message": "Event 1",
    "level": "INFO",
    "__time__": "1234567890"
}
{
    "__tag__:__path__": "/home/test-log/events.log",
    "timestamp": "1705287000",
    "message": "Event 2",
    "level": "INFO",
    "__time__": "1234567890"
}
```

### 样例3：使用毫秒级时间戳过滤

处理包含毫秒级时间戳的日志。

* 输入

```json
{"ts": "1705286400000", "message": "Event with millisecond timestamp"}
{"ts": "1705287000000", "message": "Event with millisecond timestamp 2"}
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths: 
      - /home/test-log/ms-events.log
processors:
  - Type: processor_parse_json_native
    SourceKey: content
  - Type: processor_timestamp_filter_native
    SourceKey: ts
    TimestampPrecision: millisecond
    LowerBound: 1705286400000
    UpperBound: 1705287000000
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/ms-events.log",
    "ts": "1705286400000",
    "message": "Event with millisecond timestamp",
    "__time__": "1234567890"
}
{
    "__tag__:__path__": "/home/test-log/ms-events.log",
    "ts": "1705287000000",
    "message": "Event with millisecond timestamp 2",
    "__time__": "1234567890"
}
```
