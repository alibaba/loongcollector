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
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

// TestMetricContractCategories verifies Go categories match C++ MetricCategory
// constants (core/monitor/metric_models/MetricRecord.cpp).
func TestMetricContractCategories(t *testing.T) {
	cppCategories := map[string]bool{
		"agent":         true,
		"runner":        true,
		"pipeline":      true,
		"component":     true,
		"plugin":        true,
		"plugin_source": true,
	}

	goCategories := MetricContractCategories()
	for _, cat := range goCategories {
		assert.True(t, cppCategories[cat], "Go category %q not found in C++ MetricCategory", cat)
	}
	for cat := range cppCategories {
		found := false
		for _, goCat := range goCategories {
			if goCat == cat {
				found = true
				break
			}
		}
		assert.True(t, found, "C++ category %q not covered in Go contract", cat)
	}
}

// TestTransferMetricsToPipelineEventGroup_AllFields verifies conversion with
// counters, gauges, and labels produces a correct MetricEvent.
func TestTransferMetricsToPipelineEventGroup_AllFields(t *testing.T) {
	records := []selfmonitor.MetricExportRecord{
		{
			Category: "plugin",
			Labels: map[string]string{
				"pipeline_name": "test-pipeline",
				"plugin_type":   "flusher_stdout",
			},
			Counters: map[string]uint64{
				"in_events_total":  100,
				"out_events_total": 95,
			},
			Gauges: map[string]float64{
				"cache_size": 1024.5,
			},
		},
	}
	ts := time.Unix(1700000000, 0)

	group := TransferMetricsToPipelineEventGroup(records, ts)
	require.NotNil(t, group)
	require.NotNil(t, group.GetMetrics())
	require.Len(t, group.GetMetrics().Events, 1)

	event := group.GetMetrics().Events[0]
	assert.Equal(t, "plugin", string(event.Name))
	assert.Equal(t, uint64(1700000000)*1e9, event.Timestamp)
	assert.Equal(t, "test-pipeline", string(event.Tags["pipeline_name"]))
	assert.Equal(t, "flusher_stdout", string(event.Tags["plugin_type"]))

	multiValues := event.GetUntypedMultiDoubleValues()
	require.NotNil(t, multiValues)
	require.Len(t, multiValues.Values, 3)

	inEvents := multiValues.Values["in_events_total"]
	require.NotNil(t, inEvents)
	assert.Equal(t, protocol.UntypedValueMetricType_METRIC_TYPE_COUNTER, inEvents.MetricType)
	assert.Equal(t, float64(100), inEvents.Value)

	outEvents := multiValues.Values["out_events_total"]
	require.NotNil(t, outEvents)
	assert.Equal(t, protocol.UntypedValueMetricType_METRIC_TYPE_COUNTER, outEvents.MetricType)
	assert.Equal(t, float64(95), outEvents.Value)

	cacheSize := multiValues.Values["cache_size"]
	require.NotNil(t, cacheSize)
	assert.Equal(t, protocol.UntypedValueMetricType_METRIC_TYPE_GAUGE, cacheSize.MetricType)
	assert.Equal(t, 1024.5, cacheSize.Value)
}

// TestTransferMetricsToPipelineEventGroup_EmptyCountersAndGauges verifies that
// records with no counters or gauges produce a MetricEvent with empty values.
func TestTransferMetricsToPipelineEventGroup_EmptyCountersAndGauges(t *testing.T) {
	records := []selfmonitor.MetricExportRecord{
		{
			Category: "runner",
			Labels:   map[string]string{"runner_name": "k8s_meta"},
			Counters: map[string]uint64{},
			Gauges:   map[string]float64{},
		},
	}
	ts := time.Unix(1700000000, 0)

	group := TransferMetricsToPipelineEventGroup(records, ts)
	require.NotNil(t, group.GetMetrics())
	require.Len(t, group.GetMetrics().Events, 1)

	multiValues := group.GetMetrics().Events[0].GetUntypedMultiDoubleValues()
	require.NotNil(t, multiValues)
	assert.Empty(t, multiValues.Values)
}

