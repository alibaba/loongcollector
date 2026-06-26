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

package exportutil

import (
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const passthroughLogKey = "__pipeline_passthrough__"

// FlushLogsFunc flushes legacy log groups produced from v2 events.
type FlushLogsFunc func(projectName, logstoreName, configName string, logGroupList []*protocol.LogGroup) error

// ExportLogOnly implements FlusherV2.Export for log-oriented flushers.
// Log events are converted to LogGroup; Metric/Span are JSON-serialized into synthetic log entries
// so they are forwarded through the existing Flush path instead of being silently dropped.
func ExportLogOnly(
	groups []*models.PipelineGroupEvents,
	projectName, logstoreName, configName string,
	flushLogs FlushLogsFunc,
) error {
	for _, groupEvents := range groups {
		if groupEvents == nil || len(groupEvents.Events) == 0 {
			continue
		}
		logEvents, passThrough := PartitionEvents(groupEvents.Events, LogOnlyEventKinds)
		if len(logEvents) == 0 && len(passThrough) == 0 {
			continue
		}
		logGroup, err := ToLogGroup(groupEvents.Group, logEvents)
		if err != nil {
			return err
		}
		if err := appendPassthroughLogs(logGroup, passThrough); err != nil {
			return err
		}
		if len(logGroup.Logs) == 0 {
			continue
		}
		if err := flushLogs(projectName, logstoreName, configName, []*protocol.LogGroup{logGroup}); err != nil {
			return err
		}
	}
	return nil
}

func appendPassthroughLogs(logGroup *protocol.LogGroup, passThrough []models.PipelineEvent) error {
	for _, event := range passThrough {
		payload, err := SerializePassthroughEvent(event)
		if err != nil {
			return err
		}
		ts := event.GetTimestamp()
		logGroup.Logs = append(logGroup.Logs, &protocol.Log{
			Time: uint32(ts / 1e9),
			Contents: []*protocol.Log_Content{{
				Key:   passthroughLogKey,
				Value: string(payload),
			}},
		})
	}
	return nil
}
