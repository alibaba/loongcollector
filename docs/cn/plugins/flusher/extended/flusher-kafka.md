# Kafka

## 简介

`flusher_kafka` 扩展插件用于将采集到的数据发送到 Kafka。

## 版本

[Deprecated](../../stability-level.md)，插件代码已移除，请使用 [`flusher_kafka_v2`](flusher-kafka-v2.md)。

> 按 [插件版本管理](../../stability-level.md) 约定，被标注为 Deprecated 的插件将在后续版本中移除。
> `flusher_kafka` 长期仅支持 v1 接口且不支持 Metric/Span，已由功能更完整的
> `flusher_kafka_v2`（支持 v1&v2 接口）取代，故本版本起移除其实现代码。
> 存量配置请将 `Type: flusher_kafka` 迁移到 `Type: flusher_kafka_v2`，参数说明见
> [`flusher_kafka_v2` 文档](flusher-kafka-v2.md)。
