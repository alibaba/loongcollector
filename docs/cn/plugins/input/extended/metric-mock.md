# Mock数据-Metric

## 简介

`metric_mock` 插件是用于模拟采集Metric类型输入数据的插件，可以通过调整参数获取不同的模拟输入。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/input/mock/input_mock.go)


## 事件能力

列含义见 [概览 · 事件能力列说明](../../overview.md#事件能力列说明)。

| Log | Metric | Span |
| --- | --- | --- |
| — | ✓ | — |

## 版本

[Stable](../../stability-level.md)

## 版本说明

* 推荐版本：iLogtail v1.0.27 及以上

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type | string，无默认值（必填） | 插件类型，固定为`metric_mock`。 |
| Tags | Map，其中tagKey和tagValue为String类型，`{}` | 可以按需求给mock数据添加tag。 |
| Fields | Map，键为字符串，默认 `{}` | 自定义多值指标字段，支持数值、布尔值和字符串。 |
| Index | long，`0` | 生成的mock数据的开始编号（从下一个编号开始）。 |

## 样例

每次采集产生一个单值 Counter 和一个包含 `Index` 及配置字段的多值指标。下面配置从 `Index: 100` 开始递增，并将事件输出到标准输出。

```yaml
enable: true
inputs:
  - Type: metric_mock
    Index: 100
    Tags:
      service: example
    Fields:
      temperature: 20.5
      healthy: true
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```
