# 自监控指标数据

## 简介

`input_internal_metrics` 插件收集 LoongCollector 自身运行时的指标数据，并以[多值MetricEvent](../../../developer-guide/data-model-cpp.md)的格式暴露出去。

## 版本

[Beta](../../stability-level.md)

## 版本说明

* 推荐版本：LoongCollector v3.0.5 及以上

## 配置参数

关于具体指标的详情，请参见[自监控指标说明](../../../developer-guide/self-monitor/metrics/internal-metrics-description.md)。

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  插件类型。固定为input\_internal\_metrics。  |
|  Agent  |  InternalMetricRule  |  否  |  /  |  进程级指标（LoongCollector的基本信息、资源占用率等进程级别信息）的采集规则  |
|  Runner  |  InternalMetricRule  |  否  |  /  |  Runner级指标（LoongCollector内重要单例的运行状态）的采集规则  |
|  Pipeline  |  InternalMetricRule  |  否  |  /  |  Pipeline级指标（单个采集配置流水线的状态）的采集规则  |
|  PluginSource  |  InternalMetricRule  |  否  |  /  |  数据源级（例如被采集的文件的信息）的采集规则  |
|  Plugin  |  InternalMetricRule  |  否  |  /  |  插件级指标（单个插件的状态、吞吐量等信息）的采集规则  |
|  Component  |  InternalMetricRule  |  否  |  /  |  组件级指标（为了辅助Pipeline等运行的组件的状态）的采集规则  |

InternalMetricRule 的结构如下：

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Enable  |  bool  |  否  |  true  |  是否开启。默认开启。  |
|  Interval  |  int  |  否  |  10  |  统计间隔，单位为分钟，表示每隔指定时间输出一次该类型的指标。  |

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
