# Golang 插件自监控接口

LoongCollector 提供了指标接口，可以方便地为插件增加一些自监控指标，目前支持Counter，Gauge，String，Latency等类型。

接口及实现：

<https://github.com/alibaba/loongcollector/blob/main/pkg/selfmonitor/metrics_vector_imp.go>

用户使用时需要引入pkg/helper包：

```go
import (
    "github.com/alibaba/ilogtail/pkg/selfmonitor"
)
```

## 创建指标

指标必须先定义后使用，在插件的结构体内声明具体指标。

```go
type ProcessorRateLimit struct {
    // other fields
    context         pipeline.Context
    limitMetric     selfmonitor.CounterMetric  // 第一个指标
    processedMetric selfmonitor.CounterMetric   // 第二个指标
}
```

创建指标时，需要将其注册到 LoongCollector Context 的 MetricRecord 中，以便 LoongCollector 能够采集上报数据，在插件的Init方法中，调用context 的 GetMetricRecord()方法来获取MetricRecord，然后调用selfmonitor.New**XXX**MetricAndRegister函数去注册一个指标，例如：

```go
metricsRecord := p.context.GetMetricRecord()
p.limitMetric = selfmonitor.NewCounterMetricAndRegister(metricsRecord, fmt.Sprintf("%v_limited", pluginType))
p.processedMetric = selfmonitor.NewCounterMetricAndRegister(metricsRecord, fmt.Sprintf("%v_processed", pluginType))
```

用户在声明一个Metric时可以还额外注入一些插件级别的静态Label，这是一个可选参数，例如flusher_http就把RemoteURL等配置进行上报：

```go
metricsRecord := f.context.GetMetricRecord()
metricLabels := f.buildLabels()
f.matchedEvents = selfmonitor.NewCounterMetricAndRegister(metricsRecord, "http_flusher_matched_events", metricLabels...)
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

LoongCollector 会自动采集所有注册的指标，默认采集间隔为60s，然后通过c++流水线上报，大致格式如下：

```json
[]{
{"counters":"{\"http_flusher_dropped_events\":\"3.0000\",\"http_flusher_flush_failure_count\":\"6.0000\",\"http_flusher_matched_events\":\"1.0000\",\"http_flusher_retry_count\":\"5.0000\",\"http_flusher_unmatched_events\":\"2.0000\"}","gauges":"{\"http_flusher_flush_latency_ns\":\"7.0000\"}","labels":"{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\"}"}
{"counters":"{\"http_flusher_status_code_count\":\"8.0000\"}","gauges":"{}","labels":"{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\",\"status_code\":\"200\"}"}
{"counters":"{\"http_flusher_status_code_count\":\"9.0000\"}","gauges":"{}","labels":"{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\",\"status_code\":\"400\"}"}
{"counters":"{\"http_flusher_error_count\":\"10.0000\"}","gauges":"{}","labels":"{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\",\"level\":\"error\",\"reason\":\"timeout\"}"}
{"counters":"{\"http_flusher_error_count\":\"11.0000\"}","gauges":"{}","labels":"{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\",\"level\":\"warn\",\"reason\":\"retry\"}"}
{"counters":"{\"http_flusher_error_count\":\"12.0000\"}","gauges":"{}","labels":"{\"PluginId\":\"13\",\"PluginType\":\"flusher_http\",\"RemoteURL\":\"http://localhost:8081\",\"level\":\"error\",\"reason\":\"dropped\"}"}
}
```

每个map[string]string都会包含同label的多条指标序列。

## 高级功能

### 动态Label

和Prometheus SDK类似，LoongCollector 也允许用户在自监控时上报可变Label，对于这些带可变Label的指标集合，LoongCollector 称之为MetricVector，
MetricVector同样也支持上述的指标类型，因此把上面的Metric看作是MetricVector不带动态Label的特殊实现。
用例：

```go
type FlusherHTTP struct {
    // other fields

    context     pipeline.Context
    statusCodeStatistics pipeline.MetricVector[selfmonitor.CounterMetric] // 带有动态Label的指标
}
```

声明并注册MetricVector时，可以使用selfmonitor.New**XXX**MetricVectorAndRegister方法，
需要将其带有哪些动态Label的Name也进行声明：

```go
f.statusCodeStatistics = selfmonitor.NewCounterMetricVectorAndRegister(metricsRecord,
    "http_flusher_status_code_count",
    map[string]string{"RemoteURL": f.RemoteURL},
    []string{"status_code"},
)
```

打点时通过WithLabels API传入动态Label的值，拿到一个Metric对象，然后进行打点：

```go
f.statusCodeStatistics.WithLabels(selfmonitor.LabelPair{Key: "status_code", Value: strconv.Itoa(response.StatusCode)}).Add(1)
```

## 示例

可以参考内置的一些插件：

限流插件：
<https://github.com/alibaba/loongcollector/blob/main/plugins/processor/ratelimit/processor_rate_limit.go>

http flusher插件：
<https://github.com/alibaba/loongcollector/blob/main/plugins/flusher/http/flusher_http.go>
