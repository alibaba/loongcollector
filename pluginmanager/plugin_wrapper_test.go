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

package pluginmanager

import (
	"encoding/json"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

func TestInputWrapperInitMetricRecordRegistersPluginSource(t *testing.T) {
	ctx := &ContextImp{}
	ctx.InitContext("test-project", "test-logstore", "test-pipeline")
	wrapper := &InputWrapper{
		Config: &LogstoreConfig{Context: ctx},
	}
	pluginMeta := &pipeline.PluginMeta{
		PluginID:   "1",
		PluginType: "service_mysql",
	}

	wrapper.InitMetricRecord(pluginMeta)

	require.NotNil(t, wrapper.MetricRecord)
	require.NotNil(t, wrapper.SourceMetricRecord)
	assert.Equal(t, wrapper.SourceMetricRecord, ctx.GetMetricRecord())

	assert.Equal(t, selfmonitor.MetricLabelValueMetricCategoryPlugin, labelValue(wrapper.MetricRecord.Labels, selfmonitor.MetricLabelKeyMetricCategory))
	assert.Equal(t, selfmonitor.MetricLabelValueMetricCategoryPluginSource, labelValue(wrapper.SourceMetricRecord.Labels, selfmonitor.MetricLabelKeyMetricCategory))
	assert.Equal(t, "service_mysql", labelValue(wrapper.SourceMetricRecord.Labels, selfmonitor.MetricLabelKeyPluginType))

	wrapper.outEventsTotal.Add(5)
	wrapper.outEventGroupsTotal.Add(2)
	wrapper.outSizeBytes.Add(100)

	records := ctx.ExportMetricRecords()
	require.Len(t, records, 2)

	pluginCounters, pluginCategory := parseExportedRecord(t, records[0])
	sourceCounters, sourceCategory := parseExportedRecord(t, records[1])
	assert.Equal(t, selfmonitor.MetricLabelValueMetricCategoryPlugin, pluginCategory)
	assert.Equal(t, selfmonitor.MetricLabelValueMetricCategoryPluginSource, sourceCategory)
	assert.Equal(t, "5.0000", pluginCounters[selfmonitor.MetricPluginOutEventsTotal])
	assert.Equal(t, "5.0000", sourceCounters[selfmonitor.MetricPluginOutEventsTotal])
	assert.Equal(t, "2.0000", pluginCounters[selfmonitor.MetricPluginOutEventGroupsTotal])
	assert.Equal(t, "2.0000", sourceCounters[selfmonitor.MetricPluginOutEventGroupsTotal])
	assert.Equal(t, "100.0000", pluginCounters[selfmonitor.MetricPluginOutSizeBytes])
	assert.Equal(t, "100.0000", sourceCounters[selfmonitor.MetricPluginOutSizeBytes])
}

func TestProcessorWrapperInitMetricRecordStaysPluginCategory(t *testing.T) {
	ctx := &ContextImp{}
	ctx.InitContext("test-project", "test-logstore", "test-pipeline")
	wrapper := &ProcessorWrapper{
		Config: &LogstoreConfig{Context: ctx},
	}
	wrapper.InitMetricRecord(&pipeline.PluginMeta{
		PluginID:   "2",
		PluginType: "processor_regex",
	})

	assert.Equal(t, selfmonitor.MetricLabelValueMetricCategoryPlugin, labelValue(wrapper.MetricRecord.Labels, selfmonitor.MetricLabelKeyMetricCategory))
	assert.Equal(t, wrapper.MetricRecord, ctx.GetMetricRecord())
	assert.Len(t, ctx.ExportMetricRecords(), 1)
}

func labelValue(labels []selfmonitor.LabelPair, key string) string {
	for _, label := range labels {
		if label.Key == key {
			return label.Value
		}
	}
	return ""
}

func parseExportedRecord(t *testing.T, raw map[string]string) (counters map[string]string, category string) {
	t.Helper()
	var labels map[string]string
	require.NoError(t, json.Unmarshal([]byte(raw[selfmonitor.MetricLabelPrefix]), &labels))
	require.NoError(t, json.Unmarshal([]byte(raw[selfmonitor.MetricCounterPrefix]), &counters))
	return counters, labels[selfmonitor.MetricLabelKeyMetricCategory]
}
