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

2. 在`./pkg/protocol/converter`目录下新建一个以`<protocol>_log.go`命名的文件，并在该文件中实现下列函数：

    ```Go
    func (c *Converter) ConvertToXXXProtocolLogs(logGroup *sls.LogGroup, targetFields []string) (logs interface{}, values [][]string, err error)

    func (c *Converter) ConvertToXXXProtocolStream(logGroup *sls.LogGroup, targetFields []string) (stream interface{}, values [][]string, err error)
    ```

    其中，函数名中的“XXX”为协议名（可缩写），参数中的`logGroup`为sls日志组，`targetFields`为需要提取值的字段名，返回值中的`logs`为与协议对应的数据结构组成的数组，`stream`为代表该日志组的字节流，`values`为每条日志或日志组tag中`targetFields`字段对应的值，`err`为返回的错误。

    该函数必须支持以下功能：
    1. 对输入日志进行相应的转换
    2. 能够根据`c.TagKeyRenameMap`重命名sls协议中LogTag字段的Key
    3. 能够根据`targetFields`找到对应字段的值
    4. 对于部分编码格式，能够根据`c.ProtocolKeyRenameMap`重命名协议字段的Key

    为了完成上述第2和第3点，LoongCollector 提供了下列帮助函数：

    ```Go
    func convertLogToMap(log *sls.Log, logTags []*sls.LogTag, src, topic string, tagKeyRenameMap map[string]string) (contents map[string]string, tags map[string]string)

    func findTargetValues(targetFields []string, contents, tags, tagKeyRenameMap map[string]string) (values map[string]string, err error)
    ```

    各函数的用途如下：
    - `convertLogToMap`：将符合sls协议的日志以及对应logGroup中的其他元信息转换成以map形式存储的`contents`和`tags`，同时根据`tagKeyRenameMap`重命名sls协议中LogTag字段的Key。
    - `findTargetValues`：在`convertLogToMap`获得的`contents`和`tags`中寻找`targetFields`字段对应的值，如果`targetFields`中包含被更名的默认tag键，则寻找过程还需要借助`tagKeyRenameMap`。

3. 在`./pkg/protocol/converter/converter.go`中，依次做出如下改变：

    - 新增`protocolXXX`常量，“XXX”为协议名，常量的值也为协议名
    - 如果协议支持新的编码方式，则新增`encodingXXX`常量，“XXX”为编码方式，常量的值也为编码方式
    - 在`supportedEncodingMap`中，增加该协议支持的编码类型
    - 在`c.DoWithSelectedFields`方法的`switch`语句中新增一个`case`子句，`case`名为协议名，子句内容为`return c.ConvertToXXXProtocolLogs(logGroup, targetFields)`，其中涉及的函数即为第2步中编写的函数
    - 在`c.ToByteStreamWithSelectedFields`方法的`switch`语句中新增一个`case`子句，`case`名为协议名，子句内容为`return c.ConvertToXXXProtocolStream(logGroup, targetFields)`，其中涉及的函数即为第2步中编写的函数

4. 在`./doc/cn/developer-guide/log-protocol/converter.md`的附录、`README.md`中增加协议相关内容，并在`./doc/cn/developer-guide/log-protocol/protocol-spec`文件夹下新增`<protocol>.md`文件描述具体的协议形式。
