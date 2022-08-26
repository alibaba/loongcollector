# MetricInput示例插件

## 简介
`metric_input_example` 可作为编写`MetricInput`类插件的参考示例样例，该插件会定时生成模拟的指标数据。

## 配置参数
该插件不需要配置参数。

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
```
{
    "__name__":"example_counter",
    "__labels__":"hostname#$#************************|ip#$#172.***.***.***",
    "__time_nano__":"1658491078378371632",
    "__value__":"101",
    "__time__":"1658491078"
}
```