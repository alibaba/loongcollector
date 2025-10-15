# Prometheus 重标记原生处理插件

## 简介

`processor_prom_relabel_metric_native` 按抓取配置执行指标重标记（relabel），合并外部标签、处理冲突（exported_ 前缀），并可生成 scrape 自动指标。

## 版本

[Stable](../../stability-level.md)

## 配置参数

- 复用 ScrapeConfig 静态配置（honor_labels、external_labels、metric_relabel_configs 等）
- 通过事件组元数据读取 scrape 相关统计（samples_scraped、duration、response_size、state 等）

## 行为说明

- 输入事件：MetricEvent；输出事件：MetricEvent
- honor_labels=true 时保留事件内标签，否则冲突时将原值重命名为 `exported_<label>`
- 自动指标输出：up、scrape_duration_seconds、scrape_body_size_bytes、scrape_samples_scraped、scrape_timeout_seconds 等
