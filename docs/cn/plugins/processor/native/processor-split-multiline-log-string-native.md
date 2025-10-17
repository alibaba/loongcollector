# 多行切分原生处理插件

## 简介

`processor_split_multiline_log_string_native` 插件根据多行正则配置，将一条内容包含多行的字段拆分为多条事件，支持保留为 RawEvent 或 LogEvent。

## 版本

[Stable](../../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  固定为 `processor_split_multiline_log_string_native`。 |
|  SourceKey  |  string  |  否  |  content  |  源字段名，仅支持该字段为唯一字段的日志事件。 |
|  EnableRawContent  |  bool  |  否  |  false  |  是否以 RawEvent 产出内容。 |
|  Multiline.Mode | string | 否 | custom | 可选 `custom`/`JSON`。 |
|  Multiline.StartPattern | string | 否 | 空 | 多行起始匹配正则。 |
|  Multiline.ContinuePattern | string | 否 | 空 | 多行中间匹配正则。与 Start/End 同时配置将忽略 Continue。 |
|  Multiline.EndPattern | string | 否 | 空 | 多行结束匹配正则。仅配置 End 时将从首行开始累积直到匹配 End。 |
|  Multiline.UnmatchedContentTreatment | string | 否 | single_line | 不匹配处理：`single_line` 或 `discard`。 |
|  IgnoringUnmatchWarning | bool | 否 | false | 不匹配告警是否忽略。 |
