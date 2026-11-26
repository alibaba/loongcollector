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

package logregex

import (
	"context"
	"testing"

	"github.com/alibaba/ilogtail/plugins/processor/internal/processortest"

	"github.com/pingcap/check"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

var _ = check.Suite(&processorTestSuite{})

func Test(t *testing.T) {
	logger.InitTestLogger()
	check.TestingT(t)
}

type processorTestSuite struct {
	processor pipeline.Processor
}

func (s *processorTestSuite) SetUpTest(c *check.C) {
	s.processor = pipeline.Processors["processor_split_log_regex"]()
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
		c.Assert(len(outLogs), check.Equals, 4)
	}
}

func (s *processorTestSuite) TestMultiLine(c *check.C) {
	processor, _ := s.processor.(*ProcessorSplitRegex)
	processor.SplitRegex = "\\[.*"
	_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

	{
		var log = "[2017-12-12 00:00:00] 你好\nhello\n\n[2017xxxxxx]yyyy\n [zzzz\n["
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 3)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, "[2017-12-12 00:00:00] 你好\nhello\n")
		c.Assert(pipeline.GetStringValue(outLogs[1].Contents.Get("content")), check.Equals, "[2017xxxxxx]yyyy\n [zzzz")
		c.Assert(pipeline.GetStringValue(outLogs[2].Contents.Get("content")), check.Equals, "[")
	}

	// Case: only one matched line.
	{
		var log = "[2017-12-12 00:00:00] xxxxxx"
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, log)
	}

	// Case: only one matched line ends with line feed.
	{
		var log = "[2017-12-12 00:00:00] xxxxxx\n"
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, log)
	}

	// Case: one unmatched line.
	{
		var log = "xxxxxx"
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, log)
	}

	// Case: one unmatched line ends with line feed.
	{
		var log = "xxxxxx\n"
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, log)
	}

	// Case: multiple unmatched lines.
	{
		var log = "xxxxxx\nyyyyy\nzzzzzz"
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, log)
	}

	// Case: multiple unmatched lines end with line feed.
	{
		var log = "xxxxxx\nyyyyy\nzzzzzz\n"
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, log)
	}
}

func (s *processorTestSuite) TestPreserve(c *check.C) {
	{
		processor, _ := s.processor.(*ProcessorSplitRegex)
		processor.PreserveOthers = true
		processor.SplitRegex = "\\S+.*"
		processor.SplitKey = "content"
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t["
		logEvent := processortest.Log("preserve_1", "1", "content", log, "preserve_2", "2")
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 3)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 3)
		c.Assert(outLogs[1].Contents.Len(), check.Equals, 3)
		c.Assert(outLogs[2].Contents.Len(), check.Equals, 3)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, "[2017-12-12 00:00:00] 你好\n  hello\n\t")
		c.Assert(pipeline.GetStringValue(outLogs[1].Contents.Get("content")), check.Equals, "[2017xxxxxx]yyyy")
		c.Assert(pipeline.GetStringValue(outLogs[2].Contents.Get("content")), check.Equals, "zzzz\n\t[")
		c.Assert(pipeline.GetStringValue(outLogs[1].Contents.Get("preserve_1")), check.Equals, "1")
		c.Assert(pipeline.GetStringValue(outLogs[1].Contents.Get("preserve_2")), check.Equals, "2")
	}

	{
		processor, _ := s.processor.(*ProcessorSplitRegex)
		processor.PreserveOthers = false
		processor.SplitRegex = "\\S+.*"
		processor.SplitKey = "content"
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t["
		logEvent := processortest.Log("preserve_1", "1", "content", log, "preserve_2", "2")
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 3)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 1)
		c.Assert(outLogs[1].Contents.Len(), check.Equals, 1)
		c.Assert(outLogs[2].Contents.Len(), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, "[2017-12-12 00:00:00] 你好\n  hello\n\t")
		c.Assert(pipeline.GetStringValue(outLogs[1].Contents.Get("content")), check.Equals, "[2017xxxxxx]yyyy")
		c.Assert(pipeline.GetStringValue(outLogs[2].Contents.Get("content")), check.Equals, "zzzz\n\t[")
	}
}