// TestTransferPipelineEventGroupToMetrics_RoundTrip verifies lossless round-trip
// conversion: []MetricExportRecord -> PipelineEventGroup -> []MetricExportRecord.
func TestTransferPipelineEventGroupToMetrics_RoundTrip(t *testing.T) {
	original := []selfmonitor.MetricExportRecord{
		{
			Category: "plugin",
			Labels: map[string]string{
				"pipeline_name": "pipeline-1",
				"plugin_type":   "input_file",
			},
			Counters: map[string]uint64{
				"in_events_total":  500,
				"in_size_bytes":    2048,
				"out_events_total": 500,
			},
			Gauges: map[string]float64{
				"total_delay_ms": 12.5,
			},
		},
		{
			Category: "agent",
			Labels:   map[string]string{},
			Counters: map[string]uint64{},
			Gauges: map[string]float64{
				"go_memory_used_mb": 128,
				"go_routines_total": 42,
			},
		},
	}
	ts := time.Unix(1700000000, 0)

	group := TransferMetricsToPipelineEventGroup(original, ts)
	restored := TransferPipelineEventGroupToMetrics(group)
	require.Len(t, restored, 2)

	assert.Equal(t, original[0].Category, restored[0].Category)
	assert.Equal(t, original[0].Labels, restored[0].Labels)
	assert.Equal(t, original[0].Counters, restored[0].Counters)
	assert.Equal(t, original[0].Gauges, restored[0].Gauges)

	assert.Equal(t, original[1].Category, restored[1].Category)
	assert.Equal(t, original[1].Counters, restored[1].Counters)
	assert.InDelta(t, original[1].Gauges["go_memory_used_mb"], restored[1].Gauges["go_memory_used_mb"], 0.001)
	assert.InDelta(t, original[1].Gauges["go_routines_total"], restored[1].Gauges["go_routines_total"], 0.001)
}

// TestTransferPipelineEventGroupToMetrics_NilMetrics verifies nil metrics returns nil.
func TestTransferPipelineEventGroupToMetrics_NilMetrics(t *testing.T) {
	group := &protocol.PipelineEventGroup{}
	result := TransferPipelineEventGroupToMetrics(group)
	assert.Nil(t, result)
}

// TestTransferMetricsToPipelineEventGroup_Multiple verifies batch conversion
// produces one MetricEvent per record.
func TestTransferMetricsToPipelineEventGroup_Multiple(t *testing.T) {
	records := []selfmonitor.MetricExportRecord{
		{Category: "plugin", Labels: map[string]string{"plugin_type": "a"}, Counters: map[string]uint64{"x": 1}, Gauges: map[string]float64{}},
		{Category: "runner", Labels: map[string]string{"runner_name": "b"}, Counters: map[string]uint64{}, Gauges: map[string]float64{"y": 2}},
		{Category: "pipeline", Labels: map[string]string{"pipeline_name": "c"}, Counters: map[string]uint64{"z": 3}, Gauges: map[string]float64{}},
	}
	ts := time.Unix(1700000000, 0)

	group := TransferMetricsToPipelineEventGroup(records, ts)
	require.Len(t, group.GetMetrics().Events, 3)

	assert.Equal(t, "plugin", string(group.GetMetrics().Events[0].Name))
	assert.Equal(t, "runner", string(group.GetMetrics().Events[1].Name))
	assert.Equal(t, "pipeline", string(group.GetMetrics().Events[2].Name))
}

// TestTransferRawMetricsToPipelineEventGroup verifies the convenience wrapper
// that accepts the existing map[string]string export format.
func TestTransferRawMetricsToPipelineEventGroup(t *testing.T) {
	rawMetrics := []map[string]string{
		{
			"labels":   `{"metric_category":"plugin","plugin_type":"flusher_stdout"}`,
			"counters": `{"in_events_total":"100"}`,
			"gauges":   `{"cache_size":"50.5"}`,
		},
	}
	ts := time.Unix(1700000000, 0)

	group := TransferRawMetricsToPipelineEventGroup(rawMetrics, ts)
	require.NotNil(t, group.GetMetrics())
	require.Len(t, group.GetMetrics().Events, 1)

	event := group.GetMetrics().Events[0]
	assert.Equal(t, "plugin", string(event.Name))
	assert.Equal(t, "flusher_stdout", string(event.Tags["plugin_type"]))
	_, hasCategoryTag := event.Tags[MetricFieldCategory]
	assert.False(t, hasCategoryTag, "metric_category should be extracted to Name, not kept in Tags")

	multiValues := event.GetUntypedMultiDoubleValues()
	require.NotNil(t, multiValues)

	inEvents := multiValues.Values["in_events_total"]
	require.NotNil(t, inEvents)
	assert.Equal(t, protocol.UntypedValueMetricType_METRIC_TYPE_COUNTER, inEvents.MetricType)
	assert.Equal(t, float64(100), inEvents.Value)

	cacheSize := multiValues.Values["cache_size"]
	require.NotNil(t, cacheSize)
	assert.Equal(t, protocol.UntypedValueMetricType_METRIC_TYPE_GAUGE, cacheSize.MetricType)
	assert.InDelta(t, 50.5, cacheSize.Value, 0.001)
}

