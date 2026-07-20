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

package gotime

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
)

func TestProcessorGotime_ProcessV2ReformatsDestKey(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("s_key", "2019-07-05 19:28:01")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	// Source parsed in UTC+8, reformatted into UTC+9.
	assert.Equal(t, "2019/07/05 20:28:01", log.GetIndices().Get("d_key"))

	// SetTime is true: log timestamp is set (nanoseconds) from the parsed time.
	destLocation := time.FixedZone("SpecifiedTimezone", 9*60*60)
	expected, _ := time.ParseInLocation("2006-01-02 15:04:05", "2019-07-05 20:28:01", destLocation)
	assert.Equal(t, uint64(expected.Unix())*uint64(time.Second), log.GetTimestamp())
}

func TestProcessorGotime_ProcessV2KeepSourceFalse(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	processor.KeepSource = false

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("s_key", "2019-07-05 19:28:01")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.False(t, log.GetIndices().Contains("s_key"), "source key must be removed when KeepSource is false")
	assert.Equal(t, "2019/07/05 20:28:01", log.GetIndices().Get("d_key"))
}

func TestProcessorGotime_ProcessV2TimestampSeconds(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	processor.SourceFormat = fixedSecondsTimestampPattern
	require.NoError(t, processor.Init(processor.context))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("s_key", "1645595256")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.Equal(t, "2022/02/23 14:47:36", log.GetIndices().Get("d_key"))
}

func TestProcessorGotime_ProcessV2PassesThroughMetric(t *testing.T) {
	processor, err := newProcessor()
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
