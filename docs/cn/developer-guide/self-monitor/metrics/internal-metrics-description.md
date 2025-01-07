# 自监控指标说明

## 指标类型

在 LoongCollector 中，有如下几种指标类型：

* 进程级（agent）：LoongCollector的整体状态，包含一些Cpu、Mem等信息。
* Runner级（runner）：LoongCollector内部的独立线程的单例，通常是一整个功能模块，例如file_server、processor_runner等。runner级指标记录的就是这些单例的状态。
* 配置级（pipeline）：每个采集配置的整体状态，例如一条采集配置的总输入、输出、延迟。
* 组件级（component）：每个采集配置运行过程中，会伴随一些组件的使用，例如Batcher、Compressor等。component级指标记录的就是这些组件的状态。
* 插件级（plugin）：每个配置中单个插件的详细指标，例如某个Processor插件的输入、输出、解析失败率。
* 数据源级（plugin_source）：每个配置的数据源的指标，例如文件采集时，每个源文件会有对应的数据，包含文件大小、读取的offset等。

## 指标格式

LoongCollector的指标为多值Metric结构。具体来说，对于某一个确定的对象（例如一条Pipeline、一个插件、一个数据源），它会存在一条指标记录，里面会以多个label来唯一标识它，并记录多个与它相关的指标值。

下面是一条指标的样例。`__name__`是指标类型，`__labels__`是标识该条指标对应的对象的标签，`__time__`是指标输出的时间戳，`__value__`是具体指标的值的map。

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

## 指标解释

