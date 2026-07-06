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

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

// TestMarshalAlarmsPBRoundTrip verifies that alarms marshaled by the D3 push path
// deserialize back to the same AlarmExportMessage set via the D2 contract helper.
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

	data, err := marshalAlarmsPB(alarms, now)
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

// TestMarshalAlarmsPBEmpty verifies an empty alarm batch still produces a valid,
// parseable PipelineEventGroup (yielding zero alarms).
func TestMarshalAlarmsPBEmpty(t *testing.T) {
	data, err := marshalAlarmsPB(nil, time.Unix(1700000000, 0))
	require.NoError(t, err)

	group := &protocol.PipelineEventGroup{}
	require.NoError(t, group.Unmarshal(data))
	assert.Empty(t, helper.TransferPipelineEventGroupToAlarms(group))
}

// TestMarshalMetricsPBRoundTrip verifies that raw metric records marshaled by the D3
// push path deserialize back to the expected MetricExportRecord set.
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

	data, err := marshalMetricsPB(raw, now)
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
