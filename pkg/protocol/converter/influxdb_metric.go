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
	"time"

	"github.com/influxdata/line-protocol/v2/lineprotocol"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

func (c *Converter) ConvertToInfluxdbProtocolStream(groupEvents *models.PipelineGroupEvents, targetFields []string) (stream [][]byte, values []map[string]string, err error) {
	if groupEvents == nil || len(groupEvents.Events) == 0 {
		return nil, nil, nil
	}
	pooledBuf := GetPooledByteBuf()

	var encoder lineprotocol.Encoder
	encoder.SetBuffer(*pooledBuf)
	encoder.Reset()
	encoder.SetLax(true)

	for _, event := range groupEvents.Events {
		metric, ok := event.(*models.Metric)
		if !ok {
			if c.IgnoreUnExpectedData {
				logger.Warningf(context.Background(), selfmonitor.ConvertAlarm, "unsupported event type[%T] for converter with influxdb protocol", event)
				continue
			}
			return nil, nil, fmt.Errorf("unsupported event type: %v", event.GetType())
		}
		encoder.StartLine(metric.GetName())
		for _, v := range metric.GetTags().SortTo(nil) {
			encoder.AddTag(v.Key, v.Value)
		}

		v := metric.GetValue()
		if v != nil && v.IsSingleValue() {
			vv, _ := lineprotocol.FloatValue(v.GetSingleValue())
			encoder.AddField("value", vv)
		} else if v != nil && v.IsMultiValues() {
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