LoongCollector的指标较多，这里仅列举一部分重要指标作为说明，未涉及的指标有些是过于细节的内部实现监控，有些是存在变化的可能，可以参考[源代码](https://github.com/alibaba/loongcollector/tree/main/core/monitor/metric_constants)查看用途。

### Agent级指标

Agent级的指标记录了LoongCollector的整体状态，例如Cpu、Mem等，全局唯一。

常见Labels：

| **Label名** | **含义** | **备注** |
| --- | --- | --- |
| instance_id | LoongCollector 的唯一标识 |  |
| start_time | LoongCollector 的启动时间 |  |
| hostname | LoongCollector 所在的机器名 |  |
| os | LoongCollector 所处的操作系统 |  |
| os_detail | LoongCollector 的系统详情 |  |
| version | LoongCollector 的版本 |  |

常见Metric Key：

| **Metric Key** | **含义** | **备注** |
| --- | --- | --- |
| cpu | LoongCollector 的cpu使用核数 |  |
| memory_used_mb | LoongCollector 的内存使用情况，单位为mb |  |
| go_routines_total | LoongCollector Go 部分启动的go routine数量 | k8s场景或使用扩展插件时会启动 LoongCollector Go 部分 |
| go_memory_used_mb | LoongCollector Go 部分占用的内存，单位为mb | k8s场景或使用扩展插件时会启动 LoongCollector Go 部分 |
| open_fd_total | LoongCollector 打开的文件描述符数量 |  |
| pipeline_config_total | LoongCollector 应用的采集配置数量 |  |

### Runner级指标

Runner 是 LoongCollector 内部的独立线程的单例，通常是一整个功能模块，例如file_server、processor_runner等。

常见Labels：

| **Label名** | **含义** | **备注** |
| --- | --- | --- |
| runner_name | Runner 的名称 | 常见的runner有：file_server、processor_runner、flusher_runner、http_sink等 |
| thread_no | Runner 的线程序号 |  |

常见Metric Key：

| **Metric Key** | **含义** | **备注** |
| --- | --- | --- |
| in_events_total | 当前统计周期内，进入 Runner 的 event 总数 | event 即 PipelineEvent 数据结构，基本可以认为是一条日志 |
| in_size_bytes | 当前统计周期内，进入 Runner 的数据大小，单位为字节 | 这里统计的是进入 Runner 的数据的大小，该数据可能是压缩过的，不能完全等价于 event 的数据大小 |
| last_run_time | Runner 上次执行任务的时间，格式为秒级时间戳 |  |
| total_delay_ms | Runner 执行任务的总延迟，单位为毫秒 |  |

### Pipeline级指标

Pipeline 是 LoongCollector 的[采集配置](../../../configuration/collection-config.md)，它的指标包含Pipeline的基础信息和吞吐量。

常见Labels：

| **Label名** | **含义** | **备注** |
| --- | --- | --- |
| pipeline_name | 采集配置流水线名称 |  |

常见Metric Key：

| **Metric Key** | **含义** | **备注** |
| --- | --- | --- |
| processor_in_events_total | 当前统计周期内，进入 Processor 的 event 总数 |  |
| processor_in_size_bytes | 当前统计周期内，进入 Processor 的数据大小，单位为字节 |  |
| processor_total_process_time_ms | 当前统计周期内，Processor 处理 event 总耗时，单位为毫秒 |  |
| flusher_in_events_total | 当前统计周期内，进入 Flusher 的 event 总数 |  |
| flusher_in_size_bytes | 当前统计周期内，进入 Flusher 的数据大小，单位为字节 |  |
| flusher_total_package_time_ms | 当前统计周期内，Flusher 处理 event 总耗时，单位为毫秒 |  |
| start_time | Pipeline 启动时间，格式为秒级时间戳 | Pipeline更新时，会重新启动，所以该指标可以用于判断 Pipeline 是否成功更新 |

### Component级指标

组件是用于辅助Pipeline运行的对象，它们归属于Pipeline，却对外部不可见（外部可见、可配置的是Plugin）。组件的指标根据组件类型而不同，这里只列举一些重要的。

常见Labels：

| **Label名** | **含义** | **备注** |
| --- | --- | --- |
| component_name | 组件名称 | 有：batcher，compressor，process_queue，router，sender_queue，serializer等。 |
| pipeline_name | 组件关联的采集配置流水线名称 |  |
| flusher_plugin_id | 组件关联的Flusher插件ID | 部分组件会与Pipeline中的Flusher插件关联，例如 FlusherQueue、Bacther、Compressor等，他们的关系可以参考[如何开发原生Flusher插件](../../plugin-development/native-plugins/how-to-write-native-flusher-plugins.md)。 |

常见Metric Key：

| **Metric Key** | **含义** | **备注** |
| --- | --- | --- |
| in_events_total | 当前统计周期内，进入组件的 event 总数 | event 即 PipelineEvent 数据结构，基本可以认为是一条日志 |
| out_events_total | 当前统计周期内，流出组件的 event 总数 | event 即 PipelineEvent 数据结构，基本可以认为是一条日志 |
| discarded_events_total | 当前统计周期内，被丢弃的 event 总数 | event 即 PipelineEvent 数据结构，基本可以认为是一条日志 |
| in_items_total | 当前统计周期内，进入组件的 item 总数 | item 是一些数据结构的统称，需要根据具体组件判断，不一定对应一条日志 |
| out_items_total | 当前统计周期内，流出组件的 item 总数 | item 是一些数据结构的统称，需要根据具体组件判断，不一定对应一条日志 |
| discarded_items_total | 当前统计周期内，被丢弃的 item 总数 | item 是一些数据结构的统称，需要根据具体组件判断，不一定对应一条日志 |
| in_size_bytes | 当前统计周期内，进入组件的数据大小，单位为字节 | 这里统计的是进入 Runner 的数据的大小，该数据可能是压缩或特殊处理过的，不能完全等价于 event 的数据大小 |
| out_size_bytes | 当前统计周期内，流出组件的数据大小，单位为字节 | 这里统计的是流出 Runner 的数据的大小，该数据可能是压缩或特殊处理过的，不能完全等价于 event 的数据大小 |
| discarded_size_bytes | 当前统计周期内，被丢弃的数据大小，单位为字节 | 这里统计的是 Runner 丢弃的数据的大小，该数据可能是压缩或特殊处理过的，不能完全等价于 event 的数据大小 |
| total_delay_ms | 当前统计周期内，组件聚合/发送等的延时，单位为毫秒 |  |
| total_process_time_ms | 当前统计周期内，组件处理总耗时，单位为毫秒 |  |

### Plugin级指标

一条采集配置Pipeline会包含一些[插件](../../../plugins/overview.md)，每个插件在运行过程中都会产生一些指标。

常见Labels：

| **Label名** | **含义** | **备注** |
| --- | --- | --- |
| plugin_type | 插件名 |  |
| plugin_id | 插件id | 此ID按Pipeline内插件顺序生成，暂时只用于标识插件，没有其他含义 |
| pipeline_name | 插件所属的采集配置流水线名称 |  |

常见Metric Key：

| **Metric Key** | **含义** | **备注** |
| --- | --- | --- |
| in_events_total | 当前统计周期内，进入插件的 event 总数 | event 即 PipelineEvent 数据结构，基本可以认为是一条日志 |
| out_events_total | 当前统计周期内，流出插件的 event 总数 | event 即 PipelineEvent 数据结构，基本可以认为是一条日志 |
| discarded_events_total | 当前统计周期内，被丢弃的 event 总数 | event 即 PipelineEvent 数据结构，基本可以认为是一条日志 |
| in_size_bytes | 当前统计周期内，进入插件的数据大小，单位为字节 | 这里统计的是进入 Runner 的数据的大小，该数据可能是压缩或特殊处理过的，不能完全等价于 event 的数据大小 |
| out_size_bytes | 当前统计周期内，流出插件的数据大小，单位为字节 | 这里统计的是流出 Runner 的数据的大小，该数据可能是压缩或特殊处理过的，不能完全等价于 event 的数据大小 |
| discarded_size_bytes | 当前统计周期内，被丢弃的数据大小，单位为字节 | 这里统计的是 Runner 丢弃的数据的大小，该数据可能是压缩或特殊处理过的，不能完全等价于 event 的数据大小 |
| total_delay_ms | 当前统计周期内，插件聚合/发送等的延时，单位为毫秒 |  |
| total_process_time_ms | 当前统计周期内，插件处理总耗时，单位为毫秒 |  |
| monitor_file_total | 当前统计周期内，插件监控的文件总数 | 仅限文件采集场景 |
|  |  |  |

### PluginSource级指标

这一级指标是标记数据源信息的，例如对于文件采集，被采集的文件的信息就会记录到PluginSource级指标中

常见Labels：

| **Label名** | **含义** | **备注** |
| --- | --- | --- |
| file_dev | 被采集的文件设备号 | 仅限文件采集 |
| file_inode | 被采集的文件inode号 | 仅限文件采集 |
| file_name | 被采集的文件路径 | 仅限文件采集 |

常见Metric Key：

| **Metric Key** | **含义** | **备注** |
| --- | --- | --- |
| read_offset_bytes | 当前读取的文件读到的位置 | 仅限文件采集 |
| size_bytes | 当前读取的文件的大小 | 仅限文件采集 |

## 获取自监控指标

请参见[如何收集自监控指标](how-to-collect-internal-metrics.md)。

## 添加自监控指标

请参见[如何添加自监控指标](how-to-add-internal-metrics.md)。
