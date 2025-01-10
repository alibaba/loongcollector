// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package protocol

import (
	"fmt"
	"strings"
	"sync"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const (
	ProtocolCustomSingle        = "custom_single"
	ProtocolCustomSingleFlatten = "custom_single_flatten"
	ProtocolOtlpV1              = "otlp_v1"
	ProtocolInfluxdb            = "influxdb"
	ProtocolJsonline            = "jsonline"
	ProtocolRaw                 = "raw"
)

const (
	EncodingNone     = "none"
	EncodingJSON     = "json"
	EncodingProtobuf = "protobuf"
	EncodingCustom   = "custom"
)

const (
	tagPrefix           = "__tag__:"
	targetContentPrefix = "content."
	targetTagPrefix     = "tag."

	targetGroupMetadataPrefix = "metadata."
)

const (
	tagHostIP   = "host_ip"
	tagLogTopic = "log_topic"
)

// todo: make multiple pools for different size levels
var byteBufPool = sync.Pool{
	New: func() interface{} {
		buf := make([]byte, 0, 1024)
		return &buf
	},
}

var supportedEncodingMap = map[string]map[string]bool{
	ProtocolCustomSingle: {
		EncodingJSON:     true,
		EncodingProtobuf: false,
	},
	ProtocolCustomSingleFlatten: {
		EncodingJSON:     true,
		EncodingProtobuf: false,
	},
	ProtocolOtlpV1: {
		EncodingNone: true,
	},
	ProtocolInfluxdb: {
		EncodingCustom: true,
	},
	ProtocolJsonline: {
		EncodingJSON: true,
	},
	ProtocolRaw: {
		EncodingCustom: true,
	},
}

type Converter struct {
	Protocol             string
	Encoding             string
	Separator            string
	IgnoreUnExpectedData bool
	OnlyContents         bool
	ProtocolKeyRenameMap map[string]string
	GlobalConfig         *config.GlobalConfig
}

func NewConverterWithSep(protocol, encoding, sep string, ignoreUnExpectedData bool, protocolKeyRenameMap map[string]string, globalConfig *config.GlobalConfig) (*Converter, error) {
	converter, err := NewConverter(protocol, encoding, protocolKeyRenameMap, globalConfig)
	if err != nil {
		return nil, err
	}
	converter.Separator = sep
	converter.IgnoreUnExpectedData = ignoreUnExpectedData
	return converter, nil
}

func NewConverter(protocol, encoding string, protocolKeyRenameMap map[string]string, globalConfig *config.GlobalConfig) (*Converter, error) {
	enc, ok := supportedEncodingMap[protocol]
	if !ok {
		return nil, fmt.Errorf("unsupported protocol: %s", protocol)
	}
	if supported, existed := enc[encoding]; !existed || !supported {
		return nil, fmt.Errorf("unsupported encoding: %s for protocol %s", encoding, protocol)
	}
	return &Converter{
		Protocol:             protocol,
		Encoding:             encoding,
		ProtocolKeyRenameMap: protocolKeyRenameMap,
		GlobalConfig:         globalConfig,
	}, nil
}

func (c *Converter) Do(logGroup *protocol.LogGroup) (logs interface{}, err error) {
	logs, _, err = c.DoWithSelectedFields(logGroup, nil)
	return
}

func (c *Converter) DoWithSelectedFields(logGroup *protocol.LogGroup, targetFields []string) (logs interface{}, values []map[string]string, err error) {
	switch c.Protocol {
	case ProtocolCustomSingle:
		return c.ConvertToSingleProtocolLogs(logGroup, targetFields)
	case ProtocolCustomSingleFlatten:
		return c.ConvertToSingleProtocolLogsFlatten(logGroup, targetFields)
	case ProtocolOtlpV1:
		return c.ConvertToOtlpResourseLogs(logGroup, targetFields)
	default:
		return nil, nil, fmt.Errorf("unsupported protocol: %s", c.Protocol)
	}
}

func (c *Converter) ToByteStream(logGroup *protocol.LogGroup) (stream interface{}, err error) {
	stream, _, err = c.ToByteStreamWithSelectedFields(logGroup, nil)
	return
}

func (c *Converter) ToByteStreamWithSelectedFields(logGroup *protocol.LogGroup, targetFields []string) (stream interface{}, values []map[string]string, err error) {
	switch c.Protocol {
	case ProtocolCustomSingle:
		return c.ConvertToSingleProtocolStream(logGroup, targetFields)
	case ProtocolCustomSingleFlatten:
		return c.ConvertToSingleProtocolStreamFlatten(logGroup, targetFields)
	case ProtocolInfluxdb:
		return c.ConvertToInfluxdbProtocolStream(logGroup, targetFields)
	case ProtocolJsonline:
		return c.ConvertToJsonlineProtocolStreamFlatten(logGroup)
	default:
		return nil, nil, fmt.Errorf("unsupported protocol: %s", c.Protocol)
	}
}

func (c *Converter) ToByteStreamWithSelectedFieldsV2(groupEvents *models.PipelineGroupEvents, targetFields []string) (stream interface{}, values []map[string]string, err error) {
	switch c.Protocol {
	case ProtocolRaw:
		return c.ConvertToRawStream(groupEvents, targetFields)
	case ProtocolInfluxdb:
		return c.ConvertToInfluxdbProtocolStreamV2(groupEvents, targetFields)
	default:
		return nil, nil, fmt.Errorf("unsupported protocol: %s", c.Protocol)
	}
}

func GetPooledByteBuf() *[]byte {
	return byteBufPool.Get().(*[]byte)
}

func PutPooledByteBuf(buf *[]byte) {
	*buf = (*buf)[:0]
	byteBufPool.Put(buf)
}

func TrimPrefix(str string) string {
	switch {
	case strings.HasPrefix(str, targetContentPrefix):
		return strings.TrimPrefix(str, targetContentPrefix)
	case strings.HasPrefix(str, targetTagPrefix):
		return strings.TrimPrefix(str, targetTagPrefix)
	default:
		return str
	}
}

func convertLogToMap(log *protocol.Log, logTags []*protocol.LogTag, src, topic string) (map[string]string, map[string]string) {
	contents, tags := make(map[string]string), make(map[string]string)
	// compatible with the old version tag
	for _, logContent := range log.Contents {
		switch logContent.Key {
		case "__log_topic__":
			tags[tagLogTopic] = logContent.Value
		case tagPrefix + "__user_defined_id__":
			continue
		default:
			var tagName string
			if strings.HasPrefix(logContent.Key, tagPrefix) {
				tagName = logContent.Key[len(tagPrefix):]
				tags[tagName] = logContent.Value
			} else {
				contents[logContent.Key] = logContent.Value
			}
		}
	}

	for _, logTag := range logTags {
		if logTag.Key == "__user_defined_id__" || logTag.Key == "__pack_id__" {
			continue
		}

		tags[logTag.Key] = logTag.Value
	}

	tags[tagHostIP] = src
	if topic != "" {
		tags[tagLogTopic] = topic
	}

	return contents, tags
}

func findTargetValues(targetFields []string, contents, tags map[string]string) (map[string]string, error) {
	if len(targetFields) == 0 {
		return nil, nil
	}

	desiredValue := make(map[string]string, len(targetFields))
	for _, field := range targetFields {
		switch {
		case strings.HasPrefix(field, targetContentPrefix):
			if value, ok := contents[field[len(targetContentPrefix):]]; ok {
				desiredValue[field] = value
			}
		case strings.HasPrefix(field, targetTagPrefix):
			if value, ok := tags[field[len(targetTagPrefix):]]; ok {
				desiredValue[field] = value
			}
		default:
			return nil, fmt.Errorf("unsupported field: %s", field)
		}
	}
	return desiredValue, nil
}
