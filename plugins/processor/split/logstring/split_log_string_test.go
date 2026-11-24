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

package logstring

import (
	"context"
	"fmt"
	"strconv"
	"testing"

	"github.com/alibaba/ilogtail/plugins/processor/internal/processortest"

	"github.com/pingcap/check"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	_ "github.com/alibaba/ilogtail/pkg/logger/test"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

var _ = check.Suite(&processorTestSuite{})

func Test(t *testing.T) {
	check.TestingT(t)
}

func TestDefault(t *testing.T) {
	p := &ProcessorSplit{SplitSep: "\n", PreserveOthers: true}

	log := processortest.Log()
	log.Contents.Add("content", "hello1\nhello2\nhello3")
	log.Contents.Add("__log_topic__", "test")

	logs := processortest.Apply(p, log)
	require.Equal(t, 3, len(logs), "logs: %v", logs)
	for idx, l := range logs {
		require.Equal(t, 2, l.Contents.Len())
		require.Equal(t, "test", pipeline.GetStringValue(l.Contents.Get("__log_topic__")))
		require.Equal(t, "hello"+strconv.Itoa(idx+1), pipeline.GetStringValue(l.Contents.Get("content")))
	}

	fmt.Println(processortest.Apply(p, log))
}

type processorTestSuite struct {
	processor pipeline.Processor
}

func (s *processorTestSuite) SetUpTest(c *check.C) {
	s.processor = pipeline.Processors["processor_split_log_string"]()
	_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
	logger.Info(context.Background(), "set up", s.processor.Description())
}

func (s *processorTestSuite) TearDownTest(c *check.C) {

}

func (s *processorTestSuite) TestDefault(c *check.C) {
	{
		var log = "xxxx\nyyyy\nzzzz"
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 3)
	}
	{
		var log = "xxxx\nyyyy\nzzzz\n"
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 3)
	}
	{
		var log = "\nxxxx\nyyyy\nzzzz\n"
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 3)
	}
}

func (s *processorTestSuite) TestNormal(c *check.C) {
	{
		processor, _ := s.processor.(*ProcessorSplit)
		processor.PreserveOthers = false
		processor.NoKeyError = true
		processor.SplitSep = "你好"
		processor.SplitKey = "content"
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

		var log = "你好[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t你好[你好"
		logEvent := processortest.Log("preserve_1", "1", "content", log, "preserve_2", "2")
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 3)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 1)
		c.Assert(outLogs[1].Contents.Len(), check.Equals, 1)
		c.Assert(outLogs[2].Contents.Len(), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, "[2017-12-12 00:00:00] ")
		c.Assert(pipeline.GetStringValue(outLogs[1].Contents.Get("content")), check.Equals, "\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t")
		c.Assert(pipeline.GetStringValue(outLogs[2].Contents.Get("content")), check.Equals, "[")
	}

	{
		processor, _ := s.processor.(*ProcessorSplit)
		processor.PreserveOthers = true
		processor.NoKeyError = true
		processor.SplitSep = "你好"
		processor.SplitKey = "content"
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t你好["
		logEvent := processortest.Log("preserve_1", "1", "content", log, "preserve_2", "2")
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 3)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 3)
		c.Assert(outLogs[1].Contents.Len(), check.Equals, 3)
		c.Assert(outLogs[2].Contents.Len(), check.Equals, 3)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("preserve_1")), check.Equals, "1")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("preserve_2")), check.Equals, "2")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, "[2017-12-12 00:00:00] ")
		c.Assert(pipeline.GetStringValue(outLogs[1].Contents.Get("preserve_1")), check.Equals, "1")
		c.Assert(pipeline.GetStringValue(outLogs[1].Contents.Get("preserve_2")), check.Equals, "2")
		c.Assert(pipeline.GetStringValue(outLogs[1].Contents.Get("content")), check.Equals, "\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t")
		c.Assert(pipeline.GetStringValue(outLogs[2].Contents.Get("preserve_1")), check.Equals, "1")
		c.Assert(pipeline.GetStringValue(outLogs[2].Contents.Get("preserve_2")), check.Equals, "2")
		c.Assert(pipeline.GetStringValue(outLogs[2].Contents.Get("content")), check.Equals, "[")
	}
}

func (s *processorTestSuite) TestNoKeyAlarmAndPreserve(c *check.C) {
	{
		processor, _ := s.processor.(*ProcessorSplit)
		processor.PreserveOthers = false
		processor.NoKeyError = true
		processor.SplitSep = "\\S+.*"
		processor.SplitKey = "no_this_key"
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t["
		logEvent := processortest.Log("preserve_1", "1", "content", log, "preserve_2", "2")
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 0)
	}

	{
		processor, _ := s.processor.(*ProcessorSplit)
		processor.PreserveOthers = true
		processor.NoKeyError = true
		processor.SplitSep = "\\S+.*"
		processor.SplitKey = "no_this_key"
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t["
		logEvent := processortest.Log("preserve_1", "1", "content", log, "preserve_2", "2")
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("preserve_1")), check.Equals, "1")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t[")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("preserve_2")), check.Equals, "2")
	}
}

