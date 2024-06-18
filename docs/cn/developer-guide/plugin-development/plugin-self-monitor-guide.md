# 插件自监控接口
iLogtail提供了指标接口，可以方便地为插件增加一些自监控指标，目前支持Counter，Gauge，String，Latency等类型。

接口：

<https://github.com/alibaba/ilogtail/blob/main/pkg/pipeline/self_metric.go>

实现：

<https://github.com/alibaba/ilogtail/blob/main/pkg/helper/self_metrics_vector_imp.go>

用户使用时需要引入pkg/helper包：
```go
import (
    "github.com/alibaba/ilogtail/pkg/helper"
)
```

## 创建指标
指标必须先定义后使用，在插件的结构体内声明具体指标。
```go
type ProcessorRateLimit struct {
    // other fields
    context         pipeline.Context
    limitMetric     pipeline.CounterMetric  // 第一个指标
    processedMetric pipeline.CounterMetric   // 第二个指标
}
```

创建指标时，需要将其注册到iLogtail Context 的 MetricRecord 中，以便 iLogtail 能够采集上报数据，在插件的Init方法中，调用context 的 GetMetricRecord()方法来获取MetricRecord，然后调用helper.New**XXX**MetricAndRegister函数去注册一个指标，例如：
```go
metricsRecord := p.context.GetMetricRecord()
p.limitMetric = helper.NewCounterMetricAndRegister(metricsRecord, fmt.Sprintf("%v_limited", pluginName))
p.processedMetric = helper.NewCounterMetricAndRegister(metricsRecord, fmt.Sprintf("%v_processed", pluginName))
```
用户在声明一个Metric时可以还额外注入一些插件级别的静态Label，这是一个可选参数，例如flusher_http就把RemoteURL等配置进行上报：
```go
metricsRecord := f.context.GetMetricRecord()
metricLabels := f.buildLabels()
f.matchedEvents = helper.NewCounterMetricAndRegister(metricsRecord, "http_flusher_matched_events", metricLabels...)
```

## 指标打点
不同类型的指标有不同的打点方法，直接调用对应Metric类型的方法即可。
Counter：
```go
p.processedMetric.Add(1)
```
Latency:
```go
tracker.ProcessLatency.Observe(float64(time.Since(startProcessTime)))
```
StringMetric:
```go
sc.lastBinLogMetric.Set(string(r.NextLogName))
```

