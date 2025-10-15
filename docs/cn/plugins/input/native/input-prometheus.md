# Prometheus 抓取（原生插件）

## 简介

`input_prometheus` 插件按 `ScrapeConfig` 配置抓取 Prometheus 指标并输出。

## 版本

[Beta](../../stability-level.md)

## 配置参数

|  参数  |  类型  |  必填  |  默认值  |  说明  |
| --- | --- | --- | --- | --- |
| Type | string | 是 | / | 固定为 `input_prometheus` |
| ScrapeConfig | object | 是 | / | 抓取任务配置，必填；结构与 Prometheus 抓取配置一致的子集，至少应包含 targets、relabel、抓取间隔等必要项。 |

说明：插件会基于 `ScrapeConfig` 创建内部订阅任务并自动注册至运行器，解析与重贴标签由内置处理器完成。

## 样例

```yaml
inputs:
  - Type: input_prometheus
    ScrapeConfig:
      job_name: node
      scrape_interval: 15s
      static_configs:
        - targets: ["127.0.0.1:9100"]
      relabel_configs:
        - action: replace
          target_label: instance
          replacement: local
```