func (s *processorTestSuite) TestNoKeyAlarm(c *check.C) {
	{
		processor, _ := s.processor.(*ProcessorSplitRegex)
		processor.PreserveOthers = false
		processor.NoKeyError = true
		processor.SplitRegex = "\\S+.*"
		processor.SplitKey = "no_this_key"
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t["
		logEvent := processortest.Log("preserve_1", "1", "content", log, "preserve_2", "2")
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 0)
	}

	{
		processor, _ := s.processor.(*ProcessorSplitRegex)
		processor.PreserveOthers = true
		processor.NoKeyError = true
		processor.SplitRegex = "\\S+.*"
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

	processor, _ := s.processor.(*ProcessorSplitRegex)
	processor.EnableLogPositionMeta = true
	processor.PreserveOthers = true
	processor.SplitRegex = "\\[.*"
	_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

	{
		{
			var log = "[2017-12-12 00:00:00] 你好\nhello\n\n[2017-12-12 00:00:00] yyyy\n[2017-12-12 00:00:00] 123"
			logEvent := processortest.Log("content", log)
			logEvent.SetOffset(1000)
			outLogs := processortest.Apply(s.processor, logEvent)
			c.Assert(len(outLogs), check.Equals, 3)
			s.assertLogPosition(c, outLogs[0], 1000)
			s.assertLogPosition(c, outLogs[1], 1036)
			s.assertLogPosition(c, outLogs[2], 1063)
		}
	}

	{
		{
			var log = "[2017-12-12 00:00:00] 你好\nhello\n\n [2017-12-12 00:00:00] yyyy\n[2017-12-12 00:00:00] 123"
			logEvent := processortest.Log("content", log)
			logEvent.SetOffset(1000)
			outLogs := processortest.Apply(s.processor, logEvent)
			c.Assert(len(outLogs), check.Equals, 2)
			s.assertLogPosition(c, outLogs[0], 1000)
			s.assertLogPosition(c, outLogs[1], 1064)
		}
	}
}

func (s *processorTestSuite) assertLogPosition(c *check.C, log *models.Log, offset uint64) {
	c.Assert(log.GetOffset(), check.Equals, offset)
}

// newSplitLogRegex builds the processor via its registered factory and
// applies the given config.
func newSplitLogRegex(t *testing.T, splitRegex, splitKey string, preserveOthers bool) *ProcessorSplitRegex {
	processor := pipeline.Processors["processor_split_log_regex"]().(*ProcessorSplitRegex)
	processor.SplitRegex = splitRegex
	processor.SplitKey = splitKey
	processor.PreserveOthers = preserveOthers
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
	return processor
}

func newLogWithContent(key, value string) *models.Log {
	contents := models.NewLogContents()
	contents.Add(key, value)
	return &models.Log{Contents: contents}
}

// TestProcessorSplitLogRegex_ProcessSplitsLog asserts that a single Log whose
// SplitKey holds a multiline value is split into the expected number of Logs
// with the same segment values as TestMultiLine.
func TestProcessorSplitLogRegex_ProcessSplitsLog(t *testing.T) {
	processor := newSplitLogRegex(t, "\\[.*", "content", false)

	raw := "[2017-12-12 00:00:00] 你好\nhello\n\n[2017xxxxxx]yyyy\n [zzzz\n["
	log := newLogWithContent("content", raw)

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)

	values := make([]string, 0, len(results[0].Events))
	for _, event := range results[0].Events {
		logEvent, ok := event.(*models.Log)
		require.True(t, ok, "split result must be Log events")
		v, ok := logEvent.GetIndices().Get("content").(string)
		require.True(t, ok)
		values = append(values, v)
	}

	assert.Equal(t, []string{
		"[2017-12-12 00:00:00] 你好\nhello\n",
		"[2017xxxxxx]yyyy\n [zzzz",
		"[",
	}, values)
}

// TestProcessorSplitLogRegex_ProcessPassesThroughMetric asserts that the 1->N
// split still emits every produced Log while a Metric event in the same group
// passes through unchanged (never dropped).
func TestProcessorSplitLogRegex_ProcessPassesThroughMetric(t *testing.T) {
	processor := newSplitLogRegex(t, "\\[.*", "content", false)

	raw := "[2017-12-12 00:00:00] 你好\nhello\n\n[2017xxxxxx]yyyy\n [zzzz\n["
	log := newLogWithContent("content", raw)
	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log, metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)

	var logs, metrics int
	var passedMetric *models.Metric
	for _, event := range results[0].Events {
		switch e := event.(type) {
		case *models.Log:
			logs++
		case *models.Metric:
			metrics++
			passedMetric = e
		}
	}

	assert.Equal(t, 3, logs, "input log must be split into 3 logs")
	assert.Equal(t, 1, metrics, "metric event must pass through unchanged")
	require.NotNil(t, passedMetric)
	assert.Equal(t, "m", passedMetric.GetName())
}
