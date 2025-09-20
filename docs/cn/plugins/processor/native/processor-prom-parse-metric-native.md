# Prometheus 文本解析原生处理插件

## 简介

`processor_prom_parse_metric_native` 解析 Prometheus 文本格式样本为 MetricEvent，支持 honor_timestamps、默认时间戳等通过抓取配置传入。

## 版本

[Stable](../../stability-level.md)

## 配置参数

该插件复用抓取配置（ScrapeConfig），无需单独参数：

- honor_timestamps、sample_limit、scrape_timeout_seconds 等静态项
- 事件组元数据 `__prom_scrape_timestamp_milliseconds__` 用作默认时间戳

## 事件类型

- 输入事件：RawEvent
- 输出事件：MetricEvent（自动附加 `__name__` 标签）
