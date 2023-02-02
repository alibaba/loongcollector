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

	"github.com/alibaba/ilogtail/pkg/models"
)

func (c *Converter) ConvertToRawStream(groupEvents *models.PipelineGroupEvents, targetFields []string) (stream [][]byte, values []map[string]string, err error) {
	if len(groupEvents.Events) == 0 {
		return nil, nil, nil
	}

	var targetValues map[string]string
	if len(targetFields) > 0 {
		targetValues = findTargetFieldsInGroup(targetFields, groupEvents.Group)
	}

	if len(c.Separator) == 0 {
		return getByteStream(groupEvents, targetValues)
	}

	return getByteStreamWithSep(groupEvents, targetValues, c.Separator)
}

func getByteStreamWithSep(groupEvents *models.PipelineGroupEvents, targetValues map[string]string, sep string) (stream [][]byte, values []map[string]string, err error) {
	joinedStream := *GetPooledByteBuf()
	for idx, event := range groupEvents.Events {
		eventType := event.GetType()
		if eventType != models.EventTypeByteArray {
			return nil, nil, fmt.Errorf("unsupported event type %v", eventType)
		}
		if idx != 0 {
			joinedStream = append(joinedStream, sep...)
		}
		joinedStream = append(joinedStream, event.(models.ByteArray)...)
	}
	return [][]byte{joinedStream}, []map[string]string{targetValues}, nil
}

func getByteStream(groupEvents *models.PipelineGroupEvents, targetValues map[string]string) (stream [][]byte, values []map[string]string, err error) {
	byteGroup := make([][]byte, 0, len(groupEvents.Events))
	valueGroup := make([]map[string]string, 0, len(groupEvents.Events))
	for _, event := range groupEvents.Events {
		eventType := event.GetType()
		if eventType != models.EventTypeByteArray {
			return nil, nil, fmt.Errorf("unsupported event type %v", eventType)
		}

		byteStream := *GetPooledByteBuf()
		byteStream = append(byteStream, event.(models.ByteArray)...)
		byteGroup = append(byteGroup, byteStream)
		valueGroup = append(valueGroup, targetValues)
	}
	return byteGroup, valueGroup, nil
}

func findTargetFieldsInGroup(targetFields []string, group *models.GroupInfo) map[string]string {
	if len(targetFields) == 0 {
		return nil
	}

	targetKVs := make(map[string]string, len(targetFields))

	for _, field := range targetFields {
		var tagName, tagValue string
		if strings.HasPrefix(field, targetGroupMetadataPrefix) {
			tagName = field[len(targetGroupMetadataPrefix):]
			tagValue = group.GetMetadata().Get(tagName)
		} else if strings.HasPrefix(field, targetTagPrefix) {
			tagName = field[len(targetTagPrefix):]
			tagValue = group.GetTags().Get(tagName)
		}
		targetKVs[field] = tagValue
	}

	return targetKVs
}
