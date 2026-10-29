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

package main

import (
	"testing"
	"time"
	"unsafe"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/pluginmanager"
)

// TestMarshalAlarmsPBRoundTrip verifies that alarms marshaled on the Go side for the
// PB pull path deserialize back to the same AlarmExportMessage set via the shared contract
// helper.
func TestMarshalAlarmsPBRoundTrip(t *testing.T) {
	now := time.Unix(1700000000, 0)
	alarms := []selfmonitor.AlarmExportMessage{
		{
			AlarmType:    "PARSE_ERROR_ALARM",
			AlarmLevel:   "2",
			AlarmMessage: "parse failed",
			Count:        3,
			ProjectName:  "proj-a",
			Category:     "logstore-a",
			Config:       "config-a",
		},
		{
			AlarmType:    "SEND_ALARM",
			AlarmLevel:   "3",
			AlarmMessage: "send failed",
			Count:        1,
		},
	}

	data, err := marshalAlarmsPBForTest(alarms, now)
	require.NoError(t, err)
	require.NotEmpty(t, data)

	group := &protocol.PipelineEventGroup{}
	require.NoError(t, group.Unmarshal(data))

	got := helper.TransferPipelineEventGroupToAlarms(group)
	require.Len(t, got, len(alarms))
	for i, want := range alarms {
		assert.Equal(t, want.AlarmType, got[i].AlarmType)
		assert.Equal(t, want.AlarmLevel, got[i].AlarmLevel)
		assert.Equal(t, want.AlarmMessage, got[i].AlarmMessage)
		assert.Equal(t, want.Count, got[i].Count)
		assert.Equal(t, want.ProjectName, got[i].ProjectName)
		assert.Equal(t, want.Category, got[i].Category)
		assert.Equal(t, want.Config, got[i].Config)
	}
}

func TestMarshalMetricsPBCppProvided(t *testing.T) {
	data, err := marshalMetricsPBForTest([]map[string]string{{
		selfmonitor.MetricAgentMemoryGo: "123", selfmonitor.MetricAgentGoRoutinesTotal: "45",
	}}, time.Unix(1700000000, 0))
	require.NoError(t, err)
	var pb protocol.PipelineEventGroup
	require.NoError(t, pb.Unmarshal(data))
	records := helper.TransferPipelineEventGroupToMetrics(&pb)
	require.Len(t, records, 1)
	assert.Equal(t, "agent", records[0].Category)
	assert.Equal(t, float64(123), records[0].Gauges[selfmonitor.MetricAgentMemoryGo])
	assert.Equal(t, float64(45), records[0].Gauges[selfmonitor.MetricAgentGoRoutinesTotal])
}

func TestSelfMonitorPBExportsAllocateAndRelease(t *testing.T) {
	for i := 0; i < 100; i++ {
		buffer := GetGoSelfMonitorMetricsPB(pluginmanager.MetricExportTypeCpp)
		require.NotNil(t, buffer.data)
		require.Positive(t, uint64(buffer.size))
		FreeSelfMonitorPB(buffer.data)
		alarms := GetGoSelfMonitorAlarmsPB()
		FreeSelfMonitorPB(alarms.data)
	}
	FreeSelfMonitorPB(nil)
}

func TestMarshalMetricsPBRejectsInvalidRecord(t *testing.T) {
	_, err := marshalMetricsPBForTest([]map[string]string{{"go_memory_used_mb": "invalid"}}, time.Now())
	require.Error(t, err)
	_, err = marshalMetricsPBForTest([]map[string]string{{"labels": "invalid json"}}, time.Now())
	require.Error(t, err)
}

// TestMarshalAlarmsPBEmpty verifies an empty alarm batch still produces a valid,
// parseable PipelineEventGroup (yielding zero alarms).
func TestMarshalAlarmsPBEmpty(t *testing.T) {
	data, err := marshalAlarmsPBForTest(nil, time.Unix(1700000000, 0))
	require.NoError(t, err)

	group := &protocol.PipelineEventGroup{}
	require.NoError(t, group.Unmarshal(data))
	assert.Empty(t, helper.TransferPipelineEventGroupToAlarms(group))
}

