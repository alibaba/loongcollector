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
	processor := s.processor.(*ProcessorSplit)
	processor.SplitSep = "你好"
	processor.SplitKey = "source"
	processor.NoKeyError = true

	const payload = "你好[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t你好[你好"
	log := processortest.Log("preserve_1", "1", "preserve_2", "2")
	log.GetTags().Add("source", payload)
	log.GetTags().Add("tenant", "keep")

	for _, preserve := range []bool{false, true} {
		processor.PreserveOthers = preserve
		logs := processortest.Apply(processor, log)
		c.Assert(len(logs), check.Equals, 3)
		for i, body := range []string{"[2017-12-12 00:00:00] ", "\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t", "["} {
			c.Assert(string(logs[i].GetBody()), check.Equals, body)
			c.Assert(logs[i].Contents.Len(), check.Equals, 3)
			c.Assert(logs[i].Contents.Get("preserve_1"), check.Equals, "1")
			c.Assert(logs[i].Contents.Get("preserve_2"), check.Equals, "2")
			c.Assert(logs[i].GetTags().Contains("source"), check.Equals, false)
			c.Assert(logs[i].GetTags().Contains("tenant"), check.Equals, preserve)
		}
	}
}

func (s *processorTestSuite) TestNoKeyAlarmAndPreserve(c *check.C) {
	processor := s.processor.(*ProcessorSplit)
	processor.NoKeyError = true
	processor.SplitKey = "source"
	log := processortest.Log("source", "indexed\nvalue")

	processor.PreserveOthers = false
	c.Assert(len(processortest.Apply(processor, log)), check.Equals, 0)

	processor.PreserveOthers = true
	logs := processortest.Apply(processor, log)
	c.Assert(len(logs), check.Equals, 1)
	c.Assert(len(logs[0].GetBody()), check.Equals, 0)
	c.Assert(logs[0].Contents.Get("source"), check.Equals, "indexed\nvalue")
}

func TestProcessorSplitLogStringEmitsRelativeOffsets(t *testing.T) {
	processor := &ProcessorSplit{SplitSep: "你好"}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
	log := processortest.Log("content", "你好甲你好你好乙你好丙你好")
	log.SetOffset(1000)
	log.SetRawSize(123)
	log.SetObservedTimestamp(456)
	for _, enabled := range []bool{false, true} {
		processor.EnableLogPositionMeta = enabled
		logs := processortest.Apply(processor, log)
		require.Len(t, logs, 3)
		for i, want := range []uint64{0, 9, 18} {
			assert.Equal(t, want, logs[i].GetOffset())
			assert.Zero(t, logs[i].GetRawSize())
			assert.Zero(t, logs[i].GetObservedTimestamp())
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

func TestProcessorSplitLogStringPrefersBodyAndClonesOutputs(t *testing.T) {
	processor := &ProcessorSplit{SplitKey: "payload", SplitSep: "|", PreserveOthers: true}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := processortest.Log("leading", "keep", "content", "first|second", "payload", "indexed|value")
	log.GetIndices().Add("bytes", []byte("bytes"))
	log.GetTags().Add("payload", "event|tag")
	log.GetTags().Add("tenant", "example")
	log.Timestamp = 1700000000123456789
	logs := processortest.Apply(processor, log)
	require.Len(t, logs, 2)
	for i, want := range []string{"first", "second"} {
		assert.Equal(t, want, string(logs[i].GetBody()))
		assert.Equal(t, "indexed|value", logs[i].GetIndices().Get("payload"))
		assert.Equal(t, "event|tag", logs[i].GetTags().Get("payload"))
		assert.Equal(t, log.GetTimestamp(), logs[i].GetTimestamp())
	}

	logs[0].GetBody()[0] = 'F'
	logs[0].GetIndices().Get("bytes").([]byte)[0] = 'B'
	logs[0].GetIndices().Delete("leading")
	logs[0].GetTags().Delete("tenant")
	assert.Equal(t, "first|second", string(log.GetBody()))
	assert.Equal(t, "second", string(logs[1].GetBody()))
	for _, unchanged := range []*models.Log{log, logs[1]} {
		assert.Equal(t, "bytes", string(unchanged.GetIndices().Get("bytes").([]byte)))
		assert.Equal(t, "keep", unchanged.GetIndices().Get("leading"))
		assert.Equal(t, "example", unchanged.GetTags().Get("tenant"))
	}
}

func TestProcessorSplitLogStringUsesEventTagWhenBodyIsEmpty(t *testing.T) {
	processor := &ProcessorSplit{SplitKey: "payload", SplitSep: "|", PreserveOthers: true}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
	log := processortest.Log("content", "", "payload", "indexed|value", "other", "keep")
	log.GetTags().Add("payload", "first|second")
	log.GetTags().Add("tenant", "keep")

	logs := processortest.Apply(processor, log)
	require.Len(t, logs, 2)
	assert.Positive(t, logs[0].GetTimestamp())
	assert.Equal(t, logs[0].GetTimestamp(), logs[1].GetTimestamp())
	for i, want := range []string{"first", "second"} {
		assert.Equal(t, want, string(logs[i].GetBody()))
		assert.Equal(t, "indexed|value", logs[i].GetIndices().Get("payload"))
		assert.Equal(t, map[string]string{"tenant": "keep"}, logs[i].GetTags().Iterator())
	}
	assert.Empty(t, log.GetBody())
	assert.Equal(t, "first|second", log.GetTags().Get("payload"))
}

func TestProcessorSplitLogStringEmptySourcePreservesOtherFieldsAndTags(t *testing.T) {
	processor := &ProcessorSplit{SplitKey: "payload", SplitSep: "|", PreserveOthers: true}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
	log := processortest.Log("payload", "indexed|value", "other", "keep")
	log.GetTags().Add("payload", "")
	log.GetTags().Add("tenant", "keep")
	logs := processortest.Apply(processor, log)
	require.Len(t, logs, 1)
	assert.Empty(t, logs[0].GetBody())
	assert.Equal(t, "indexed|value", logs[0].GetIndices().Get("payload"))
	assert.Equal(t, map[string]string{"tenant": "keep"}, logs[0].GetTags().Iterator())
	assert.True(t, log.GetTags().Contains("payload"))

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
