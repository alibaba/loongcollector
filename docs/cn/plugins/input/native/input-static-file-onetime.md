# 一次性文件采集（原生插件）

## 简介

`input_static_file_onetime` 插件对匹配到的文件集合执行一次性读取并输出，适用于历史文件或批量补采场景。

## 版本

[Beta](../../stability-level.md)

## 配置参数

|  参数  |  类型  |  必填  |  默认值  |  说明  |
| --- | --- | --- | --- | --- |
| Type | string | 是 | / | 固定为 `input_static_file_onetime` |
| FilePaths | [string] | 是 | / | 文件路径，支持 `*` 与 `**`，`**` 仅用于文件前缀的目录匹配且最多出现一次 |
| MaxDirSearchDepth | uint | 否 | 0 | `**` 的最大目录递归深度 |
| ExcludeFilePaths | [string] | 否 | 空 | 文件路径黑名单（绝对路径，支持 `*`） |
| ExcludeFiles | [string] | 否 | 空 | 文件名黑名单（支持 `*`） |
| ExcludeDirs | [string] | 否 | 空 | 目录黑名单（绝对路径，支持 `*`） |
| FileEncoding | string | 否 | utf8 | 文件编码，支持 utf8、gbk |
| TailSizeKB | uint | 否 | 1024 | 初次生效时相对文件结尾的起始偏移（KB），小于文件大小时从尾部回溯 |
| Multiline | object | 否 | 空 | 多行聚合选项，支持 custom 与 JSON，见下表 |
| Tags | map[string]string | 否 | 空 | Tag 重命名/删除配置，含 `__default__` 语义 |

多行聚合选项：

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| Mode | string | 否 | custom | custom 或 JSON |
| StartPattern | string | 当 Mode=custom 时至少填一项 | 空 | 行首正则 |
| ContinuePattern | string |  | 空 | 行续正则 |
| EndPattern | string |  | 空 | 行尾正则 |
| UnmatchedContentTreatment | string | 否 | single_line | discard 或 single_line |

注意：如果配置发生变化将重置文件集合和读取进度，重启开始一次性采集任务；配置启用后，文件集合会在启动阶段计算并读取，后续新增文件不会纳入集合。

## 样例

```yaml
inputs:
  - Type: input_static_file_onetime
    FilePaths:
      - /data/log/**/*.log
    MaxDirSearchDepth: 5
    Multiline:
      StartPattern: ^\d{4}-\d{2}-\d{2} .*
```
