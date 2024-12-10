# 日志协议

LoongCollector 的日志数据默认以sls自定义协议的形式与外部进行交互，但也支持日志数据在sls协议和其它标准协议或自定义协议之间的转换。除此之外，对于某种协议，LoongCollector 还支持对日志数据进行不同方式的编码。

目前，LoongCollector 日志数据支持的协议及相应的编码方式如下表所示，其中协议类型可分为自定义协议和标准协议：

| 协议类型  | 协议名称                                                                                             | 支持的编码方式       |
|-------|--------------------------------------------------------------------------------------------------|---------------|
| 标准协议  | [sls协议](protocol-spec/sls.md)                                                                  | json、protobuf |
| 自定义协议 | [单条协议](protocol-spec/custom_single.md)                                                         | json          |
| 标准协议  | [Influxdb协议](https://docs.influxdata.com/influxdb/v1.8/write_protocols/line_protocol_reference/) | custom        |
| 字节流协议 | [raw协议](protocol-spec/raw.md)                                                                  | custom        |
