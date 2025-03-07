# 自监控告警数据

## 简介

`input_internal_alarms` 插件收集 LoongCollector 自身运行时的告警数据，并以[LogEvent](../../../developer-guide/data-model-cpp.md)的格式暴露出去。

## 版本

[Beta](../../stability-level.md)

## 版本说明

* 推荐版本：LoongCollector v3.0.5 及以上

## 配置参数

关于具体告警的详情，请参见[自监控告警说明](../../../developer-guide/self-monitor/alarms/internal-alarms-description.md)。

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为input\_internal\_alarms。  |

## 样例

采集LoongCollector所有自监控告警，并将采集结果写到本地文件。

``` yaml
enable: true
inputs:
  - Type: input_internal_alarms
flushers:
  - Type: flusher_file
    FilePath: self_monitor/self_alarms.log
```

输出到 LoongCollector 的 `self_monitor/self_alarms.log` 文件中，每行均为一条json格式的告警。
