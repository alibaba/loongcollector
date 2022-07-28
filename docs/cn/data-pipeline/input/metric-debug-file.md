# 文本日志（debug）

## 简介
`metric_debug_file` 插件是一个用于调试的插件，它可以读取指定文件的内容，并将其与指定的名称绑定在一起成为一个字段，用于Metric模拟输入。（注：该插件由于一些原因，暂时无法直接使用）[源代码](https://github.com/alibaba/ilogtail/blob/main/plugins/input/debugfile/input_debug_file.go)

## 配置参数
| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type | String，无默认值（必填） | 插件类型，固定为`metric_debug_file`。 |
| InputFilePath | String，无默认值（必填） | 要读入的文件路径。 |
| FieldName | String，`content` | 生成的字段的字段名。 |

## 样例

* 输入
```
echo "2022/07/26/14:25:47 hello world" >> /home/test-log/debug_file.log
```

* 采集配置
```yaml
enable: true
inputs:
  - Type: metric_debug_file
    InputFilePath: /home/test-log/debug_file.log
    FieldName: HelloWorld
flushers:
  - Type: flusher_stdout
    OnlyStdout: true  
```

* 输出
```json
{
    "HelloWorld": "2022/07/26/14:25:47 hello world",
    "__time__":"1658814799"
}
```