// TestMetricCounterSerialization verifies counter values are correctly preserved
// through the PB round-trip, matching C++ stod() parsing behavior.
func TestMetricCounterSerialization(t *testing.T) {
	records := []selfmonitor.MetricExportRecord{
		{
			Category: "plugin",
			Labels:   map[string]string{},
			Counters: map[string]uint64{"in_events_total": 999999},
			Gauges:   map[string]float64{},
		},
	}
	ts := time.Unix(1700000000, 0)

	group := TransferMetricsToPipelineEventGroup(records, ts)
	restored := TransferPipelineEventGroupToMetrics(group)
	require.Len(t, restored, 1)
	assert.Equal(t, uint64(999999), restored[0].Counters["in_events_total"])
}

// TestMetricGaugeSerialization verifies gauge values including fractional parts
// are correctly preserved through the PB round-trip.
func TestMetricGaugeSerialization(t *testing.T) {
	records := []selfmonitor.MetricExportRecord{
		{
			Category: "agent",
			Labels:   map[string]string{},
			Counters: map[string]uint64{},
			Gauges:   map[string]float64{"go_memory_used_mb": 256.75},
		},
	}
	ts := time.Unix(1700000000, 0)

	group := TransferMetricsToPipelineEventGroup(records, ts)
	restored := TransferPipelineEventGroupToMetrics(group)
	require.Len(t, restored, 1)
	assert.InDelta(t, 256.75, restored[0].Gauges["go_memory_used_mb"], 0.001)
}

// TestFormatCounterValue verifies counter formatting matches ExportMetricRecords.
func TestFormatCounterValue(t *testing.T) {
	assert.Equal(t, "0", FormatCounterValue(0))
	assert.Equal(t, "42", FormatCounterValue(42))
	assert.Equal(t, "18446744073709551615", FormatCounterValue(^uint64(0)))
}

// TestFormatGaugeValue verifies gauge formatting matches ExportMetricRecords.
func TestFormatGaugeValue(t *testing.T) {
	assert.Equal(t, "0", FormatGaugeValue(0))
	assert.Equal(t, "1024.5", FormatGaugeValue(1024.5))
	assert.Equal(t, "1e+06", FormatGaugeValue(1e6))
}

// TestParseMetricExportRecord_RoundTrip verifies parse from raw map matches structured record.
func TestParseMetricExportRecord_RoundTrip(t *testing.T) {
	raw := map[string]string{
		"labels":   `{"metric_category":"runner","runner_name":"k8s_meta","cluster_id":"test-cluster"}`,
		"counters": `{"collect_entity_total":"200"}`,
		"gauges":   `{"cache_size":"1024"}`,
	}

	record, err := selfmonitor.ParseMetricExportRecord(raw)
	require.NoError(t, err)
	assert.Equal(t, "runner", record.Category)
	assert.Equal(t, "k8s_meta", record.Labels["runner_name"])
	assert.Equal(t, "test-cluster", record.Labels["cluster_id"])
	_, hasCat := record.Labels["metric_category"]
	assert.False(t, hasCat, "metric_category should be extracted to Category field")
	assert.Equal(t, uint64(200), record.Counters["collect_entity_total"])
	assert.InDelta(t, 1024.0, record.Gauges["cache_size"], 0.001)
}

// TestParseMetricExportRecord_EmptyFields verifies parse with empty JSON values.
func TestParseMetricExportRecord_EmptyFields(t *testing.T) {
	raw := map[string]string{
		"labels":   `{}`,
		"counters": `{}`,
		"gauges":   `{}`,
	}

	record, err := selfmonitor.ParseMetricExportRecord(raw)
	require.NoError(t, err)
	assert.Equal(t, "", record.Category)
	assert.Empty(t, record.Labels)
	assert.Empty(t, record.Counters)
	assert.Empty(t, record.Gauges)
}
