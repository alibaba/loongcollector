# 锚点

## 简介

`processor_anchor`插件可以通过指定的起始和结束锚点从原始字段中提取子串，并将结果作为新字段写入日志（支持将 JSON 子串展开为多个字段）。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/processor/anchor/anchor.go)

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| v1 接口 | v2 接口 | Log | Metric | Span |
| --- | --- | --- | --- | --- |
| ✓ | ✓ | v1&v2 | 透传 | 透传 |

| LogGroup(v1) | EventTypeLogging | EventTypeMetric | EventTypeSpan |
| ------------ | ---------------- | --------------- | ------------- |
|      ✅      |      ✅           |      透传        |      透传      |

> v2 场景下对每条 Log 事件按起止锚点从 SourceKey 值中提取字段（输入 Log 输出 Log），Metric/Span 事件显式透传（不处理但不丢弃）。

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.0.27 及以上

## 配置参数

| 参数            | 类型             | 是否必选 | 说明                                                                                     |
| --------------- | ---------------- | -------- | ---------------------------------------------------------------------------------------- |
| Type            | string           | 是       | 插件类型，固定为 `processor_anchor`。                                                     |
| SourceKey       | string           | 否       | 待处理的原始字段名。为空时处理日志中的第一个字段。                                        |
| Anchors         | object 数组      | 是       | 锚点配置数组，每个元素定义一次提取规则，具体字段见下表。                                   |
| NoKeyError      | bool             | 否       | 找不到 SourceKey 时是否告警。默认 `false`，表示不告警。                                   |
| NoAnchorError   | bool             | 否       | 找不到起始或结束锚点时是否告警。默认 `false`，表示不告警。                                |
| KeepSource      | bool             | 否       | 是否保留原始字段。默认 `false`，表示提取后删除原始字段。                                  |

`Anchors` 数组中每个锚点对象的字段如下：

| 参数            | 类型   | 是否必选 | 说明                                                                                                                       |
| --------------- | ------ | -------- | -------------------------------------------------------------------------------------------------------------------------- |
| Start           | string | 是       | 起始锚点。从该字符串之后开始截取；为空时表示从字段开头截取。                                                                |
| Stop            | string | 是       | 结束锚点。截取到该字符串之前结束；为空时表示截取到字段末尾。                                                                |
| FieldName       | string | 是       | 提取结果写入的字段名（`FieldType` 为 `json` 时作为展开字段的前缀）。                                                        |
| FieldType       | string | 否       | 提取值的类型，可选 `string` 或 `json`。默认按 `string` 处理。                                                               |
| ExpondJSON      | bool   | 否       | 当 `FieldType` 为 `json` 时，是否将 JSON 展开为多个键值对。默认 `false`。                                                   |
| IgnoreJSONError | bool   | 否       | 解析 JSON 失败时是否忽略错误（不告警）。默认 `false`。                                                                      |
| ExpondConnecter | string | 否       | 展开 JSON 时用于拼接前缀与子键的连接符。默认 `_`。                                                                          |
| MaxExpondDepth  | int    | 否       | JSON 展开的最大深度，`0` 表示不限制（内部按 100 处理），`1` 表示不展开。默认 `0`。                                          |

## 样例

采集 `/home/test-log/` 路径下的 `test.log` 文件，从 `content` 字段中按锚点提取 `时间` 与 `等级` 两个字段。

* 输入

```bash
echo 'time:2022-07-09 level:error msg:hello' >> /home/test-log/test.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths:
      - /home/test-log/*.log
processors:
  - Type: processor_anchor
    SourceKey: content
    KeepSource: true
    Anchors:
      - Start: "time:"
        Stop: " level:"
        FieldName: time
        FieldType: string
      - Start: "level:"
        Stop: " msg:"
        FieldName: level
        FieldType: string
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/test.log",
    "content": "time:2022-07-09 level:error msg:hello",
    "time": "2022-07-09",
    "level": "error",
    "__time__": "1657354602"
}
```
