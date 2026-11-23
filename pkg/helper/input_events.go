// Copyright 2026 iLogtail Authors
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

package helper

import (
	"fmt"
	"math"
	"strconv"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

// NewInputLog creates an event for a row of string fields and independent tags.
func NewInputLog(tags, fields map[string]string, timestamps ...time.Time) *models.Log {
	log := newInputLog(tags, models.NewLogContents(), timestamps)
	for key, value := range fields {
		setInputLogField(log, key, value)
	}
	return log
}

// NewInputLogRow keeps the input column order for processors and routers.
func NewInputLogRow(tags map[string]string, keys, values []string, timestamps ...time.Time) (*models.Log, error) {
	if len(keys) != len(values) {
		return nil, fmt.Errorf("mismatched log row: %d columns and %d values", len(keys), len(values))
	}
	log := newInputLog(tags, models.NewOrderedLogContents(len(keys)), timestamps)
	for i, key := range keys {
		setInputLogField(log, key, values[i])
	}
	return log, nil
}

func newInputLog(tags map[string]string, contents models.LogContents, timestamps []time.Time) *models.Log {
	var timestamp time.Time
	if len(timestamps) == 0 {
		timestamp = time.Now()
	} else {
		timestamp = timestamps[0]
	}
	target := models.NewTags()
	target.AddAll(tags)
	return &models.Log{Contents: contents, Tags: target, Timestamp: uint64(timestamp.UnixNano())}
}

func setInputLogField(log *models.Log, key, value string) {
	if key == models.BodyKey {
		log.SetBody([]byte(value))
		return
	}
	log.Contents.Add(key, value)
}

// InputEventFromProtocolLog decodes a single protobuf row at an input boundary.
// The SLS metric row convention becomes a Metric; other rows become Logs.
func InputEventFromProtocolLog(raw *protocol.Log) models.PipelineEvent {
	if raw == nil {
		return nil
	}
	// Recognize the metric row format without constructing a map or a
	// discarded Log event on the Metric path.
	var name, valueText, timeText, labelText string
	for _, field := range raw.Contents {
		if field == nil {
			continue
		}
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
		if valueText == StaleNan {
			value, err = math.Float64frombits(StaleNaN), nil
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
		event = inputLogFromProtocol(raw)
	}
	return event
}

func inputLogFromProtocol(in *protocol.Log) *models.Log {
	log := &models.Log{Tags: models.NewTags()}
	if len(in.Contents) > 0 {
		log.Contents = models.NewOrderedLogContents(len(in.Contents))
	}
	const tagPrefix = "__tag__:"
	for _, content := range in.Contents {
		if content == nil {
			continue
		}
		switch {
		case content.GetKey() == models.BodyKey:
			log.SetBody(util.ZeroCopyStringToBytes(content.Value))
		case content.GetKey() == tagPrefix+"__file_offset__":
			if offset, err := strconv.ParseInt(content.Value, 10, 64); err == nil {
				log.Offset = uint64(offset)
			}
		case strings.HasPrefix(content.Key, tagPrefix):
			log.Tags.Add(content.Key[len(tagPrefix):], content.Value)
		default:
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
