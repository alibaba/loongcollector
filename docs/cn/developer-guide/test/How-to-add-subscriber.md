# 如何编写Subscriber插件

订阅器（Subscribe）插件是测试引擎中用于接收数据的组件，在接收到数据后，订阅器会进一步将数据发送至验证器（Validator）进行校验。如果您为 LoongCollector 开发了新的输出插件，则您必须为该输出插件编写一个对应的订阅器，用于从您输出插件对应的存储单元中拉取 LoongCollector 写入的数据，并在集成测试中使用该订阅器。

## Subscriber接口定义

- `Name()`：返回订阅器的名字；
- `Start()`：启动订阅器，不断地从目标存储单元中拉取所需要的数据，在对数据进行转换后，将数据发送至`SubscribeChan()`返回的通道；
- `Stop()`：停止订阅器；
- `SubscribeChan()`：返回用于向验证器发送接收到的数据的通道，其中通道的数据类型所对应协议的具体信息可参见[LogGroup](../../docs/cn/developer-guide/data-structure.md)；
- `FlusherConfig()`：返回与该订阅器相对应的 LoongCollector 输出插件的默认配置，可直接返回空字符串。

```go
type Subscriber interface {
    doc.Doc
    // Name returns the name of the subscriber
    Name() string
    // Start starts the subscriber
    Start() error
    // Stop stops the subscriber
    Stop()
    // SubscribeChan returns the channel used to transmit received data to validator
    SubscribeChan() <-chan *protocol.LogGroup
    // FlusherConfig returns the default flusher config for LoongCollector container correspoding to this subscriber
    FlusherConfig() string
}
```

## Subscriber开发流程

1. 在./test/engine/subscriber文件下新建一个<subscriber_name>.go文件，并在该文件中实现Subscriber接口，样例可参考默认订阅器[grpc](../engine/subscriber/grpc.go)。
2. 在该文件的`init()`函数中，使用`RegisterCreator(name string, creator Creator)`函数对插件进行注册，其中`Creator`是函数原型`func(spec map[string]interface{}) (Subscriber, error)`的简称，样例可参考默认订阅器[grpc](../engine/subscriber/grpc.go)。
3. 在引擎配置文件ilogtail-e2e.yaml的subscriber块中配置相关参数。
