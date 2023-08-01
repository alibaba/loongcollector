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
	"context"
	"fmt"
	"strings"
	"time"

	"github.com/influxdata/line-protocol/v2/lineprotocol"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

// ConvertToInfluxdbProtocolStream converts @logGroup to []byte in the influxdb line protocol,
// @c.TagKeyRenameMap, @c.ProtocolKeyRenameMap param will be ignored, as they are not very suitable for metrics.
func (c *Converter) ConvertToInfluxdbProtocolStream(logGroup *protocol.LogGroup, targetFields []string) (stream [][]byte, values []map[string]string, err error) {
	pooledBuf := GetPooledByteBuf()

	var encoder lineprotocol.Encoder
	encoder.SetBuffer(*pooledBuf)
	encoder.Reset()
	encoder.SetLax(true)

	reader := newMetricReader()
	defer reader.recycle()

	for _, log := range logGroup.Logs {
		err := reader.set(log)
		if err != nil {
			return nil, nil, err
		}

		metricName, fieldName := reader.readNames()
		labels, err := reader.readSortedLabels()
		if err != nil {
			return nil, nil, err
		}

		v, err := reader.readValue()
		if err != nil {
			return nil, nil, err
		}
		value, ok := lineprotocol.NewValue(v)
		if !ok {
			return nil, nil, fmt.Errorf("unknown field value: %v", v)
		}
		timestamp, err := reader.readTimestamp()
		if err != nil {
			return nil, nil, err
		}

		encoder.StartLine(metricName)
		for _, v := range labels {
			encoder.AddTag(v.Key, v.Value)
		}
		encoder.AddField(fieldName, value)
		encoder.EndLine(timestamp)
		if encoder.Err() != nil {
			return nil, nil, encoder.Err()
		}
	}

	// we are batching logs in LogGroup, so only support find tags in the logGroup.LogTags
	var desiredValues map[string]string
	if len(targetFields) > 0 {
		desiredValues = findTargetValuesInLogTags(targetFields, logGroup.LogTags)
	}

	return [][]byte{encoder.Bytes()}, []map[string]string{desiredValues}, nil
}

func (c *Converter) ConvertToInfluxdbProtocolStreamV2(groupEvents *models.PipelineGroupEvents, targetFields []string) (stream [][]byte, values []map[string]string, err error) {
	pooledBuf := GetPooledByteBuf()

	var encoder lineprotocol.Encoder
	encoder.SetBuffer(*pooledBuf)
	encoder.Reset()
	encoder.SetLax(true)

	for _, event := range groupEvents.Events {
		metric, ok := event.(*models.Metric)
		if !ok {
			if c.IgnoreUnExpectedData {
				logger.Warningf(context.Background(), "CONVERT_ALARM", "unsupported event type[%T] for converter with influxdb protocol", event)
				continue
			}
			return nil, nil, fmt.Errorf("unsupported event type: %v", event.GetType())
		}
		encoder.StartLine(metric.GetName())
		for _, v := range metric.GetTags().SortTo(nil) {
			encoder.AddTag(v.Key, v.Value)
		}

		v := metric.GetValue()
		if v.IsSingleValue() {
			vv, _ := lineprotocol.FloatValue(v.GetSingleValue())
			encoder.AddField("value", vv)
		} else if v.IsMultiValues() {
			for name, value := range v.GetMultiValues().Iterator() {
				vv, _ := lineprotocol.FloatValue(value)
				encoder.AddField(name, vv)
			}
		}

		fields := metric.GetTypedValue()
		for name, value := range fields.Iterator() {
			vv, ok := lineprotocol.NewValue(value.Value)
			if !ok {
				return nil, nil, fmt.Errorf("unsupported typed value:%+v", value)
			}
			encoder.AddField(name, vv)
		}

		t := int64(metric.GetTimestamp())
		encoder.EndLine(time.Unix(t/1e9, t%1e9))
		if encoder.Err() != nil {
			return nil, nil, encoder.Err()
		}
	}

	// we are batching events in groupEvents, so only support find tags in the groupEvents.Group
	var desiredValues map[string]string
	if len(targetFields) > 0 {
		desiredValues = findTargetFieldsInGroup(targetFields, groupEvents.Group)
	}

	return [][]byte{encoder.Bytes()}, []map[string]string{desiredValues}, nil
}

func findTargetValuesInLogTags(targetFields []string, logTags []*protocol.LogTag) map[string]string {
	if len(targetFields) == 0 {
		return nil
	}

	tagMap := make(map[string]string, len(logTags))
	for _, logTag := range logTags {
		var tagName string
		if strings.HasPrefix(logTag.Key, tagPrefix) {
			tagName = targetTagPrefix + logTag.Key[len(tagPrefix):]
		} else {
			tagName = logTag.Key
		}
		tagMap[tagName] = logTag.Value
	}

	desiredValue := make(map[string]string, len(targetFields))
	for _, field := range targetFields {
		v, ok := tagMap[field]
		if !ok {
			continue
		}
		desiredValue[field] = v
	}
	return desiredValue
}
