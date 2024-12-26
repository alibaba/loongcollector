# Mock数据-Service

## 简介

`service_mock` 插件是用于模拟采集Service类型输入数据的插件，可以通过调整参数获取不同的模拟输入。[源代码](https://github.com/alibaba/loongcollector/blob/main/plugins/input/mockd/input_mockd.go)

## 版本

[Stable](../../stability-level.md)

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type | String，无默认值（必填） | 插件类型，固定为`service_mock` |
| Tags | Map，其中tagKey和tagValue为String类型，`{}` | 可以按需求给mock数据添加tag。 |
| Fields | Map，其中fieldKey和fieldValue为String类型，`{}` | 可以按需求给mock数据添加字段。 |
| File | String，`""` | 指定一个文件并读取，在Fields中添加一个key为content、值为文件内容的字段。 |
| Index | Long，`0` | 生成的mock数据的开始编号（从下一个编号开始）。 |
| LogsPerSecond | Integer，`0` | 每秒生产的日志数量。 |
| MaxLogCount | Integer，`0` | 最大生产的日志总数，若为0则没有上限。 |

## 样例

* 采集配置

```yaml
enable: true
inputs:
  - Type: service_mock
    Index: 100
    Tags:
      tag1: tag1
      tag2: tag2
    Fields:
      field1: field1
      field2: field2
    LogsPerSecond: 2
    MaxLogCOunt: 3
flushers:
  - Type: flusher_stdout
    OnlyStdout: true  
```

* 输出

```json
{
    "tag1":"tag1",
    "tag2":"tag2",
    "field1":"field1",
    "field2":"field2",
    "Index":"101",
    "__time__":"1658814793"
}
{
    "tag1":"tag1",
    "tag2":"tag2",
    "field1":"field1",
    "field2":"field2",
    "Index":"102",
    "__time__":"1658814793"
}
{
    "tag1":"tag1",
    "tag2":"tag2",
    "field1":"field1",
    "field2":"field2",
    "Index":"103",
    "__time__":"1658814794"
}
```
