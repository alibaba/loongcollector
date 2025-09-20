# 飞天解析原生处理插件

## 简介

`processor_parse_apsara_native` 插件解析飞天格式日志行，提取时间并拆解基础字段（如 level、thread、file、line），并将剩余 `key:value` 形式字段写入事件。

## 版本

[Stable](../../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为 `processor_parse_apsara_native`。  |
|  SourceKey  |  string  |  是  |  /  |  源字段名（整行 Apsara 文本）。  |
|  Timezone  |  string  |  否  |  空  |  日志时间所属时区，格式 `GMT+HH:MM` 或 `GMT-HH:MM`。  |
|  KeepingSourceWhenParseFail  |  bool  |  否  |  false  |  解析失败时是否保留源字段。  |
|  KeepingSourceWhenParseSucceed  |  bool  |  否  |  false  |  解析成功时是否保留源字段。  |
|  RenamedSourceKey  |  string  |  否  |  `SourceKey` |  当保留源字段时用于存储的字段名，默认与 SourceKey 相同。  |
|  CopingRawLog  |  bool  |  否  |  false  |  解析失败且保留源字段时，是否复制原文到 `__raw_log__`。  |

## 产出字段

- `__time__`: 解析出的事件时间（秒/纳秒精度）
- `__LEVEL__`, `__THREAD__`, `__FILE__`, `__LINE__`: 飞天基础字段（若存在）
- 其余 `key:value` 形式的键值对将作为普通字段写入

## 示例

输入（飞天格式）：

```plain
[2024-06-01 12:00:00.123456] [INFO] [17283] [main.cpp:123]\tkey1:val1\tkey2:val2
```

配置：

```yaml
processors:
  - Type: processor_parse_apsara_native
    SourceKey: content
    Timezone: GMT+08:00
    KeepingSourceWhenParseSucceed: false
```
