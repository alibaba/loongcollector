// Copyright 2024 iLogtail Authors
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

package pluginmanager

import (
	"math"
	"strconv"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

// inputCollector adapts the public Collector API at the input boundary. It emits
// native events directly; no LogGroup or second runner is constructed.
type inputCollector struct{ context pipeline.PipelineContext }

func (c *inputCollector) group(ctx map[string]interface{}) *models.GroupInfo {
	group := models.NewGroup(models.NewMetadata(), models.NewTags())
	for key, value := range ctx {
		switch value := value.(type) {
		case string:
			group.Metadata.Add(key, value)
		case []*protocol.LogTag:
			for _, tag := range value {
				group.Tags.Add(tag.Key, tag.Value)
			}
		}
	}
	return group
}
func (c *inputCollector) AddData(tags, fields map[string]string, t ...time.Time) {
	c.AddDataWithContext(tags, fields, nil, t...)
}
func (c *inputCollector) AddDataArray(tags map[string]string, keys, values []string, t ...time.Time) {
	c.AddDataArrayWithContext(tags, keys, values, nil, t...)
}
func (c *inputCollector) AddRawLog(log *protocol.Log) { c.AddRawLogWithContext(log, nil) }
func (c *inputCollector) AddDataWithContext(tags, fields map[string]string, ctx map[string]interface{}, t ...time.Time) {
	ts := time.Now()
	if len(t) > 0 {
		ts = t[0]
	}
	log := &models.Log{Timestamp: uint64(ts.UnixNano()), Tags: models.NewTags(), Contents: models.NewLogContents()}
	for key, value := range tags {
		log.Tags.Add(key, value)
	}
	for key, value := range fields {
		if key == contentKey {
			log.SetBody([]byte(value))
		} else {
			log.Contents.Add(key, value)
		}
	}
	c.context.Collector().Collect(c.group(ctx), log)
}
func (c *inputCollector) AddDataArrayWithContext(tags map[string]string, keys, values []string, ctx map[string]interface{}, t ...time.Time) {
	if len(keys) != len(values) {
		return
	}
	fields := make(map[string]string, len(keys))
	for i, key := range keys {
		fields[key] = values[i]
	}
	c.AddDataWithContext(tags, fields, ctx, t...)
}
func (c *inputCollector) AddRawLogWithContext(raw *protocol.Log, ctx map[string]interface{}) {
	if raw == nil {
		return
	}
	// Recognize the public metric-log format without constructing a map or a
	// discarded Log event on the Metric path.
	var name, valueText, timeText, labelText string
	for _, field := range raw.Contents {
		switch field.Key {
		case "__name__":
			name = field.Value
		case "__value__":
			valueText = field.Value
		case "__time_nano__":
			timeText = field.Value
		case "__labels__":
			labelText = field.Value
		}
	}
	var event models.PipelineEvent
	if name != "" && valueText != "" && timeText != "" {
		value, err := strconv.ParseFloat(valueText, 64)
		if valueText == helper.StaleNan {
			value, err = math.Float64frombits(helper.StaleNaN), nil
		}
		timestamp, timeErr := strconv.ParseInt(timeText, 10, 64)
		if err == nil && timeErr == nil {
			labels := models.NewTags()
			for _, label := range strings.Split(labelText, "|") {
				if key, value, ok := strings.Cut(label, "#$#"); ok {
					labels.Add(key, value)
				}
			}
			event = models.NewSingleValueMetric(name, models.MetricTypeUntyped, labels, timestamp, value)
		}
	}
	if event == nil {
		event = convertInputLog(raw)
	}

	c.context.Collector().Collect(c.group(ctx), event)
}

// convertInputLog is the public raw-log API boundary, never a runner queue type.
func convertInputLog(in *protocol.Log) models.PipelineEvent {
	log := &models.Log{}
	log.Tags = models.NewTags()
	for _, content := range in.Contents {
		switch {
		case content.Key == contentKey:
			log.SetBody(util.ZeroCopyStringToBytes(content.Value))
		case content.Key == fileOffsetKey:
			if offset, err := strconv.ParseInt(content.Value, 10, 64); err == nil {
				log.Offset = uint64(offset)
			}
		case strings.HasPrefix(content.Key, tagPrefix):
			log.Tags.Add(content.Key[len(tagPrefix):], content.Value)
		default:
			if log.Contents == nil {
				log.Contents = models.NewLogContents()
			}
			log.Contents.Add(content.Key, content.Value)
		}
	}
	if in.Time != 0 {
		log.Timestamp = uint64(time.Second * time.Duration(in.Time))
		if in.TimeNs != nil {
			log.Timestamp += uint64(*in.TimeNs)
		}
	} else {
		log.Timestamp = uint64(time.Now().UnixNano())
	}
	return log
}