func (s *processorTestSuite) TestEnableLogPositionMeta(c *check.C) {

	{
		processor, _ := s.processor.(*ProcessorSplit)
		processor.EnableLogPositionMeta = true
		var log = "xxxx\nyyyy\nzzzz\n"
		logEvent := processortest.Log("content", log)
		logEvent.SetOffset(1000)
		outLogs := processortest.Apply(processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 3)
		for i := 0; i < len(outLogs); i++ {
			c.Assert(outLogs[i].GetOffset(), check.Equals, uint64(1000+i*5))
		}
	}

	{
		processor, _ := s.processor.(*ProcessorSplit)
		processor.EnableLogPositionMeta = true
		var log = "xxxx\nyyyy\nzzzz\n"
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 3)
		for i := 0; i < len(outLogs); i++ {
			c.Assert(outLogs[i].GetOffset(), check.Equals, uint64(0))
		}
	}

	{
		processor, _ := s.processor.(*ProcessorSplit)
		processor.EnableLogPositionMeta = false
		var log = "xxxx\nyyyy\nzzzz\n"
		logEvent := processortest.Log("content", log)
		logEvent.SetOffset(1000)
		outLogs := processortest.Apply(processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 3)
		for i := 0; i < len(outLogs); i++ {
			c.Assert(outLogs[i].GetOffset(), check.Equals, uint64(1000))
		}
	}
}

func newSplitLogString(t *testing.T) *ProcessorSplit {
	processor := pipeline.Processors["processor_split_log_string"]().(*ProcessorSplit)
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
	return processor
}

func TestProcessorSplitLogString_ProcessSplitsLog(t *testing.T) {
	processor := newSplitLogString(t)

	log := models.NewSimpleLog([]byte("hello1\nhello2\nhello3"), models.NewTags(), 0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	results := context.Collector().ToArray()
	var logs int
	for _, group := range results {
		for _, event := range group.Events {
			_, ok := event.(*models.Log)
			assert.True(t, ok, "split result must be Log events")
			logs++
		}
	}
	assert.Equal(t, 3, logs, "input log must be split into 3 logs")
}

func TestProcessorSplitLogStringSplitsConfiguredField(t *testing.T) {
	processor := &ProcessorSplit{SplitKey: "payload", SplitSep: "|", EnableLogPositionMeta: true}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := processortest.Log("content", "raw body", "payload", "first|second")
	log.GetTags().Add("tenant", "example")
	log.Timestamp = 1700000000123456789
	log.SetOffset(100)
	logs := processortest.Apply(processor, log)
	require.Len(t, logs, 2)
	assert.Equal(t, map[string]interface{}{"payload": "first"}, logs[0].GetIndices().Iterator())
	assert.Equal(t, map[string]interface{}{"payload": "second"}, logs[1].GetIndices().Iterator())
	assert.Equal(t, uint64(100), logs[0].GetOffset())
	assert.Equal(t, uint64(106), logs[1].GetOffset())
	assert.Equal(t, log.GetTimestamp(), logs[0].GetTimestamp())
	assert.Equal(t, log.GetTimestamp(), logs[1].GetTimestamp())
	assert.Empty(t, logs[0].GetTags().Iterator())
	assert.Equal(t, "first|second", log.GetIndices().Get("payload"))
}

func TestProcessorSplitLogStringDiscardsEmptySplitField(t *testing.T) {
	processor := &ProcessorSplit{SplitKey: "payload", SplitSep: "|", PreserveOthers: true}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := processortest.Log("payload", "", "other", "keep")
	log.SetBody([]byte("original body"))
	logs := processortest.Apply(processor, log)
	require.Empty(t, logs)

	processor.PreserveOthers = false
	require.Empty(t, processortest.Apply(processor, log))
}

func TestProcessorSplitLogString_ProcessPassesThroughMetric(t *testing.T) {
	processor := newSplitLogString(t)

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}

func TestProcessorSplitLogString_ProcessPassesThroughSpan(t *testing.T) {
	processor := newSplitLogString(t)

	span := models.NewSpan("s", "trace", "span", models.SpanKindClient, 0, 0, models.NewTags(), nil, nil)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{span}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Span)
	assert.True(t, ok, "span event must pass through unchanged")
}
