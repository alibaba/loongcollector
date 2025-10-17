# 容器日志解析原生处理插件

## 简介

`processor_parse_container_log_native` 插件解析容器 runtime 输出的日志，支持 containerd text 与 docker json-file 两种格式，提取时间、流类型以及正文，并可根据流类型选择性过滤。

## 版本

[Stable](../../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  固定为 `processor_parse_container_log_native`。 |
|  SourceKey  |  string  |  否  |  content  |  源字段名（原始行）。 |
|  IgnoringStdout  |  bool  |  否  |  false  |  是否忽略 stdout 流。 |
|  IgnoringStderr  |  bool  |  否  |  false  |  是否忽略 stderr 流。 |
|  IgnoreParseWarning | bool | 否 | false | 解析失败时是否忽略告警。 |
|  KeepingSourceWhenParseFail | bool | 否 | false | 解析失败时是否保留源字段。 |

## 产出字段

- `_time_`: 容器日志时间
- `_source_`: 流类型，`stdout` 或 `stderr`
- `content`: 日志正文（去除结尾换行）
- 对于 containerd `P`（部分）日志，插件会打上标记，供 `processor_merge_multiline_log_native` 合并
