# ServiceInput示例插件

## 简介
`service_input_example` 可作为编写`ServiceInput`类插件的参考示例样例，可以在指定端口接收模拟HTTP请求。

## 配置参数
| 参数    | 类型   | 是否必选 | 说明                                         |
| ------- | ------ | -------- | -------------------------------------------- |
| Type    | String | 是       | 插件类型，固定为`service_input_example`      |
| Address | String | 否       | <p>接收端口。</p><p>默认取值为`:19000`。</p> |


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
```
curl --header "test:val123" http://127.0.0.1:19000/data
```

* 输出
```
{
	"test":"val123",
	"__time__":"1658495321"
}
```