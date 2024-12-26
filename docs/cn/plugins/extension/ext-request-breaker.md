# RequestBreaker 请求拦截扩展

## 简介

`ext_request_breaker` 扩展插件，实现了 [extensions.RequestInterceptor](https://github.com/alibaba/loongcollector/blob/main/pkg/pipeline/extensions/request_interceptor.go) 接口，课题在 http_flusher 插件中引用，提供请求熔断的能力。

## 版本

[Alpha](../stability-level.md)

## 配置参数

| 参数              | 类型    | 是否必选 | 说明                                              |
|-----------------|-------|------|-------------------------------------------------|
| FailureRatio    | Float | 是    | 熔断的阈值，失败的请求在 WindowInSeconds 窗口时间内的失败率，默认值 0.10 |
| WindowInSeconds | Int   | 是    | 统计请求成功失败的窗口时间，默认值 10                            |

## 样例

使用 `metric_mock` input 插件生成数据，并将采集结果以 `custom_single` 协议、`json`格式提交到 `http://localhost:8086/write`。

且在flusher处理时，配置熔断策略

```yaml
enable: true
version: v2
inputs:
  - Type: metric_mock
    GroupMeta:
      meta1: meta1
    GroupTags:
      tag1: tag1
  - Type: metric_mock
    GroupMeta:
      meta2: meta2
    GroupTags:
      tag2: tag2
flushers:
  - Type: flusher_http
    RemoteURL: "http://localhost:8086/write"
    Convert:
      Protocol: custom_single
      Encoding: json
    RequestInterceptors: 
      - Type: ext_request_breaker
        FailureRatio: 0.1
        WindowInSeconds: 10
extensions:
  - Type: ext_groupinfo_filter
    Tags:
      tag1: tag1
```
