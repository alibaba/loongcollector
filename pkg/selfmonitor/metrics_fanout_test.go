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

package selfmonitor

import (
	"encoding/json"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestFanoutCounterAdd(t *testing.T) {
	pluginRecord := &MetricsRecord{}
	sourceRecord := &MetricsRecord{}
	pluginCounter := NewCounterMetricAndRegister(pluginRecord, MetricPluginOutEventsTotal)
	sourceCounter := NewCounterMetricAndRegister(sourceRecord, MetricPluginOutEventsTotal)
	fanout := NewFanoutCounter(pluginCounter, sourceCounter, nil)

	fanout.Add(4)

	pluginExport := pluginRecord.ExportMetricRecords()
	sourceExport := sourceRecord.ExportMetricRecords()

	var pluginCounters map[string]string
	require.NoError(t, json.Unmarshal([]byte(pluginExport[MetricCounterPrefix]), &pluginCounters))
	var sourceCounters map[string]string
	require.NoError(t, json.Unmarshal([]byte(sourceExport[MetricCounterPrefix]), &sourceCounters))
	assert.Equal(t, "4.0000", pluginCounters[MetricPluginOutEventsTotal])
	assert.Equal(t, "4.0000", sourceCounters[MetricPluginOutEventsTotal])
	assert.Equal(t, CounterType, fanout.Type())
}
