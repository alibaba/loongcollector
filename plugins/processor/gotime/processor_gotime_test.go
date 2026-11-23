// Copyright 2021 iLogtail Authors
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
	"strings"
	"testing"
	"time"

	"github.com/alibaba/ilogtail/plugins/processor/internal/processortest"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func init() {
	logger.InitTestLogger(logger.OptionOpenMemoryReceiver)
}

func newProcessor() (*ProcessorGotime, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorGotime{
		SourceKey:      "s_key",
		SourceFormat:   "2006-01-02 15:04:05",
		SourceLocation: 8,       // 8，为空表示本机时区
		DestKey:        "d_key", // 目标Key，为空不生效
		DestFormat:     "2006/01/02 15:04:05",
		DestLocation:   9,    // 8，为空表示本机时区
		SetTime:        true, // 是否设置到时间字段，默认为true
		KeepSource:     true, // 是否保留源字段
		NoKeyError:     true,
		AlarmIfFail:    true,
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestSourceKey(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	log := processortest.Log()
	timeStr := "2019-07-05 19:28:01"
	log.Contents.Add("s_key", timeStr)
	processortest.Apply(processor, log)
	assert.Equal(t, "2019/07/05 20:28:01", pipeline.GetStringValue(log.Contents.Get("d_key")))
}

func TestSetTime(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	log := processortest.Log()
	timeStr := "2019-07-05 19:28:01"
	log.Contents.Add("s_key", timeStr)
	processortest.Apply(processor, log)
	destLocation := time.FixedZone("SpecifiedTimezone", 9*60*60)
	time1, _ := time.ParseInLocation("2006-01-02 15:04:05", "2019-07-05 20:28:01", destLocation)
	unixTime1 := uint64(time1.UnixNano())
	assert.Equal(t, unixTime1, log.GetTimestamp())
}

func TestKeepSource(t *testing.T) {
	processor, err := newProcessor()
	if processor == nil {
		return
	}
	processor.KeepSource = false
	require.NoError(t, err)
	log := processortest.Log()
	timeStr := "2019-07-05 19:28:01"
	log.Contents.Add("s_key", timeStr)
	processortest.Apply(processor, log)

}

func TestNoKeyError(t *testing.T) {
	logger.ClearMemoryLog()
	processor, err := newProcessor()
	require.NoError(t, err)

	log := processortest.Log()
	processortest.Apply(processor, log)
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.True(t, strings.Contains(memoryLog, "GOTIME_FIND_ALARM\tcannot find key s_key"))
}

func TestAlarmIfFail(t *testing.T) {
	logger.ClearMemoryLog()
	processor, err := newProcessor()
	require.NoError(t, err)
	log := processortest.Log()
	timeStr := "2019-07-05-19:28:01"
	log.Contents.Add("s_key", timeStr)
	processortest.Apply(processor, log)
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.True(t, strings.Contains(memoryLog, "GOTIME_PARSE_ALARM\tParseInLocation(2006-01-02 15:04:05, 2019-07-05-19:28:01, SpecifiedTimezone) "+
		"failed: parsing time \"2019-07-05-19:28:01\" as \"2006-01-02 15:04:05\": cannot parse \"-19:28:01\" as \" \""))
}

func TestSourceFormatTimestampSeconds(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	log := processortest.Log()
	unixStr := "1645595256"
	log.Contents.Add("s_key", unixStr)
	processor.SourceFormat = fixedSecondsTimestampPattern
	_ = processor.Init(processor.context)

	processortest.Apply(processor, log)
	assert.Equal(t, "2022/02/23 14:47:36", pipeline.GetStringValue(log.Contents.Get("d_key")))
}

func TestSourceFormatTimestampMilliseconds(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	processor.SourceFormat = fixedMillisecondsTimestampPattern
	processor.DestFormat = "2006/01/02 15:04:05.000"
	_ = processor.Init(processor.context)

	log := processortest.Log()
	unixStr := "1645595256807"
	log.Contents.Add("s_key", unixStr)

	processortest.Apply(processor, log)
	assert.Equal(t, "2022/02/23 14:47:36.807", pipeline.GetStringValue(log.Contents.Get("d_key")))
}

func TestSourceFormatTimestampMicroseconds(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	log := processortest.Log()
	unixStr := "1645595256807000"
	log.Contents.Add("s_key", unixStr)
	processor.SourceFormat = fixedMicrosecondsTimestampPattern
	processor.DestFormat = "2006/01/02 15:04:05.000000"
	_ = processor.Init(processor.context)
	processortest.Apply(processor, log)
	assert.Equal(t, "2022/02/23 14:47:36.807000", pipeline.GetStringValue(log.Contents.Get("d_key")))
}

func TestSourceFormatTimestampNanoseconds(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	log := processortest.Log()
	unixStr := "1645595256807000123"
	log.Contents.Add("s_key", unixStr)
	processor.SourceFormat = fixedNanosecondsTimestampPattern
	processor.DestFormat = "2006/01/02 15:04:05.000000000"
	_ = processor.Init(processor.context)
	processortest.Apply(processor, log)
	assert.Equal(t, "2022/02/23 14:47:36.807000123", pipeline.GetStringValue(log.Contents.Get("d_key")))
}

func TestEmptyTimezone(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorGotime{
		SourceKey:      "s_key",
		SourceFormat:   "2006-01-02 15:04:05",
		SourceLocation: machineTimeZone,
		DestKey:        "d_key",
		DestFormat:     "2006/01/02 15:04:05",
		DestLocation:   9,
		SetTime:        true,
		KeepSource:     true,
		NoKeyError:     true,
		AlarmIfFail:    true,
	}
	err := processor.Init(ctx)
	require.NoError(t, err)

	log := processortest.Log()
	timeStr := "2019-07-05 19:28:01"
	localTime, _ := time.ParseInLocation("2006-01-02 15:04:05", timeStr, time.Local)
	log.Contents.Add("s_key", localTime.Format(processor.SourceFormat))
	processortest.Apply(processor, log)
	destLocation := time.FixedZone("SpecifiedTimezone", 9*60*60)
	processedTime := localTime.In(destLocation)
	assert.Equal(t, processedTime.Format(processor.DestFormat), pipeline.GetStringValue(log.Contents.Get("d_key")))
}

func TestProcessorGotime_ProcessReformatsDestKey(t *testing.T) {
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

func TestProcessorGotime_ProcessKeepSourceFalse(t *testing.T) {
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

func TestProcessorGotime_ProcessTimestampSeconds(t *testing.T) {
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

func TestProcessorGotime_ProcessPassesThroughMetric(t *testing.T) {
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
