# 多行合并原生处理插件

## 简介

`processor_merge_multiline_log_native` 插件将多行日志合并为单条事件，支持按容器部分日志标记或自定义正则进行合并。

## 版本

[Stable](../../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  固定为 `processor_merge_multiline_log_native`。 |
|  SourceKey  |  string  |  否  |  content  |  源字段名。 |
|  MergeType  |  string  |  是  |  /  |  合并类型：`flag` 或 `regex`。`flag` 基于容器 runtime 产生的部分日志标记 `P` 合并，`regex` 使用多行正则配置。 |

当 `MergeType=regex` 时，复用多行配置：

| 参数 | 类型 | 是否必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| Multiline.Mode | string | 否 | custom | 可选 `custom`/`JSON`。 |
| Multiline.StartPattern | string | 否 | 空 | 多行起始匹配正则。 |
| Multiline.ContinuePattern | string | 否 | 空 | 多行中间匹配正则。与 Start/End 同时配置将忽略 Continue。 |
| Multiline.EndPattern | string | 否 | 空 | 多行结束匹配正则。仅配置 End 时将从首行开始累积直到匹配 End。 |
| Multiline.UnmatchedContentTreatment | string | 否 | single_line | 不匹配处理：`single_line` 或 `discard`。 |
| IgnoringUnmatchWarning | bool | 否 | false | 不匹配告警是否忽略。 |

## 行为说明

- flag 模式：依赖 `processor_parse_container_log_native` 标记出的部分日志，将同一段连续 `P` 段合并为一条；`F` 段视为完整行。
- regex 模式：按 Start/Continue/End 组合进行切分与合并。