// TestMarshalMetricsPBRoundTrip verifies that raw metric records marshaled on the Go
// side deserialize back to the expected MetricExportRecord set.
func TestMarshalMetricsPBRoundTrip(t *testing.T) {
	now := time.Unix(1700000000, 0)
	raw := []map[string]string{
		{
			selfmonitor.MetricLabelPrefix:   `{"metric_category":"plugin","plugin_type":"flusher_stdout"}`,
			selfmonitor.MetricCounterPrefix: `{"proc_in_records_total":"100"}`,
			selfmonitor.MetricGaugePrefix:   `{}`,
		},
		{
			selfmonitor.MetricLabelPrefix:   `{"metric_category":"runner","runner_name":"k8s_meta"}`,
			selfmonitor.MetricCounterPrefix: `{"proc_in_records_total":"5"}`,
			selfmonitor.MetricGaugePrefix:   `{"cache_size":"12"}`,
		},
	}

	data, err := marshalMetricsPBForTest(raw, now)
	require.NoError(t, err)
	require.NotEmpty(t, data)

	group := &protocol.PipelineEventGroup{}
	require.NoError(t, group.Unmarshal(data))

	got := helper.TransferPipelineEventGroupToMetrics(group)
	require.Len(t, got, len(raw))

	byCategory := make(map[string]selfmonitor.MetricExportRecord, len(got))
	for _, r := range got {
		byCategory[r.Category] = r
	}

	plugin, ok := byCategory["plugin"]
	require.True(t, ok)
	assert.Equal(t, "flusher_stdout", plugin.Labels["plugin_type"])
	assert.Equal(t, uint64(100), plugin.Counters["proc_in_records_total"])

	runner, ok := byCategory["runner"]
	require.True(t, ok)
	assert.Equal(t, "k8s_meta", runner.Labels["runner_name"])
	assert.Equal(t, uint64(5), runner.Counters["proc_in_records_total"])
	assert.Equal(t, float64(12), runner.Gauges["cache_size"])
}

// Copy out only in tests, so assertions also verify bytes outlive FreeSelfMonitorPB.
func marshalMetricsPBForTest(raw []map[string]string, now time.Time) ([]byte, error) {
	group, err := buildMetricsPB(raw, now)
	if err != nil {
		return nil, err
	}
	return copyPBForTest(group), nil
}

func marshalAlarmsPBForTest(alarms []selfmonitor.AlarmExportMessage, now time.Time) ([]byte, error) {
	group, err := buildAlarmsPB(alarms, now)
	if err != nil {
		return nil, err
	}
	return copyPBForTest(group), nil
}

func copyPBForTest(group *protocol.PipelineEventGroup) []byte {
	buffer := toPBBuffer(group, nil)
	defer FreeSelfMonitorPB(buffer.data)
	return append([]byte(nil), unsafe.Slice((*byte)(unsafe.Pointer(buffer.data)), int(buffer.size))...)
}

func TestSelfMonitorPBEmptyBatchHasNoAllocation(t *testing.T) {
	metrics := toPBBuffer(buildMetricsPB(nil, time.Now()))
	require.Nil(t, metrics.data)
	require.Zero(t, uint64(metrics.size))
	alarms := toPBBuffer(buildAlarmsPB(nil, time.Now()))
	require.Nil(t, alarms.data)
	require.Zero(t, uint64(alarms.size))
}

func TestSelfMonitorPBMetricContractCompatibility(t *testing.T) {
	cases := []map[string]string{
		{"labels": `{"metric_category":"plugin","host":"主机","plugin_type":"input_file"}`,
			"counters": `{"events":"100","bad":"not-a-number","fraction":"1.75"}`,
			"gauges":   `{"delay":"12.2500"}`},
		{"labels": "", "counters": "{}", "gauges": "{}"},
		{"labels": `{"metric_category":"agent"}`,
			"counters": `{"shared":"2"}`, "gauges": `{"shared":"3.25"}`},
	}
	now := time.Unix(1700000000, 123)
	for i, raw := range cases {
		actual, err := buildMetricsPB([]map[string]string{raw}, now)
		require.NoError(t, err, "case %d", i)
		expected := helper.TransferRawMetricsToPipelineEventGroup([]map[string]string{raw}, now)
		assert.Equal(t, helper.TransferPipelineEventGroupToMetrics(expected),
			helper.TransferPipelineEventGroupToMetrics(actual), "case %d", i)
		decoded := &protocol.PipelineEventGroup{}
		require.NoError(t, decoded.Unmarshal(copyPBForTest(actual)))
		assert.Equal(t, uint64(now.UnixNano()), decoded.GetMetrics().Events[0].Timestamp)
	}
}
