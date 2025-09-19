# 单行切分原生处理插件

## 简介

`processor_split_string_native` 插件按指定分隔字符（默认换行）将单条日志中的多行内容切分为多条事件。可选择以 RawEvent 形式产出。

## 版本

[Stable](../../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  固定为 `processor_split_string_native`。 |
|  SourceKey  |  string  |  否  |  content  |  源字段名，仅支持该字段为唯一字段的日志事件。 |
|  SplitChar  |  int  |  否  |  10 (`\n`) |  切分字符（ASCII/字节值）。 |
|  EnableRawContent  |  bool  |  否  |  false  |  是否以 RawEvent 产出内容，否则以 LogEvent 产出并复制时间戳与位置信息。 |


