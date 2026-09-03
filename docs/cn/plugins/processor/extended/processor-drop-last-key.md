# 丢弃最后字段

## 简介

`processor_drop_last_key`插件用于在日志的字段中命中 `Include` 列表中的任意字段时，丢弃指定的 `DropKey` 字段。常用于处理完成后清理临时字段。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/processor/droplastkey/processor_drop_last_key.go)

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| v1 接口 | v2 接口 | Log | Metric | Span |
| --- | --- | --- | --- | --- |
| ✓ | ✓ | v1&v2 | 透传 | 透传 |

> v2 场景下对每条 Log 事件按同样规则丢弃字段（输入 Log 输出 Log），Metric/Span 事件显式透传（不处理但不丢弃）。

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.0.27 及以上

## 配置参数

| 参数    | 类型     | 是否必选 | 说明                                                                       |
| ------- | -------- | -------- | -------------------------------------------------------------------------- |
| Type    | string   | 是       | 插件类型，固定为 `processor_drop_last_key`。                               |
| Include | string[] | 是       | 触发条件字段列表。当日志中存在其中任意字段时，才执行丢弃动作。             |
| DropKey | string   | 是       | 命中触发条件后待丢弃的字段名。                                             |

## 样例

采集 `/home/test-log/` 路径下的 `test.log` 文件，当日志中包含字段 `a` 时丢弃字段 `b`。

* 输入

```bash
echo 'a=1 b=2' >> /home/test-log/test.log
```

* 采集配置

```yaml
enable: true
inputs:
  - Type: input_file
    FilePaths:
      - /home/test-log/*.log
processors:
  - Type: processor_split_key_value
    Delimiter: " "
    Separator: "="
  - Type: processor_drop_last_key
    Include:
      - a
    DropKey: b
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/test.log",
    "a": "1",
    "__time__": "1657354602"
}
```
