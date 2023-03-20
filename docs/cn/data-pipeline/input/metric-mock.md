# Mock数据-Metric

## 简介

`metric_mock` 插件是用于模拟采集Metric类型输入数据的插件，可以通过调整参数获取不同的模拟输入。[源代码](https://github.com/alibaba/ilogtail/blob/main/plugins/input/mock/input_mock.go)

## 版本

[Stable](../stability-level.md)

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type | String，无默认值（必填） | 插件类型，固定为`metric_mock`。 |
| Tags | Map，其中tagKey和tagValue为String类型，`{}` | 可以按需求给mock数据添加tag。 |
| Fields | Map，其中fieldKey和fieldValue为String类型，`{}` | 可以按需求给mock数据添加字段。 |
| Index | Long，`0` | 生成的mock数据的开始编号（从下一个编号开始）。 |
| OpenPrometheusPattern | Boolean，`false` | 是否生成Prometheus样式的mock数据。 |

## 样例

* 采集配置1

```yaml
enable: true
inputs:
  - Type: metric_mock
    Fields:
      field1: field1
      field2: field2
flushers:
  - Type: flusher_stdout
    OnlyStdout: true  
```

* 输出1

```json
{
    "field2":"field2",
    "Index":"1",
    "field1":"field1",
    "__time__":"1658807050"
}
{
    "field1":"field1",
    "field2":"field2",
    "Index":"2",
    "__time__":"1658807051"
}
```

* 采集配置2

```yaml
enable: true
inputs:
  - Type: metric_mock
    Index: 100
    Tags:
      tag1: tag1
      tag2: tag2
    Fields:
      field1: field1
      field2: field2
    OpenPrometheusPattern: true
flushers:
  - Type: flusher_stdout
    OnlyStdout: true  
```

* 输出2

```json
{
    "__name__":"metrics_mock",
    "__labels__":"field1#$#field1|field2#$#field2|tag1#$#tag1|tag2#$#tag2",
    "__time_nano__":"1658806869597190887",
    "__value__":"101",
    "__time__":"1658806869"
}
{
    "__name__":"metrics_mock",
    "__labels__":"field1#$#field1|field2#$#field2|tag1#$#tag1|tag2#$#tag2",
    "__time_nano__":"1658806870597391426",
    "__value__":"102",
    "__time__":"1658806870"
}
```
