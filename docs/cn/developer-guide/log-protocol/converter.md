# 协议转换

在开发Flusher插件时，用户往往需要将日志数据从sls协议转换成其他协议。在扩展Metric数据模型后，v2版本的Flusher插件还需要支持从PipelineGroupEvents数据转换成其他协议的场景。为了加快开发插件流程，iLogtail提供了通用协议转换模块，用户只需要指定目标协议的名称和编码方式即可获得编码后的字节流。

## Converter结构

日志协议转换器的具体实现为`Converter`结构，其定义如下：

```Go
type Converter struct {
    Protocol             string
    Encoding             string
    TagKeyRenameMap      map[string]string
    ProtocolKeyRenameMap map[string]string
}
```

其中，各字段的含义如下：

- `Protocol`：协议名称
- `Encoding`：编码方式
- `TagKeyRenameMap`：tag字段Key重命名表，可用于重命名sls协议中LogTag字段的Key，其中系统保留的LogTag的Key默认值见附录。如果需要删去某个tag，则只需将该tag重命名为空字符串即可。
- `ProtocolKeyRenameMap`：协议字段Key重命名表，可用于重命名协议字段的Key，仅部分编码模式下可用，详见附录

用户可以使用如下函数创建`Converter`对象实例：

```Go
func NewConverter(protocol, encoding string, tagKeyRenameMap, protocolKeyRenameMap map[string]string) (*Converter, error)
```

其中，`protocol`字段和`encoding`的可选取值为见附录。

## 转换方法

`Converter`结构支持如下几种方法：

```Go
func (c *Converter) Do(logGroup *sls.LogGroup) (logs interface{}, err error)

func (c *Converter) ToByteStream(logGroup *sls.LogGroup) (stream interface{}, err error)

func (c *Converter) DoWithSelectedFields(logGroup *sls.LogGroup, targetFields []string) (logs interface{}, values []map[string]string, err error)

func (c *Converter) ToByteStreamWithSelectedFields(logGroup *sls.LogGroup, targetFields []string) (stream interface{}, values []map[string]string, err error)

func (c *Converter) ToByteStreamWithSelectedFieldsV2(groupEvents *models.PipelineGroupEvents, targetFields []string) (stream interface{}, values []map[string]string, err error)
```

各方法的用法如下：

- `Do`：给定日志组`logGroup`，根据`Converter`中设定的协议，将sls日志组中的各条日志分别转换为与目标协议相对应的数据结构，并将结果组装在`logs`中；
- `ToByteStream`：在完成`Do`方法的基础上，根据`Converter`中设定的编码格式，对转换后的日志进行编码形成字节流，并将结果组装在`stream`中；
- `DoWithSelectedFields`：在完成和`Do`方法一致的日志转换基础上，在各条日志及日志组tag中找到`targetFields`数组中指定字段的值，以map的形式将结果保存于`values`数组中。如果没有找到该字段，则`values`的各个map中将不包括该字段。`targetFields`中各字符串的的格式要求如下：
  - 如果需要获取某个日志字段的值，则字符串的命名方式为“content.XXX"，其中XXX为该字段的键名；
  - 如果需要获取某个日志tag的值，则字符串的命名方式为“tag.XXX"，其中XXX为该字段的键名。
- `ToByteStreamWithSelectedFields`：与`DoWithSelectedFields`方法类似，在完成和`ToByteStream`方法一致的日志转换基础上，在各条日志及日志组tag中找到`targetFields`数组中指定字段的值，以map的形式将结果保存于`values`数组中，`targetFields`中各字符串的的格式要求同上。
- `ToByteStreamWithSelectedFieldsV2`：作用与`ToByteStreamWithSelectedFields`方法一致，只是作用于v2版本的数据模版的协议转换

## 使用步骤

这里给出使用`Converter`进行日志转换的典型步骤：

1. 在当前文件中引入`protocol`包：

    ```Go
    import "github.com/alibaba/loongcollector/pkg/protocol"
    ```

2. 使用选定的转换参数创建`Converter`对象实例：

    ```Go
    c, err := protocol.NewConverter(...)
    ```

3. 给定一个日志组,

    - 如果只需要对日志格式进行转换，则调用`Converter`的`Do`方法获得转换后的数据结构：

        ```Go
        convertedLogs, err := c.Do(logGroup)
        ```

    - 如果需要对日志格式进行转换并编码，则调用`Converter`的`ToByteStream`方法获得编码后的字节流：

        ```Go
        serializedLogs, err := c.ToByteStream(logGroup)
        ```

    - 如果在对日志格式进行转换的同时，还想额外获取日志中某些字段的值，则调用`Converter`的`DoWithSelectedFields`方法同时获得转换后的数据结构与选定日志字段的值：

        ```Go
        convertedLogs, targetValues, err := c.DoWithSelectedFields(logGroup, selectedFields)
        ```

    - 如果在对日志格式进行转换并编码的同时，还想额外获取日志中某些字段的值，则调用`Converter`的`ToByteStreamWithSelectedFields`方法同时获得编码后的字节流与选定日志字段的值：

        ```Go
        serializedLogs, targetValues, err := c.ToByteStreamWithSelectedFields(logGroup, selectedFields)
        ```

## 示例

假设需要将日志从sls协议转换为单条协议，编码方式选择“json”，同时将tag中的host.name键更名为hostname，将协议中的time键更名为@timestamp，则创建`Converter`的代码如下：

```Go
c, err := protocol.NewConverter("custom_single", "json", map[string]string{"host.name":"hostname"}, map[string]string{"time", "@timestamp"})
```

## 附录

- 可选协议名

    | 协议名                   | 意义                                       |
    |------------------------------------------| ------ |
    | custom_single         | 单条协议                                     |
    | custom_single_flatten | 单条协议，数据平铺 ，如写入kafka的json消息体              |
    | influxdb              | Influxdb协议                               |
    | raw                   | 原始Byte流协议，仅支持v2版本中ByteArray类型的Event的协议转换 |

- 可选编码方式

    | 编码方式 | 意义 |
    | ------ | ------ |
    | json | json编码方式 |
    | protobuf | protobuf编码方式 |
    | custom | 自定义编码方式  |

- sls协议中系统保留LogTag的Key默认值：

    | 字段名 | 描述 |
    | ------ | ------ |
    | host.ip | iLogtail所属机器或容器的ip地址 |
    | log.topic | 日志的topic |
    | log.file.path | 被采集文件的路径 |
    | host.name | iLogtail所属机器或容器的主机名 |
    | k8s.node.ip | iLogtail容器所处K8s节点的ip |
    | k8s.node.name | iLogtail容器所处K8s节点的名称 |
    | k8s.namespace.name | 业务容器所属的K8s命名空间 |
    | k8s.pod.name | 业务容器所属的K8s Pod名称 |
    | k8s.pod.ip | 业务容器所属的K8s Pod ip |
    | k8s.pod.uid | 业务容器所属的K8s Pod uid |
    | (k8s.)container.name | 业务容器的名称。如果处于K8s环境中，则增加“k8s”前缀 |
    | (k8s.)container.ip | 业务容器的ip。如果处于K8s环境中，则增加“k8s”前缀 |
    | (k8s.)container.image.name | 用于创建业务容器的镜像名称。如果处于K8s环境中，则增加“k8s”前缀 |

- 可用于重命名协议字段Key的编码格式：

    json
