# ServiceInput示例插件

## 简介

`service_input_example` 可作为编写`ServiceInput`类插件的参考示例样例，可以在指定端口接收模拟HTTP请求。[源代码](https://github.com/alibaba/ilogtail/blob/main/plugins/input/example/service_example.go)

## 版本

[Stable](../stability-level.md)

## 配置参数

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type | String，无默认值（必填） | 插件类型，固定为`service_input_example`。 |
| Address | String，`：19000` | 接收端口。 |

## 样例

* 采集配置

```yaml
enable: true
inputs:
  - Type: service_input_example
flushers:
  - Type: flusher_stdout
    OnlyStdout: true  
```

* 输入

```bash
curl --header "test:val123" http://127.0.0.1:19000/data
```

* 输出

```json
{
    "test":"val123",
    "__time__":"1658495321"
}
```
