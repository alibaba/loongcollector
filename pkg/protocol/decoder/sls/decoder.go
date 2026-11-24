// Copyright 2021 iLogtail Authors
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

package sls

import (
	"fmt"
	"net/http"

	"google.golang.org/protobuf/encoding/protowire"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/common"
)

type Decoder struct{}

func (d *Decoder) ParseRequest(res http.ResponseWriter, req *http.Request, maxBodySize int64) (data []byte, statusCode int, err error) {
	return common.CollectBody(res, req, maxBodySize)
}

// Decode reads the SLS protobuf group envelope directly into pipeline events.
func (d *Decoder) Decode(data []byte, _ *http.Request) ([]*models.PipelineGroupEvents, error) {
	const (
		logField         = 1
		categoryField    = 2
		topicField       = 3
		sourceField      = 4
		machineUUIDField = 5
		tagField         = 6
	)

	result := &models.PipelineGroupEvents{Group: models.NewGroup(models.NewMetadata(), models.NewTags())}
	var topic, machineUUID string
	for len(data) > 0 {
		field, kind, consumed := protowire.ConsumeTag(data)
		if consumed < 0 {
			return nil, fmt.Errorf("read SLS group field: %w", protowire.ParseError(consumed))
		}
		if !field.IsValid() {
			return nil, fmt.Errorf("invalid SLS group field number %d", field)
		}
		data = data[consumed:]
		if field < logField || field > tagField {
			consumed = protowire.ConsumeFieldValue(field, kind, data)
			if consumed < 0 {
				return nil, fmt.Errorf("skip SLS group field %d: %w", field, protowire.ParseError(consumed))
			}
			data = data[consumed:]
			continue
		}
		if kind != protowire.BytesType {
			return nil, fmt.Errorf("invalid wire type %d for SLS group field %d", kind, field)
		}
		value, consumed := protowire.ConsumeBytes(data)
		if consumed < 0 {
			return nil, fmt.Errorf("read SLS group field %d: %w", field, protowire.ParseError(consumed))
		}
		data = data[consumed:]
		switch field {
		case logField:
			log, err := decodeLog(value)
			if err != nil {
				return nil, fmt.Errorf("read SLS log: %w", err)
			}
			result.Events = append(result.Events, log)
		case tagField:
			key, value, err := decodeKeyValue(value)
			if err != nil {
				return nil, fmt.Errorf("read SLS log tag: %w", err)
			}
			result.Group.Tags.Add(key, value)
		case categoryField:
			setMetadata(result.Group.Metadata, "sls.category", value)
		case sourceField:
			setMetadata(result.Group.Metadata, "source", value)
		case topicField:
			topic = string(value)
		case machineUUIDField:
			machineUUID = string(value)
		}
	}
	if len(result.Events) == 0 {
		return nil, nil
	}
	if topic != "" {
		result.Group.Tags.Add(pipeline.TopicTagKey, topic)
	}
	if machineUUID != "" {
		result.Group.Tags.Add("__machine_uuid__", machineUUID)
	}
	return []*models.PipelineGroupEvents{result}, nil
}

// A repeated scalar field uses its last value. Empty values mean missing metadata.
func setMetadata(metadata models.Metadata, key string, value []byte) {
	if len(value) == 0 {
		metadata.Delete(key)
	} else {
		metadata.Add(key, string(value))
	}
}
