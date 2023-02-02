# raw协议

raw协议在内存中对应的数据结构定义如下：

## ByteArray & GroupInfo

传输数据的原始字节流，是`PipelineEvent`的一种实现。

```go
type ByteArray []byte
```

传输数据的标签及元数据信息，简单的key/value对。

```go
type GroupInfo struct {
	Metadata Metadata
	Tags     Tags
}
```

## PipelineGroupEvents
传输数据整体。

```go
type PipelineGroupEvents struct {
    Group  *GroupInfo
    Events []PipelineEvent
}
```
