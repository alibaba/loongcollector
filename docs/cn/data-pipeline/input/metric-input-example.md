# MetricInput示例插件

## 简介

`metric_input_example` 可作为编写`MetricInput`类插件的参考示例样例，该插件会定时生成模拟的指标数据。[源代码](https://github.com/alibaba/ilogtail/blob/main/plugins/input/example/metric_example.go)

## 版本

[Stable](../stability-level.md)

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type | String，无默认值（必填） | 插件类型，固定为`metric_input_example`。 |

## 样例

* 采集配置

```yaml
enable: true
inputs:
  - Type: metric_input_example
flushers:
  - Type: flusher_stdout
    OnlyStdout: true  
```

* 输出

```json

{
    "__name__":"example_counter",
    "__labels__":"hostname#$#************************|ip#$#172.***.***.***",
    "__time_nano__":"1658491078378371632",
    "__value__":"101",
    "__time__":"1658491078"
}
```
