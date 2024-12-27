# 如何收集自监控指标

LoongCollector目前提供了暴露自监控指标数据的Input插件，可以通过配置包含该插件的Pipeline，实现自监控数据的收集。

## 创建采集配置Pipeline

1. 选择输入插件[自监控指标数据](../../../plugins/input/native/input-internal-metrics.md)。这里需要注意一点，就是`input_internal_metrics`插件输出的数据格式是 C++ 的多值 MetricEvent 格式（UntypedMultiDoubleValues），需要确保数据的下游支持这种格式数据的处理。
2. 选择输出插件[本地文件](../../../plugins/flusher/native/flusher-file.md)。该插件为原生输出插件，使用的`Serializer`支持 [C++ 的多值 MetricEvent 格式（UntypedMultiDoubleValues）的解析](https://github.com/alibaba/loongcollector/blob/cacbf206cf66307819992b8fe393f8c36086ac0a/core/pipeline/serializer/JsonSerializer.cpp#L84)，所以可以直接使用来输出自监控指标数据。我们输出到`self_monitor/self_metrics.log`文件，方便查看与分析。原生输出插件与`Serializer`的关系请参见[如何开发原生Flusher插件](../../plugin-development/native-plugins/how-to-write-native-flusher-plugins.md)。
3. 最终的yaml如下。我们将其保存到 LoongCollector 的运行目录下的 `conf/continuous_pipeline_config/local`目录， LoongCollector 会自动加载该配置。

    ```yaml
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

## 查看自监控指标数据

采集配置生效后，大约一分钟，可以看到自监控指标数据输出到`self_monitor/self_metrics.log`文件。文件中，每行均为一条json格式的指标。下面是其中一行 agent 级指标展开后的参考样例。`__name__`是指标类型，`__labels__`是标识该条指标对应的对象的标签，`__time__`是指标输出的时间戳，`__value__`是具体指标的值的map。

```json
{
    "__labels__":{
        "hostname":"xxx",
        "instance_id":"xxx",
        "os":"Linux",
        "os_detail":"xxx",
        "project":"",
        "start_time":"2024-12-26 06:20:25",
        "uuid":"xxx",
        "version":"0.0.1"
    },
    "__name__":"agent",
    "__source__":"xxx.xxx.xxx.xxx",
    "__time__":1735194085,
    "__value__":{
        "cpu":0.002,
        "go_memory_used_mb":0.0,
        "go_routines_total":0.0,
        "memory_used_mb":25.0,
        "open_fd_total":0.0,
        "pipeline_config_total":1.0
    }
}
```
