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

package processorstrptime

import (
	"strconv"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
)

func TestStrptime_ProcessV2SetsTimestamp(t *testing.T) {
	time.Local = time.UTC
	processor, err := newProcessor("%Y/%m/%d %H:%M:%S", nilUTCOffset, true)
	require.NoError(t, err)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add(defaultSourceKey, "2016/01/02 12:59:59")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	expected := time.Date(2016, time.January, 2, 12, 59, 59, 0, time.UTC)
	// models.Log.Timestamp is in nanoseconds.
	assert.Equal(t, uint64(expected.Unix())*uint64(time.Second), log.GetTimestamp())
	// Source key kept by default, precise timestamp written.
	assert.True(t, log.GetIndices().Contains(defaultSourceKey))
	require.True(t, log.GetIndices().Contains(defaultPreciseTimestampKey))
	assert.Equal(t, strconv.FormatInt(expected.UnixNano()/1e6, 10),
		log.GetIndices().Get(defaultPreciseTimestampKey))
}

func TestStrptime_ProcessV2KeepSourceFalse(t *testing.T) {
	time.Local = time.UTC
	processor, err := newProcessor("%Y/%m/%d", nilUTCOffset, false)
	require.NoError(t, err)
	processor.KeepSource = false

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add(defaultSourceKey, "2016/01/02")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	expected := time.Date(2016, time.January, 2, 0, 0, 0, 0, time.UTC)
	assert.Equal(t, uint64(expected.Unix())*uint64(time.Second), log.GetTimestamp())
	assert.False(t, log.GetIndices().Contains(defaultSourceKey), "source key must be removed when KeepSource is false")
}

func TestStrptime_ProcessV2PassesThroughMetric(t *testing.T) {
	processor, err := newProcessor("%Y/%m/%d", nilUTCOffset, true)
	require.NoError(t, err)

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}
