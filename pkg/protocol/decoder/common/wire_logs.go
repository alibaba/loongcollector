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

package common

import (
	"time"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

// WireLogsToEvents preserves the timestamp and log contents at the decoding boundary.
func WireLogsToEvents(logs []*protocol.Log) []models.PipelineEvent {
	events := make([]models.PipelineEvent, 0, len(logs))
	for _, wireLog := range logs {
		if wireLog == nil {
			continue
		}
		contents := models.NewOrderedLogContents(len(wireLog.Contents))
		for _, field := range wireLog.Contents {
			contents.Add(field.GetKey(), field.GetValue())
		}
		events = append(events, &models.Log{
			Timestamp: uint64(wireLog.GetTime())*uint64(time.Second) + uint64(wireLog.GetTimeNs()),
			Contents:  contents,
			Tags:      models.NewTags(),
		})
	}
	return events
}
