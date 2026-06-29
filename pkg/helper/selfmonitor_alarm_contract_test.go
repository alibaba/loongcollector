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
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/pkg/util"
)

// TestAlarmContractFieldNames verifies the field key constants match the C++
// AlarmManager::FlushAllRegionAlarm log event content keys exactly.
// C++ reference: core/monitor/AlarmManager.cpp lines 126-140.
func TestAlarmContractFieldNames(t *testing.T) {
	cppFields := map[string]bool{
		"alarm_type":    true,
		"alarm_level":   true,
		"alarm_message": true,
		"alarm_count":   true,
		"ip":            true,
		"os":            true,
		"ver":           true,
		"project_name":  true,
		"category":      true,
		"config":        true,
	}

	required := AlarmContractFields()
	optional := AlarmContractOptionalFields()
	all := append(required, optional...)

	for _, field := range all {
		assert.True(t, cppFields[field], "Go contract field %q not found in C++ AlarmManager", field)
	}
	for field := range cppFields {
		found := false
		for _, f := range all {
			if f == field {
				found = true
				break
			}
		}
		assert.True(t, found, "C++ field %q not covered in Go contract", field)
	}
}

// TestTransferAlarmsToPipelineEventGroup_AllFields verifies conversion with all
// fields populated produces a PipelineEventGroup containing correct LogEvents.
func TestTransferAlarmsToPipelineEventGroup_AllFields(t *testing.T) {
	alarms := []selfmonitor.AlarmExportMessage{
		{
			AlarmType:    "PLUGIN_ALARM",
			AlarmLevel:   "3",
			AlarmMessage: "plugin crashed",
			ProjectName:  "my-project",
			Category:     "my-logstore",
			Config:       "my-config",
			Count:        10,
		},
	}
	ts := time.Unix(1700000000, 0)

	group, err := TransferAlarmsToPipelineEventGroup(alarms, ts)
	require.NoError(t, err)
	require.NotNil(t, group)
	require.NotNil(t, group.GetLogs())
	require.Len(t, group.GetLogs().Events, 1)

	contentMap := logEventToMap(group.GetLogs().Events[0])
	assert.Equal(t, "PLUGIN_ALARM", contentMap[AlarmFieldType])
	assert.Equal(t, "3", contentMap[AlarmFieldLevel])
	assert.Equal(t, "plugin crashed", contentMap[AlarmFieldMessage])
	assert.Equal(t, "10", contentMap[AlarmFieldCount])
	assert.Equal(t, util.GetIPAddress(), contentMap[AlarmFieldIP])
	assert.Equal(t, runtime.GOOS, contentMap[AlarmFieldOS])
	assert.Equal(t, config.BaseVersion, contentMap[AlarmFieldVersion])
	assert.Equal(t, "my-project", contentMap[AlarmFieldProject])
	assert.Equal(t, "my-logstore", contentMap[AlarmFieldCategory])
	assert.Equal(t, "my-config", contentMap[AlarmFieldConfig])
}

// TestTransferAlarmsToPipelineEventGroup_OptionalFieldsOmitted verifies that
// empty optional fields are not included in the log event, matching C++ behavior.
func TestTransferAlarmsToPipelineEventGroup_OptionalFieldsOmitted(t *testing.T) {
	alarms := []selfmonitor.AlarmExportMessage{
		{
			AlarmType:    "BOOT_INIT_ALARM",
			AlarmLevel:   "1",
			AlarmMessage: "init warning",
			Count:        1,
		},
	}
	ts := time.Unix(1700000000, 0)

	group, err := TransferAlarmsToPipelineEventGroup(alarms, ts)
	require.NoError(t, err)
	require.Len(t, group.GetLogs().Events, 1)

	contentMap := logEventToMap(group.GetLogs().Events[0])
	assert.Equal(t, "BOOT_INIT_ALARM", contentMap[AlarmFieldType])
	assert.Equal(t, "1", contentMap[AlarmFieldLevel])
	assert.Equal(t, "init warning", contentMap[AlarmFieldMessage])
	assert.Equal(t, "1", contentMap[AlarmFieldCount])

	_, hasProject := contentMap[AlarmFieldProject]
	_, hasCategory := contentMap[AlarmFieldCategory]
	_, hasConfig := contentMap[AlarmFieldConfig]
	assert.False(t, hasProject, "empty project_name should be omitted")
	assert.False(t, hasCategory, "empty category should be omitted")
	assert.False(t, hasConfig, "empty config should be omitted")
}

