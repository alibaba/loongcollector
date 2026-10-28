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
	"runtime"
	"strconv"
	"time"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/pkg/util"
)

// Self-monitor alarm PB contract field keys.
// These MUST stay aligned with C++ AlarmManager::FlushAllRegionAlarm (core/monitor/AlarmManager.cpp)
// and the InnerGoAlarm struct (core/go_pipeline/LogtailPlugin.h).
const (
	AlarmFieldType     = "alarm_type"
	AlarmFieldLevel    = "alarm_level"
	AlarmFieldMessage  = "alarm_message"
	AlarmFieldCount    = "alarm_count"
	AlarmFieldIP       = "ip"
	AlarmFieldOS       = "os"
	AlarmFieldVersion  = "ver"
	AlarmFieldProject  = "project_name"
	AlarmFieldCategory = "category"
	AlarmFieldConfig   = "config"
)

// AlarmContractFields returns the ordered list of required field keys in the
// self-monitor alarm PB contract. Optional fields (project_name, category, config)
// are included only when non-empty, matching the C++ behavior.
func AlarmContractFields() []string {
	return []string{
		AlarmFieldType,
		AlarmFieldLevel,
		AlarmFieldMessage,
		AlarmFieldCount,
		AlarmFieldIP,
		AlarmFieldOS,
		AlarmFieldVersion,
	}
}

// AlarmContractOptionalFields returns field keys that are conditionally included.
func AlarmContractOptionalFields() []string {
	return []string{
		AlarmFieldProject,
		AlarmFieldCategory,
		AlarmFieldConfig,
	}
}

// TransferAlarmsToPipelineEventGroup converts a batch of AlarmExportMessage to a
// PipelineEventGroup containing LogEvents, using the existing PipelineEventGroup PB
// as the transport format. Each alarm becomes one LogEvent with key-value Contents.
//
// Field mapping aligns with C++ AlarmManager::FlushAllRegionAlarm:
//
//	AlarmType    -> "alarm_type"    (C++ AlarmMessage.mMessageType)
//	AlarmLevel   -> "alarm_level"   (C++ AlarmMessage.mLevel)
//	AlarmMessage -> "alarm_message" (C++ AlarmMessage.mMessage)
//	Count        -> "alarm_count"   (C++ AlarmMessage.mCount, as decimal string)
//	ProjectName  -> "project_name"  (C++ AlarmMessage.mProjectName, optional)
//	Category     -> "category"      (C++ AlarmMessage.mCategory, optional)
//	Config       -> "config"        (C++ AlarmMessage.mConfig, optional)
func TransferAlarmsToPipelineEventGroup(alarms []selfmonitor.AlarmExportMessage, t time.Time) (*protocol.PipelineEventGroup, error) {
	logEvents := make([]*protocol.LogEvent, 0, len(alarms))
	for i := range alarms {
		logEvent, err := transferAlarmToLogEvent(&alarms[i], t)
		if err != nil {
			return nil, err
		}
		logEvents = append(logEvents, logEvent)
	}
	group := &protocol.PipelineEventGroup{
		PipelineEvents: &protocol.PipelineEventGroup_Logs{
			Logs: &protocol.PipelineEventGroup_LogEvents{Events: logEvents},
		},
	}
	return group, nil
}

// TransferPipelineEventGroupToAlarms converts a PipelineEventGroup (containing LogEvents)
// back to a slice of AlarmExportMessage. This is the inverse of TransferAlarmsToPipelineEventGroup.
func TransferPipelineEventGroupToAlarms(group *protocol.PipelineEventGroup) []selfmonitor.AlarmExportMessage {
	logsWrapper := group.GetLogs()
	if logsWrapper == nil {
		return nil
	}
	alarms := make([]selfmonitor.AlarmExportMessage, 0, len(logsWrapper.Events))
	for _, logEvent := range logsWrapper.Events {
		alarms = append(alarms, transferLogEventToAlarm(logEvent))
	}
	return alarms
}

func transferAlarmToLogEvent(msg *selfmonitor.AlarmExportMessage, t time.Time) (*protocol.LogEvent, error) {
	fields := make(map[string]string, 10)
	fields[AlarmFieldType] = msg.AlarmType
	fields[AlarmFieldLevel] = msg.AlarmLevel
	fields[AlarmFieldMessage] = msg.AlarmMessage
	fields[AlarmFieldCount] = strconv.Itoa(msg.Count)
	fields[AlarmFieldIP] = util.GetIPAddress()
	fields[AlarmFieldOS] = runtime.GOOS
	fields[AlarmFieldVersion] = config.BaseVersion
	if msg.ProjectName != "" {
		fields[AlarmFieldProject] = msg.ProjectName
	}
	if msg.Category != "" {
		fields[AlarmFieldCategory] = msg.Category
	}
	if msg.Config != "" {
		fields[AlarmFieldConfig] = msg.Config
	}
	return CreateLogEvent(t, false, fields)
}

func transferLogEventToAlarm(logEvent *protocol.LogEvent) selfmonitor.AlarmExportMessage {
	msg := selfmonitor.AlarmExportMessage{}
	for _, content := range logEvent.Contents {
		key := string(content.Key)
		val := string(content.Value)
		switch key {
		case AlarmFieldType:
			msg.AlarmType = val
		case AlarmFieldLevel:
			msg.AlarmLevel = val
		case AlarmFieldMessage:
			msg.AlarmMessage = val
		case AlarmFieldCount:
			msg.Count, _ = strconv.Atoi(val)
		case AlarmFieldProject:
			msg.ProjectName = val
		case AlarmFieldCategory:
			msg.Category = val
		case AlarmFieldConfig:
			msg.Config = val
		}
	}
	return msg
}
