# 保留/剔除字段

## 简介

`processor_pick_key`插件可以按字段名保留（Include）或剔除（Exclude）日志中的指定字段。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/processor/pickkey/processor_pick_key.go)

## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| v1 接口 | v2 接口 | Log | Metric | Span |
| --- | --- | --- | --- | --- |
| ✓ | ✓ | v1&v2 | 透传 | 透传 |

> v2 场景下按字段名保留或剔除 Log 事件的字段，字段被全部剔除后该 Log 事件会被丢弃（与 v1 一致），Metric/Span 事件显式透传（不处理也不丢弃）。

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.0.27 及以上

## 配置参数

| 参数    | 类型       | 是否必选 | 说明                                                                                     |
| ------- | ---------- | -------- | ---------------------------------------------------------------------------------------- |
| Type    | string     | 是       | 插件类型，固定为 `processor_pick_key`。                                                   |
| Include | string数组 | 否       | 需要保留的字段名列表。配置后仅保留列表中的字段。                                          |
| Exclude | string数组 | 否       | 需要剔除的字段名列表。配置后剔除列表中的字段。                                            |

> 说明：`Include` 与 `Exclude` 可同时配置，先按 `Include` 保留、再按 `Exclude` 剔除；至少配置其中一项才有过滤效果。

## 样例

采集 `/home/test-log/` 路径下的 `test.log` 文件，仅保留 `time` 与 `level` 字段。

* 输入

```bash
echo '{"time":"2022-07-09","level":"error","msg":"hello"}' >> /home/test-log/test.log
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
  - Type: processor_pick_key
    Include:
      - time
      - level
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```

* 输出

```json
{
    "__tag__:__path__": "/home/test-log/test.log",
    "time": "2022-07-09",
    "level": "error",
    "__time__": "1657354602"
}
```
