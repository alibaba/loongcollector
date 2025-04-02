# 本地文件

## 简介

`flusher_file` `flusher`插件将采集到的数据写入本地文件中。flusher\_file插件使用[spdlog](https://github.com/gabime/spdlog)库实现，所以写入的文件具有部分日志文件的特征，例如存在大小限制、会自动轮转。

## 版本

[Alpha](../../stability-level.md)

## 版本说明

* 推荐版本：LoongCollector v3.0.5 及以上

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为flusher\_file。  |
|  FilePath  |  string  |  是  |  /  |  目标文件路径。写入的文件大小超过10M时会触发轮转，最多同时存在10个文件。  |

## 样例

采集LoongCollector所有自监控指标，并将采集结果写到本地文件。

``` yaml
enable: true
inputs:
  - Type: input_internal_metrics
    Agent:
      Enable: true
      Interval: 1
    Runner:
      Enable: true
      Interval: 1
    Pipeline:
      Enable: true
      Interval: 1
    Plugin:
      Enable: true
      Interval: 10
    Component:
      Enable: true
      Interval: 10
    PluginSource:
      Enable: true
      Interval: 10
flushers:
  - Type: flusher_file
    FilePath: self_monitor/self_metrics.log
```

输出到 LoongCollector 的 `self_monitor/self_metrics.log` 文件中，每行均为一条json格式的指标。下面是其中一行展开后的参考样例：

```json
{
    "__labels__":{
        "component_name":"process_queue",
        "pipeline_name":"pipeline-demo",
        "project":"",
        "queue_type":"bounded"
    },
    "__name__":"component",
    "__time__":1735127390,
    "__value__":{
        "fetch_times_total":6000.0,
        "in_items_total":0.0,
        "in_size_bytes":0.0,
        "out_items_total":0.0,
        "queue_size":0.0,
        "queue_size_bytes":0.0,
        "total_delay_ms":0.0,
        "valid_fetch_times_total":0.0,
        "valid_to_push_status":1.0
    }
}
```
