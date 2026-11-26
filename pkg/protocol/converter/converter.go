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

	"github.com/alibaba/ilogtail/pkg/models"
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

// Separators for the __labels__ field in SLS metric records.
const (
	KeyValueSeparator = "#$#"
	LabelSeparator    = "|"
)

const (
	targetContentPrefix = "content."
	targetTagPrefix     = "tag."

	targetGroupMetadataPrefix = "metadata."
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
	TagKeyRenameMap      map[string]string
	ProtocolKeyRenameMap map[string]string
}

func NewConverterWithSep(protocol, encoding, sep string, ignoreUnExpectedData bool, tagKeyRenameMap, protocolKeyRenameMap map[string]string) (*Converter, error) {
	converter, err := NewConverter(protocol, encoding, tagKeyRenameMap, protocolKeyRenameMap)
	if err != nil {
		return nil, err
	}
	converter.Separator = sep
	converter.IgnoreUnExpectedData = ignoreUnExpectedData
	return converter, nil
}

func NewConverter(protocol, encoding string, tagKeyRenameMap, protocolKeyRenameMap map[string]string) (*Converter, error) {
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
		TagKeyRenameMap:      tagKeyRenameMap,
		ProtocolKeyRenameMap: protocolKeyRenameMap,
	}, nil
}

func (c *Converter) ToByteStreamWithSelectedFields(groupEvents *models.PipelineGroupEvents, targetFields []string) (stream interface{}, values []map[string]string, err error) {
	switch c.Protocol {
	case ProtocolCustomSingle:
		return c.ConvertToSingleProtocolStream(groupEvents, targetFields)
	case ProtocolCustomSingleFlatten:
		return c.ConvertToSingleProtocolStreamFlatten(groupEvents, targetFields)
	case ProtocolJsonline:
		return c.ConvertToJSONLineStream(groupEvents)
	case ProtocolRaw:
		return c.ConvertToRawStream(groupEvents, targetFields)
	case ProtocolInfluxdb:
		return c.ConvertToInfluxdbProtocolStream(groupEvents, targetFields)
	default:
		return nil, nil, fmt.Errorf("unsupported protocol: %s", c.Protocol)
	}
}

func (c *Converter) ToByteStream(groupEvents *models.PipelineGroupEvents) (stream interface{}, err error) {
	stream, _, err = c.ToByteStreamWithSelectedFields(groupEvents, nil)
	return
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

func findTargetValues(targetFields []string, contents, tags, tagKeyRenameMap map[string]string) (map[string]string, error) {
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
			} else if value, ok := tagKeyRenameMap[field[len(targetTagPrefix):]]; ok {
				desiredValue[field] = tags[value]
			}
		default:
			return nil, fmt.Errorf("unsupported field: %s", field)
		}
	}
	return desiredValue, nil
}

func addTagIfRequired(tags, tagKeyRenameMap map[string]string, key, value string) {
	if newKey, ok := tagKeyRenameMap[key]; ok && len(newKey) != 0 {
		tags[newKey] = value
	} else if !ok {
		tags[key] = value
	}
}
