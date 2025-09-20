# PB 事件解析原生处理插件

## 简介

`processor_parse_from_pb_native` 将 RawEvent 中承载的 Protobuf `PipelineEventGroup` 反序列化为内部事件组结构，用于在边界/跨语言链路中传递事件。

## 版本

[Stable](../../stability-level.md)

## 配置参数

|  **参数**  |  **类型**  |  **是否必填**  |  **默认值**  |  **说明**  |
| --- | --- | --- | --- | --- |
|  Type  |  string  |  是  |  /  |  固定为 `processor_parse_from_pb_native`。 |
|  Protocol  |  string  |  是  |  /  |  PB 协议名，目前支持：`LoongSuite`。 |

## 事件类型

- 输入事件：RawEvent（内容为 PB 序列化的 `PipelineEventGroup`）
- 输出事件：对应内部 `PipelineEventGroup` 中的事件集合（Log/Metric/Span 等）
