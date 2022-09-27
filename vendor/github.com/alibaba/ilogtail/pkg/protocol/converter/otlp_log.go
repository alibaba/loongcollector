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
	"time"

	commonv1 "go.opentelemetry.io/proto/otlp/common/v1"
	logv1 "go.opentelemetry.io/proto/otlp/logs/v1"
	resourcev1 "go.opentelemetry.io/proto/otlp/resource/v1"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

var (
	bodyKey  = "content"
	levelKey = "level"
)

func (c *Converter) ConvertToOtlpLogsV1(logGroup *protocol.LogGroup, targetFields []string) (*logv1.ResourceLogs, [][]string, error) {
	desiredValues := make([][]string, len(logGroup.Logs))
	attrs := make([]*commonv1.KeyValue, 0)
	attrs = append(attrs, c.convertToOtlpKeyValue("source", logGroup.GetSource()), c.convertToOtlpKeyValue("topic", logGroup.GetTopic()), c.convertToOtlpKeyValue("machine_uuid", logGroup.GetMachineUUID()))
	for _, t := range logGroup.LogTags {
		attrs = append(attrs, c.convertToOtlpKeyValue(t.Key, t.Value))
	}
	logRecords := make([]*logv1.LogRecord, 0)
	for i, log := range logGroup.Logs {
		contents, tags := convertLogToMap(log, logGroup.LogTags, logGroup.Source, logGroup.Topic, c.TagKeyRenameMap)
		desiredValue, err := findTargetValues(targetFields, contents, tags, c.TagKeyRenameMap)
		if err != nil {
			return nil, nil, err
		}
		desiredValues[i] = desiredValue

		logAttrs := make([]*commonv1.KeyValue, 0)
		for k, v := range contents {
			if k != bodyKey {
				logAttrs = append(logAttrs, c.convertToOtlpKeyValue(k, v))
			}
		}
		for k, v := range tags {
			logAttrs = append(logAttrs, c.convertToOtlpKeyValue(k, v))
		}
		logRecord := &logv1.LogRecord{
			TimeUnixNano:         uint64(log.Time) * uint64(time.Second),
			ObservedTimeUnixNano: uint64(log.Time) * uint64(time.Second),
			Attributes:           logAttrs,
		}
		if body, has := contents[bodyKey]; has {
			logRecord.Body = &commonv1.AnyValue{Value: &commonv1.AnyValue_StringValue{StringValue: body}}
		}
		if level, has := contents[levelKey]; has {
			logRecord.SeverityText = level
		} else if level, has = tags[level]; has {
			logRecord.SeverityText = level
		}

		logRecords = append(logRecords, logRecord)
	}
	instrumentLogs := []*logv1.ScopeLogs{{
		Scope:      &commonv1.InstrumentationScope{},
		LogRecords: logRecords,
	}}
	return &logv1.ResourceLogs{
		Resource: &resourcev1.Resource{
			Attributes: attrs,
		},
		ScopeLogs: instrumentLogs,
	}, desiredValues, nil
}

func (c *Converter) convertToOtlpKeyValue(key, value string) *commonv1.KeyValue {
	return &commonv1.KeyValue{Key: key, Value: &commonv1.AnyValue{Value: &commonv1.AnyValue_StringValue{StringValue: value}}}
}
