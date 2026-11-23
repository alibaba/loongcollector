# 协议转换

Flusher 可用 `Converter` 将 `models.PipelineGroupEvents` 转换成目标协议。创建转换器时传入协议、编码方式，以及可选的 Tag 和协议字段重命名表。

## 创建 Converter

```go
func NewConverter(protocol, encoding string, tagKeyRenameMap, protocolKeyRenameMap map[string]string) (*Converter, error)

func NewConverterWithSep(protocol, encoding, sep string, ignoreUnExpectedData bool, tagKeyRenameMap, protocolKeyRenameMap map[string]string) (*Converter, error)
```

- `tagKeyRenameMap` 使用输入事件或事件组中的 Tag 键。将新键名设置为空字符串可删除对应 Tag。
- `protocolKeyRenameMap` 用于重命名 JSON 协议的字段，例如将 `custom_single` 的 `time` 改为 `@timestamp`。
- `NewConverterWithSep` 还可设置分隔符和是否跳过不支持的事件。分隔符用于 `raw` 协议。

## 转换方法

```go
func (c *Converter) ToByteStream(groupEvents *models.PipelineGroupEvents) (stream interface{}, err error)

func (c *Converter) ToByteStreamWithSelectedFields(groupEvents *models.PipelineGroupEvents, targetFields []string) (stream interface{}, values []map[string]string, err error)

func (c *Converter) ToOTLP(group *models.PipelineGroupEvents) (plog.ResourceLogs, pmetric.ResourceMetrics, ptrace.ResourceSpans, error)
```

- `ToByteStream` 按选定协议转换并编码事件组。根据协议，`stream` 的类型为 `[][]byte` 或 `[]byte`。
- `ToByteStreamWithSelectedFields` 在编码时提取指定字段，支持的字段范围取决于协议。`custom_single` 和 `custom_single_flatten` 使用 `content.XXX` 选择内容字段，`tag.XXX` 选择 Tag。对应键不存在时，结果 map 省略该键。
- `ToOTLP` 将事件转换为 OTLP 的日志、指标和链路资源。创建转换器时使用 `otlp_v1` 和 `none`，由调用方将资源装入相应的 OTLP 请求。

## 示例

将事件组转换成 `custom_single` JSON。若输入含有 `service.name` Tag，将它改名为 `service`，同时将协议字段 `time` 改名为 `@timestamp`。

```go
package example

import (
	"github.com/alibaba/ilogtail/pkg/models"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
)

func encodeGroup(group *models.PipelineGroupEvents) (interface{}, []map[string]string, error) {
	c, err := converter.NewConverter(
		converter.ProtocolCustomSingle,
		converter.EncodingJSON,
		map[string]string{"service.name": "service"},
		map[string]string{"time": "@timestamp"},
	)
	if err != nil {
		return nil, nil, err
	}
	return c.ToByteStreamWithSelectedFields(group, []string{"tag.service"})
}
```

## 附录

| 协议名 | 编码方式 | 说明 |
| --- | --- | --- |
| `custom_single` | `json` | 单条协议，每条记录编码为一个 JSON 对象。 |
| `custom_single_flatten` | `json` | 平铺的单条协议，内容字段和 Tag 放在同一 JSON 对象中。 |
| `jsonline` | `json` | 将平铺 JSON 记录用换行符连接。 |
| `influxdb` | `custom` | 将 Metric 事件转换成 InfluxDB 行协议。 |
| `raw` | `custom` | 输出 ByteArray 事件的原始字节流。 |
| `otlp_v1` | `none` | 通过 `ToOTLP` 转换为 OTLP 资源。 |
