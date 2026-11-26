# 增加新的日志协议

如果 LoongCollector 暂时不支持您所需的日志协议，您可以为 LoongCollector 增加该协议并添加相应的协议转换函数，具体步骤如下：

1. 如果您的协议支持Protobuf或其它可通过模式定义生成相应内存结构的编码方式，您需要首先在`./pkg/protocol`目录下新建一个以协议名为命名的文件夹，然后在该文件夹中增加一个以编码方式为命名的子文件夹，在该文件中存放相应的模式定义文件，然后将由代码生成工具生成的与该模式定义文件相对应的Go代码文件放置在父目录中。目录组织结构如下：

    ```plain
    ./pkg/protocol/
    ├── <protocol_name>
    │   ├── <encoding>
    │       └──  模式定义文件
    │   └── 代码生成工具生成的Go文件
    └── converter
        ├── converter.go
        ├── <protocol>_log.go
        └── 测试文件
    ```

2. 在 `pkg/protocol/converter` 目录下新增 `<protocol>_log.go`，实现以事件组为输入的转换方法。

    ```go
    func (c *Converter) ConvertToXXXProtocolStream(group *models.PipelineGroupEvents, targetFields []string) (stream interface{}, values []map[string]string, err error)
    ```

    函数名中的 `XXX` 为协议名。`stream` 为编码结果。协议支持字段提取时，`values` 保存 `targetFields` 对应的值。支持哪些事件、字段提取和字段重命名，取决于目标协议。

    实现 Log 转换时，可复用 `convertPipelineLogToMap` 读取事件内容、事件 Tag 和组 Tag，并按 `c.TagKeyRenameMap` 重命名 Tag。`findTargetValues` 可按 `content.XXX` 和 `tag.XXX` 从转换后的内容及 Tag 中取值。可参考 [单条协议转换实现](../../../../pkg/protocol/converter/converter_events.go)。

3. 在 `pkg/protocol/converter/converter.go` 中新增协议常量，并在 `supportedEncodingMap` 中登记支持的编码方式。在 `ToByteStreamWithSelectedFields` 的 `switch` 中加入该协议，调用新增的转换方法。为新协议补充测试。

4. 更新 [协议转换](converter.md) 和 [日志协议](log-protocol.md)，在 `protocol-spec` 目录中说明新增协议的具体格式。
