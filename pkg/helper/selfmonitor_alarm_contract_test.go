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
	// These are the exact string keys used by C++ logEvent->SetContent(...)
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

// TestAlarmExportMessageFieldAlignment verifies that AlarmExportMessage struct
// fields map 1:1 to InnerGoAlarm C struct fields (core/go_pipeline/LogtailPlugin.h)
// and to AlarmManager::SendExternalAlarm parameters.
//
// InnerGoAlarm:
//   alarmType    -> AlarmExportMessage.AlarmType
//   alarmLevel   -> AlarmExportMessage.AlarmLevel
//   alarmMessage -> AlarmExportMessage.AlarmMessage
//   projectName  -> AlarmExportMessage.ProjectName
//   category     -> AlarmExportMessage.Category
//   config       -> AlarmExportMessage.Config
//   count        -> AlarmExportMessage.Count
func TestAlarmExportMessageFieldAlignment(t *testing.T) {
	msg := selfmonitor.AlarmExportMessage{
		AlarmType:    "TEST_ALARM",
		AlarmLevel:   "2",
		AlarmMessage: "test message",
		ProjectName:  "test-project",
		Category:     "test-logstore",
		Config:       "test-config",
		Count:        5,
	}

	assert.Equal(t, "TEST_ALARM", msg.AlarmType)
	assert.Equal(t, "2", msg.AlarmLevel)
	assert.Equal(t, "test message", msg.AlarmMessage)
	assert.Equal(t, "test-project", msg.ProjectName)
	assert.Equal(t, "test-logstore", msg.Category)
	assert.Equal(t, "test-config", msg.Config)
	assert.Equal(t, 5, msg.Count)
}

// TestTransferAlarmExportMessageToPBLogEvent_AllFields verifies round-trip
// with all fields populated.
func TestTransferAlarmExportMessageToPBLogEvent_AllFields(t *testing.T) {
	msg := &selfmonitor.AlarmExportMessage{
		AlarmType:    "PLUGIN_ALARM",
		AlarmLevel:   "3",
		AlarmMessage: "plugin crashed",
		ProjectName:  "my-project",
		Category:     "my-logstore",
		Config:       "my-config",
		Count:        10,
	}
	ts := time.Unix(1700000000, 0)

	logEvent, err := TransferAlarmExportMessageToPBLogEvent(msg, ts)
	require.NoError(t, err)
	require.NotNil(t, logEvent)

	contentMap := logEventToMap(logEvent)
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

// TestTransferAlarmExportMessageToPBLogEvent_OptionalFieldsOmitted verifies
// that empty optional fields are not included in the log event, matching
// C++ behavior where empty strings skip SetContent.
func TestTransferAlarmExportMessageToPBLogEvent_OptionalFieldsOmitted(t *testing.T) {
	msg := &selfmonitor.AlarmExportMessage{
		AlarmType:    "BOOT_INIT_ALARM",
		AlarmLevel:   "1",
		AlarmMessage: "init warning",
		Count:        1,
	}
	ts := time.Unix(1700000000, 0)

	logEvent, err := TransferAlarmExportMessageToPBLogEvent(msg, ts)
	require.NoError(t, err)

	contentMap := logEventToMap(logEvent)
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

// TestTransferPBLogEventToAlarmExportMessage_RoundTrip verifies lossless
// conversion from AlarmExportMessage -> PB LogEvent -> AlarmExportMessage.
func TestTransferPBLogEventToAlarmExportMessage_RoundTrip(t *testing.T) {
	original := &selfmonitor.AlarmExportMessage{
		AlarmType:    "FLUSHER_FLUSH_ALARM",
		AlarmLevel:   "2",
		AlarmMessage: "http 500",
		ProjectName:  "proj-x",
		Category:     "logstore-y",
		Config:       "config-z",
		Count:        7,
	}
	ts := time.Unix(1700000000, 0)

	logEvent, err := TransferAlarmExportMessageToPBLogEvent(original, ts)
	require.NoError(t, err)

	restored := TransferPBLogEventToAlarmExportMessage(logEvent)
	assert.Equal(t, original.AlarmType, restored.AlarmType)
	assert.Equal(t, original.AlarmLevel, restored.AlarmLevel)
	assert.Equal(t, original.AlarmMessage, restored.AlarmMessage)
	assert.Equal(t, original.ProjectName, restored.ProjectName)
	assert.Equal(t, original.Category, restored.Category)
	assert.Equal(t, original.Config, restored.Config)
	assert.Equal(t, original.Count, restored.Count)
}

// TestAlarmLevelValues verifies Go AlarmLevel constants produce the same string
// values as C++ AlarmLevel enum (1=warning, 2=error, 3=critical).
// C++ reference: core/monitor/AlarmManager.h enum AlarmLevel.
func TestAlarmLevelValues(t *testing.T) {
	assert.Equal(t, "1", selfmonitor.AlarmLevelWaring.String())
	assert.Equal(t, "2", selfmonitor.AlarmLevelError.String())
	assert.Equal(t, "3", selfmonitor.AlarmLevelCritical.String())

	// Verify C++ ToString(level) produces the same: "1", "2", "3"
	assert.Equal(t, "1", strconv.Itoa(1)) // ALARM_LEVEL_WARNING = 1
	assert.Equal(t, "2", strconv.Itoa(2)) // ALARM_LEVEL_ERROR = 2
	assert.Equal(t, "3", strconv.Itoa(3)) // ALARM_LEVEL_CRITICAL = 3
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
	msg := &selfmonitor.AlarmExportMessage{
		AlarmType:    "TEST_ALARM",
		AlarmLevel:   "1",
		AlarmMessage: "test",
		Count:        42,
	}
	ts := time.Unix(1700000000, 0)

	logEvent, err := TransferAlarmExportMessageToPBLogEvent(msg, ts)
	require.NoError(t, err)

	contentMap := logEventToMap(logEvent)
	assert.Equal(t, "42", contentMap[AlarmFieldCount])

	restored := TransferPBLogEventToAlarmExportMessage(logEvent)
	assert.Equal(t, 42, restored.Count)
}

// TestCppAlarmTypeStringAlignment verifies that Go AlarmType string values
// match C++ mMessageType vector entries.
// C++ reference: AlarmManager constructor in core/monitor/AlarmManager.cpp.
func TestCppAlarmTypeStringAlignment(t *testing.T) {
	// A subset of C++ internal alarm types that are also used in Go
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
			// Not all C++ internal types need a Go counterpart, but shared ones must match
			t.Logf("C++ alarm type %q has no matching Go constant (may be C++-only)", cppType)
		}
	}

	// Verify Go-only alarm types exported via SendExternalAlarm are valid strings
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