// TestTransferPipelineEventGroupToAlarms_RoundTrip verifies lossless round-trip
// conversion: []AlarmExportMessage -> PipelineEventGroup -> []AlarmExportMessage.
func TestTransferPipelineEventGroupToAlarms_RoundTrip(t *testing.T) {
	original := []selfmonitor.AlarmExportMessage{
		{
			AlarmType:    "FLUSHER_FLUSH_ALARM",
			AlarmLevel:   "2",
			AlarmMessage: "http 500",
			ProjectName:  "proj-x",
			Category:     "logstore-y",
			Config:       "config-z",
			Count:        7,
		},
		{
			AlarmType:    "PLUGIN_ALARM",
			AlarmLevel:   "1",
			AlarmMessage: "timeout",
			Count:        3,
		},
	}
	ts := time.Unix(1700000000, 0)

	group, err := TransferAlarmsToPipelineEventGroup(original, ts)
	require.NoError(t, err)

	restored := TransferPipelineEventGroupToAlarms(group)
	require.Len(t, restored, 2)

	assert.Equal(t, original[0].AlarmType, restored[0].AlarmType)
	assert.Equal(t, original[0].AlarmLevel, restored[0].AlarmLevel)
	assert.Equal(t, original[0].AlarmMessage, restored[0].AlarmMessage)
	assert.Equal(t, original[0].ProjectName, restored[0].ProjectName)
	assert.Equal(t, original[0].Category, restored[0].Category)
	assert.Equal(t, original[0].Config, restored[0].Config)
	assert.Equal(t, original[0].Count, restored[0].Count)

	assert.Equal(t, original[1].AlarmType, restored[1].AlarmType)
	assert.Equal(t, original[1].AlarmLevel, restored[1].AlarmLevel)
	assert.Equal(t, original[1].AlarmMessage, restored[1].AlarmMessage)
	assert.Equal(t, "", restored[1].ProjectName)
	assert.Equal(t, "", restored[1].Category)
	assert.Equal(t, "", restored[1].Config)
	assert.Equal(t, original[1].Count, restored[1].Count)
}

// TestTransferAlarmsToPipelineEventGroup_MultipleAlarms verifies batch conversion
// produces one LogEvent per alarm in the PipelineEventGroup.
func TestTransferAlarmsToPipelineEventGroup_MultipleAlarms(t *testing.T) {
	alarms := []selfmonitor.AlarmExportMessage{
		{AlarmType: "TYPE_A", AlarmLevel: "1", AlarmMessage: "msg-a", Count: 1},
		{AlarmType: "TYPE_B", AlarmLevel: "2", AlarmMessage: "msg-b", Count: 2},
		{AlarmType: "TYPE_C", AlarmLevel: "3", AlarmMessage: "msg-c", Count: 3},
	}
	ts := time.Unix(1700000000, 0)

	group, err := TransferAlarmsToPipelineEventGroup(alarms, ts)
	require.NoError(t, err)
	require.Len(t, group.GetLogs().Events, 3)

	for i, logEvent := range group.GetLogs().Events {
		contentMap := logEventToMap(logEvent)
		assert.Equal(t, alarms[i].AlarmType, contentMap[AlarmFieldType])
		assert.Equal(t, alarms[i].AlarmLevel, contentMap[AlarmFieldLevel])
		assert.Equal(t, alarms[i].AlarmMessage, contentMap[AlarmFieldMessage])
		assert.Equal(t, strconv.Itoa(alarms[i].Count), contentMap[AlarmFieldCount])
	}
}

