# 数据模型

LoongCollector 目前支持 `SLS Log Protocol` 和 `Pipeline Event` 两种数据模型，两种模型的描述和对比如下：  
|  | SLS Log Protocol | Pipeline Event |
|  ----  | ----  |  ---- |
| 描述 | SLS 日志的专用处理结构 | 可扩展的可观测性数据模型，支持Metrics、Trace、Logging、Bytes、Profile等 |
| 流水线配置版本 |        v1              |          v2       |
| Logging 数据结构 |      支持           |        支持         |
| Metric 数据结构 |      不支持           |      支持           |
| Trace 数据结构 |      不支持            |       支持          |
| ByteArray 数据结构 |      不支持            |       支持          |
| Profile 数据结构 |      不支持          |       支持          |
| 自定义数据结构 |      不支持          |       支持          |

## SLS Log Protocol

参考 [SLS 协议](log-protocol/protocol-spec/sls.md)

## Pipeline Event

`PipelineEvent` 是数据处理管道中的抽象数据接口，定义如下  

```go
type PipelineEvent interface {
 GetName() string

 SetName(string)

 GetTags() Tags

 GetType() EventType

 GetTimestamp() uint64

 GetObservedTimestamp() uint64

 SetObservedTimestamp(uint64)

 Clone() PipelineEvent
}
```  

我们还需要引入聚合的概念 `PipelineGroupEvents`，来支持部分需要对数据进行分组的操作，聚合事件也允许有独立的 meta 和 tags，一般是从数据中提取

```go
type PipelineGroupEvents struct {
 Group  *GroupInfo
 Events []PipelineEvent
}

type GroupInfo struct {
 Metadata Metadata
 Tags     Tags
}
```

### Metric 模型

Metric模型定义如下：  

```go
type MetricEvent struct {
        Name              string
        MetricType        MetricType
        Timestamp         uint64
        ObservedTimestamp uint64
        Unit              string
        Description       string
        Tags              Tags
        Value             MetricValue
        TypedValue        MetricTypedValues
}
```

主流的metrics数据有单值(eg. Prometheus)和多值(eg. influxdb)两种设计，LoongCollector 中也需要支持两种不同的设计，基于此设计了 MetricValue 接口和MetricSingleValue 和 MetricMultiValue 两个不同的实现

```go
type MetricValue interface {
        IsSingleValue() bool

        IsMultiValues() bool

        GetSingleValue() float64

        GetMultiValues() MetricFloatValues
}
type MetricSingleValue struct {
        Value float64
}

type MetricMultiValue struct {
        Values MetricFloatValues
}
```

通常情况下，对于 counter 、gauge 类型的 metric 会使用单值 ，Histogram 和 Summary 类型在不同的TSDB上可能使用单值或者多值，对于单值和多值的处理，可以使用下面的API

```go
if metric.Value.IsSingleValue() {
    value := metric.Value.GetSingleValue()
} 

if metric.Value.IsMultiValues() {
  values := metric.Value.GetMultiValues()
  for field, value := range values.Iterator() {
    // field 为 string 类型,表示多值的字段名
    // value 为 float64 类型
  }
}
```

TypedValue 设计。MetricValue 不管是单值还是多值的value类型都是float64，在一些 TSDB 中比如 influxdb，除了数值类型，还支持string bool 等非数值类型 [influxdb field-value](https://docs.influxdata.com/influxdb/v1.8/concepts/glossary/#field-value) 。为此，我们扩展了一个额外的 TypedValue 字段用来存储非数值的 field 值。

```go
type TypedValue struct {
        Type  ValueType
        Value interface{}
}

type MetricTypedValues interface {
        KeyValues[*TypedValue]
}
```

### Trace 模型

目前主流的开源 tracing 模型有opentracing opentelemetry skywalking 等几种不同的规范。
不管在哪种 trace 模型中，trace 都是由多个 Span 组成的，(不同的是在 skywalking 和许多私有协议中，trace 和 span 中间还有一个 segment 或者 transaction 概念）。  
在 Span 中，一般还会定义携带数据语义的 Tags ，和记录一些关键事件发生的 logs 或 events 。  
其中zipkin jaeger 都是 opentracing 的一种实现，而opentelemetry 可以认为是在opentracing的基础上扩展的新规范和超集（兼容 w3c 和定义了明确的 OTLP 协议）。
所以我们定义的 Span 结构要兼容上面提到的几种开源协议，具体的结构如下

```go
type SpanLink struct {
    TraceID    string
    SpanID     string
    TraceState string
    Tags       Tags
}

type SpanEvent struct {
    Timestamp int64
    Name      string
}

type Span struct {
    TraceID      string
    SpanID       string
    ParentSpanID string
    Name         string
    TraceState   string

    StartTime         uint64
    EndTime           uint64
    ObservedTimestamp uint64

    Kind   SpanKind
    Status StatusCode
    Tags   Tags
    Links  []*SpanLink
    Events []*SpanEvent
}
```

### Log 模型

Log 模型可以兼容非结构化和结构化日志，并且预留链路信息记录的字段。

- 其中 Offset 记录了日志文件采集时，日志在文件中的偏移量，可选
- Name 对 Log 也是可选的
- SpanID 、TraceID 在数据关联时使用，可选
- Contents 在日志结构化的场景使用，存储从 Body 原始日志文本分析的 KV

```go
type LogContents KeyValues[string]

type Log struct {
    Name              string
    Level             string
    SpanID            string
    TraceID           string
    Tags              Tags
    Timestamp         uint64
    ObservedTimestamp uint64
    Offset            uint64
    Contents          LogContents
}
```

其中Level字段，对齐Open Telemetry Logs，支持以下等级：
`Trace`, `Trace2`, `Trace3`, `Trace4`,
`Debug`, `Debug2`, `Debug3`, `Debug4`,
`Info`, `Info2`, `Info3`, `Info4`,
`Warn`, `Warn2`, `Warn3`, `Warn4`,
`Error`, `Error2`, `Error3`, `Error4`,
`Fatal`, `Fatal2`, `Fatal3`, `Fatal4`,
`Unspecified`.
