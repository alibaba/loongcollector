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
	"fmt"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

// ToLogGroup converts v2 log events and group metadata into a legacy LogGroup for v1 Flush paths.
func ToLogGroup(group *models.GroupInfo, logEvents []models.PipelineEvent) (*protocol.LogGroup, error) {
	logGroup := &protocol.LogGroup{
		Logs: make([]*protocol.Log, 0, len(logEvents)),
	}
	if group != nil {
		logGroup.LogTags = make([]*protocol.LogTag, 0, group.GetTags().Len())
		for k, v := range group.GetTags().Iterator() {
			logGroup.LogTags = append(logGroup.LogTags, &protocol.LogTag{
				Key:   k,
				Value: v,
			})
		}
	}
	for _, event := range logEvents {
		log, ok := event.(*models.Log)
		if !ok {
			return nil, fmt.Errorf("expected log event, got %T", event)
		}
		pbLog, err := helper.TransferLogEventToPB(log)
		if err != nil {
			return nil, err
		}
		legacyLog, err := helper.TransferPBLogEventToLegacyLog(pbLog)
		if err != nil {
			return nil, err
		}
		logGroup.Logs = append(logGroup.Logs, legacyLog)
	}
	return logGroup, nil
}