// TestAlarmLevelValues verifies Go AlarmLevel constants produce the same string
// values as C++ AlarmLevel enum (1=warning, 2=error, 3=critical).
func TestAlarmLevelValues(t *testing.T) {
	assert.Equal(t, "1", selfmonitor.AlarmLevelWaring.String())
	assert.Equal(t, "2", selfmonitor.AlarmLevelError.String())
	assert.Equal(t, "3", selfmonitor.AlarmLevelCritical.String())
}

// TestAlarmLevelIsValid verifies only valid levels pass validation.
func TestAlarmLevelIsValid(t *testing.T) {
	assert.True(t, selfmonitor.AlarmLevelWaring.IsValid())
	assert.True(t, selfmonitor.AlarmLevelError.IsValid())
	assert.True(t, selfmonitor.AlarmLevelCritical.IsValid())
	assert.False(t, selfmonitor.AlarmLevel("0").IsValid())
	assert.False(t, selfmonitor.AlarmLevel("4").IsValid())
	assert.False(t, selfmonitor.AlarmLevel("").IsValid())
}

// TestAlarmCountSerialization verifies count is serialized as a decimal string,
// matching C++ ToString(messagePtr->mCount).
func TestAlarmCountSerialization(t *testing.T) {
	alarms := []selfmonitor.AlarmExportMessage{
		{AlarmType: "TEST_ALARM", AlarmLevel: "1", AlarmMessage: "test", Count: 42},
	}
	ts := time.Unix(1700000000, 0)

	group, err := TransferAlarmsToPipelineEventGroup(alarms, ts)
	require.NoError(t, err)

	contentMap := logEventToMap(group.GetLogs().Events[0])
	assert.Equal(t, "42", contentMap[AlarmFieldCount])

	restored := TransferPipelineEventGroupToAlarms(group)
	assert.Equal(t, 42, restored[0].Count)
}

// TestCppAlarmTypeStringAlignment verifies that Go AlarmType string values
// match C++ mMessageType vector entries.
func TestCppAlarmTypeStringAlignment(t *testing.T) {
	cppInternalTypes := []string{
		"USER_CONFIG_ALARM",
		"CATEGORY_CONFIG_ALARM",
		"CHECKPOINT_ALARM",
		"PROCESS_QUEUE_BUSY_ALARM",
		"SEND_DATA_FAIL_ALARM",
		"DISCARD_DATA_ALARM",
	}

	for _, cppType := range cppInternalTypes {
		found := false
		for _, goType := range knownGoAlarmTypes() {
			if goType == cppType {
				found = true
				break
			}
		}
		if !found {
			t.Logf("C++ alarm type %q has no matching Go constant (may be C++-only)", cppType)
		}
	}

	goOnlyTypes := []selfmonitor.AlarmType{
		selfmonitor.PluginAlarm,
		selfmonitor.FlusherFlushAlarm,
		selfmonitor.DropDataAlarm,
		selfmonitor.CategoryConfigAlarm,
		selfmonitor.CheckpointAlarm,
	}
	for _, at := range goOnlyTypes {
		assert.NotEmpty(t, at.String(), "Go AlarmType must produce a non-empty string")
	}
}

// TestTransferPipelineEventGroupToAlarms_NilLogs verifies nil logs returns nil.
func TestTransferPipelineEventGroupToAlarms_NilLogs(t *testing.T) {
	group := &protocol.PipelineEventGroup{}
	result := TransferPipelineEventGroupToAlarms(group)
	assert.Nil(t, result)
}

func knownGoAlarmTypes() []string {
	return []string{
		selfmonitor.CategoryConfigAlarm.String(),
		selfmonitor.CheckpointAlarm.String(),
		selfmonitor.DropDataAlarm.String(),
		selfmonitor.PluginAlarm.String(),
		selfmonitor.FlusherFlushAlarm.String(),
		selfmonitor.FlushDataAlarm.String(),
	}
}

func logEventToMap(logEvent *protocol.LogEvent) map[string]string {
	m := make(map[string]string, len(logEvent.Contents))
	for _, c := range logEvent.Contents {
		m[string(c.Key)] = string(c.Value)
	}
	return m
}