## 指标上报
iLogtial会自动采集所有注册的指标，默认采集间隔为60s，然后通过default_flusher上报，数据格式为LogGroup，格式如下：
```json
{"Logs":[{"Time":0,"Contents":[{"Key":"http_flusher_matched_events","Value":"2.0000"},{"Key":"__name__","Value":"http_flusher_matched_events"},{"Key":"RemoteURL","Value":"http://testeof.com/write"},{"Key":"db","Value":"%{metadata.db}"},{"Key":"flusher_http_id","Value":"0"},{"Key":"project","Value":"p"},{"Key":"config_name","Value":"c"},{"Key":"plugins","Value":""},{"Key":"category","Value":"p"},{"Key":"source_ip","Value":"100.80.230.110"}]},{"Time":0,"Contents":[{"Key":"http_flusher_unmatched_events","Value":"0.0000"},{"Key":"__name__","Value":"http_flusher_unmatched_events"},{"Key":"db","Value":"%{metadata.db}"},{"Key":"flusher_http_id","Value":"0"},{"Key":"RemoteURL","Value":"http://testeof.com/write"},{"Key":"project","Value":"p"},{"Key":"config_name","Value":"c"},{"Key":"plugins","Value":""},{"Key":"category","Value":"p"},{"Key":"source_ip","Value":"100.80.230.110"}]},{"Time":0,"Contents":[{"Key":"http_flusher_dropped_events","Value":"0.0000"},{"Key":"__name__","Value":"http_flusher_dropped_events"},{"Key":"RemoteURL","Value":"http://testeof.com/write"},{"Key":"db","Value":"%{metadata.db}"},{"Key":"flusher_http_id","Value":"0"},{"Key":"project","Value":"p"},{"Key":"config_name","Value":"c"},{"Key":"plugins","Value":""},{"Key":"category","Value":"p"},{"Key":"source_ip","Value":"100.80.230.110"}]},{"Time":0,"Contents":[{"Key":"http_flusher_retry_count","Value":"2.0000"},{"Key":"__name__","Value":"http_flusher_retry_count"},{"Key":"RemoteURL","Value":"http://testeof.com/write"},{"Key":"db","Value":"%{metadata.db}"},{"Key":"flusher_http_id","Value":"0"},{"Key":"project","Value":"p"},{"Key":"config_name","Value":"c"},{"Key":"plugins","Value":""},{"Key":"category","Value":"p"},{"Key":"source_ip","Value":"100.80.230.110"}]},{"Time":0,"Contents":[{"Key":"http_flusher_flush_failure_count","Value":"2.0000"},{"Key":"__name__","Value":"http_flusher_flush_failure_count"},{"Key":"db","Value":"%{metadata.db}"},{"Key":"flusher_http_id","Value":"0"},{"Key":"RemoteURL","Value":"http://testeof.com/write"},{"Key":"project","Value":"p"},{"Key":"config_name","Value":"c"},{"Key":"plugins","Value":""},{"Key":"category","Value":"p"},{"Key":"source_ip","Value":"100.80.230.110"}]},{"Time":0,"Contents":[{"Key":"http_flusher_flush_latency_ns","Value":"2504448312.5000"},{"Key":"__name__","Value":"http_flusher_flush_latency_ns"},{"Key":"db","Value":"%{metadata.db}"},{"Key":"flusher_http_id","Value":"0"},{"Key":"RemoteURL","Value":"http://testeof.com/write"},{"Key":"project","Value":"p"},{"Key":"config_name","Value":"c"},{"Key":"plugins","Value":""},{"Key":"category","Value":"p"},{"Key":"source_ip","Value":"100.80.230.110"}]}],"Category":"","Topic":"","Source":"","MachineUUID":""}
```
一组LogGroup中会有多条Log，每一条Log都对应一条指标，其中`
{"Key":"__name__","Value":"http_flusher_matched_events"}
`是一个特殊的Label，代表指标的名字。


## 高级功能
### 动态Label
和Prometheus SDK类似，iLogtail也允许用户在自监控时上报可变Label，对于这些带可变Label的指标集合，iLogtail称之为MetricVector，
MetricVector同样也支持上述的指标类型，因此把上面的Metric看作是MetricVector不带动态Label的特殊实现。
用例：
```go
type FlusherHTTP struct {
    // other fields

    context     pipeline.Context
    statusCodeStatistics pipeline.MetricVector[pipeline.CounterMetric] // 带有动态Label的指标
}
```
声明并注册MetricVector时，可以使用helper.New**XXX**MetricVectorAndRegister方法，
需要将其带有哪些动态Label的Name也进行声明：
```go
f.statusCodeStatistics = helper.NewCounterMetricVectorAndRegister(metricsRecord,
    "http_flusher_status_code_count",
    map[string]string{"RemoteURL": f.RemoteURL},
    []string{"status_code"},
)
```

打点时通过WithLabels API传入动态Label的值，拿到一个Metric对象，然后进行打点：
```go
f.statusCodeStatistics.WithLabels(pipeline.Label{Key: "status_code", Value: strconv.Itoa(response.StatusCode)}).Add(1)
```

## 示例
可以参考内置的一些插件：

限流插件：
<https://github.com/alibaba/ilogtail/blob/main/plugins/processor/ratelimit/processor_rate_limit.go>

http flusher插件：
<https://github.com/alibaba/ilogtail/blob/main/plugins/flusher/http/flusher_http.go>