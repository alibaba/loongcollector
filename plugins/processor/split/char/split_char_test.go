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

package char

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
	s.processor = pipeline.Processors["processor_split_char"]()
	require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
	logger.Info(context.Background(), "set up", s.processor.Description())
}

func (s *processorTestSuite) TearDownTest(c *check.C) {
}

func (s *processorTestSuite) TestInitError(c *check.C) {
	processor, _ := s.processor.(*ProcessorSplitChar)
	processor.SplitSep = ""
	c.Assert(s.processor.Init(mock.NewEmptyContext("p", "l", "c")), check.NotNil)
}

func (s *processorTestSuite) TestDefault(c *check.C) {
	{
		var log = "xxxx\nyyyy\nzzzz"
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("_split_preserve_")), check.Equals, log)
	}
	{
		var log = "xxxx\nyyyy\nzzzz\n"
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("_split_preserve_")), check.Equals, log)
	}
	{
		var log = "\nxxxx\nyyyy\nzzzz\n"
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("_split_preserve_")), check.Equals, log)
	}
}

func (s *processorTestSuite) TestNormal(c *check.C) {
	{
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = false
		processor.KeepSource = true
		processor.NoKeyError = true
		processor.SplitSep = "]"
		processor.SourceKey = "content"
		processor.SplitKeys = []string{"key1", "key2"}
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

		var log = "你好[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t你好[你好"
		logEvent := processortest.Log("preserve_1", "1", "content", log, "preserve_2", "2")
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 5)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("preserve_1")), check.Equals, "1")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, log)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("preserve_2")), check.Equals, "2")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key1")), check.Equals, "你好[2017-12-12 00:00:00")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key2")), check.Equals, " 你好\n  hello\n\t\n[2017xxxxxx")
	}

	{
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = true
		processor.KeepSource = false
		processor.NoKeyError = false
		processor.SplitSep = "["
		processor.SourceKey = "content"
		processor.SplitKeys = []string{"key1", "key2"}
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t你好[你好"
		logEvent := processortest.Log("preserve_1", "1", "content", log, "preserve_2", "2")
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 5)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("preserve_1")), check.Equals, "1")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("preserve_2")), check.Equals, "2")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key1")), check.Equals, "")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key2")), check.Equals, "2017-12-12 00:00:00] 你好\n  hello\n\t\n")

		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("_split_preserve_")), check.Equals, "2017xxxxxx]yyyy\nzzzz\n\t你好[你好")
	}

	{
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = true
		processor.KeepSource = false
		processor.NoKeyError = false
		processor.SplitSep = "["
		processor.SourceKey = "content"
		processor.SplitKeys = []string{"key1", "key2"}
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t你好[你好"
		logEvent := processortest.Log("preserve_1", "1", "content", log, "preserve_2", "2")
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 5)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("preserve_1")), check.Equals, "1")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("preserve_2")), check.Equals, "2")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key1")), check.Equals, "")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key2")), check.Equals, "2017-12-12 00:00:00] 你好\n  hello\n\t\n")

		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("_split_preserve_")), check.Equals, "2017xxxxxx]yyyy\nzzzz\n\t你好[你好")
	}
}

func (s *processorTestSuite) TestQuote(c *check.C) {
	{
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = false
		processor.NoKeyError = true
		processor.SplitSep = ","
		processor.SourceKey = "content"
		processor.QuoteFlag = true
		processor.Quote = "\""
		processor.SplitKeys = []string{"key1", "key2", "key3", "key4", "key5"}
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

		var log = `1999,Chevy,"Venture ""Extended Edition, Very Large""","",5000.00`
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 5)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key1")), check.Equals, "1999")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key2")), check.Equals, "Chevy")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key3")), check.Equals, `Venture "Extended Edition, Very Large"`)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key4")), check.Equals, "")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key5")), check.Equals, "5000.00")
	}
	{
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = false
		processor.NoKeyError = true
		processor.SplitSep = ","
		processor.SourceKey = "content"
		processor.QuoteFlag = true
		processor.Quote = "\""
		processor.SplitKeys = []string{"key1", "key2", "key3", "key4", "key5"}
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

		var log = `1999,Chevy,"Venture ""Extended Edition, Very Large""","","5000.00"""`
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 5)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key1")), check.Equals, "1999")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key2")), check.Equals, "Chevy")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key3")), check.Equals, `Venture "Extended Edition, Very Large"`)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key4")), check.Equals, "")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key5")), check.Equals, "5000.00\"")
	}
	{
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = true
		processor.NoKeyError = true
		processor.SplitSep = ","
		processor.SourceKey = "content"
		processor.QuoteFlag = true
		processor.Quote = "\""
		processor.SplitKeys = []string{"key1", "key2", "key3", "key4", "key5"}
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

		var log = `1999,Chevy,"Venture ""Extended Edition, Very Large""","","5000.00""", ,,,`
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 6)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key1")), check.Equals, "1999")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key2")), check.Equals, "Chevy")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key3")), check.Equals, `Venture "Extended Edition, Very Large"`)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key4")), check.Equals, "")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key5")), check.Equals, "5000.00\"")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("_split_preserve_")), check.Equals, " ,,,")
	}
	{
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = true
		processor.NoKeyError = true
		processor.SplitSep = ","
		processor.SourceKey = "content"
		processor.QuoteFlag = true
		processor.Quote = "\""
		processor.SplitKeys = []string{"key1", "key2", "key3", "key4", "key5"}
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

		var log = `,Chevy,"Venture ""Extended Edition, Very Large""",,"5000.00""", `
		logEvent := processortest.Log("content", log)
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 6)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key1")), check.Equals, "")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key2")), check.Equals, "Chevy")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key3")), check.Equals, `Venture "Extended Edition, Very Large"`)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key4")), check.Equals, "")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key5")), check.Equals, "5000.00\"")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("_split_preserve_")), check.Equals, " ")
	}
}

func (s *processorTestSuite) TestNoKeyAlarmAndPreserve(c *check.C) {
	{
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = false
		processor.NoKeyError = true
		processor.SplitSep = "["
		processor.SourceKey = "no_this_key"
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t["
		logEvent := processortest.Log("preserve_1", "1", "content", log, "preserve_2", "2")
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 3)
	}

	{
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = true
		processor.NoKeyError = true
		processor.KeepSource = false
		processor.SplitSep = "["
		processor.SourceKey = "no_this_key"
		processor.SplitKeys = []string{"key_1", "key_2"}
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t["
		logEvent := processortest.Log("preserve_1", "1", "content", log, "preserve_2", "2")
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 3)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, log)
	}
}

func (s *processorTestSuite) TestKeepSourceIfParseError(c *check.C) {
	processor, _ := s.processor.(*ProcessorSplitChar)
	processor.PreserveOthers = true
	processor.KeepSource = false
	processor.KeepSourceIfParseError = true
	processor.NoKeyError = false
	processor.SplitSep = "["
	processor.SourceKey = "content"
	processor.QuoteFlag = true
	processor.Quote = "\""
	processor.SplitKeys = []string{"key1", "key2", "key3", "key4", "key5"}
	require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

	var log = "2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t你好[你好[\"[hello\"[\"\"invalid\""
	logEvent := processortest.Log("preserve_1", "1", "content", log, "preserve_2", "2")
	outLogs := processortest.Apply(s.processor, logEvent)

	require.Equal(c, 1, len(outLogs))
	require.Equal(c, 7, outLogs[0].Contents.Len())
	require.Equal(c, "1", pipeline.GetStringValue(outLogs[0].Contents.Get("preserve_1")))

	// Encounter parsing error, source is kept.
	require.Equal(c, log, pipeline.GetStringValue(outLogs[0].Contents.Get("content")))
	require.Equal(c, "2", pipeline.GetStringValue(outLogs[0].Contents.Get("preserve_2")))

	// Partial keys are extracted.
	require.Equal(c, "2017-12-12 00:00:00] 你好\n  hello\n\t\n", pipeline.GetStringValue(outLogs[0].Contents.Get("key1")))
	require.Equal(c, "2017xxxxxx]yyyy\nzzzz\n\t你好", pipeline.GetStringValue(outLogs[0].Contents.Get("key2")))
	require.Equal(c, "你好", pipeline.GetStringValue(outLogs[0].Contents.Get("key3")))
	require.Equal(c, "[hello", pipeline.GetStringValue(outLogs[0].Contents.Get("key4")))
}

func TestProcessorSplitChar_ProcessSplitsIntoKeys(t *testing.T) {
	processor := &ProcessorSplitChar{
		SplitSep:  ",",
		SplitKeys: []string{"k1", "k2", "k3"},
		SourceKey: "src",
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("src", "a,b,c")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.Equal(t, "a", contents.Get("k1"))
	assert.Equal(t, "b", contents.Get("k2"))
	assert.Equal(t, "c", contents.Get("k3"))
	// KeepSource defaults to false, so the source key is removed.
	assert.False(t, contents.Contains("src"), "source key must be removed by default")
}

func TestProcessorSplitChar_ProcessKeepSource(t *testing.T) {
	processor := &ProcessorSplitChar{
		SplitSep:   ",",
		SplitKeys:  []string{"k1", "k2"},
		SourceKey:  "src",
		KeepSource: true,
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("src", "a,b")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.Equal(t, "a", contents.Get("k1"))
	assert.Equal(t, "b", contents.Get("k2"))
	assert.True(t, contents.Contains("src"), "source key must be kept when KeepSource is set")
}

func TestProcessorSplitChar_ProcessMissingSourceKey(t *testing.T) {
	processor := &ProcessorSplitChar{
		SplitSep:  ",",
		SplitKeys: []string{"k1", "k2"},
		SourceKey: "src",
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("other", "a,b")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.False(t, contents.Contains("k1"), "missing source key must not produce split keys")
	assert.Equal(t, "a,b", contents.Get("other"), "unrelated key must be untouched")
}

func TestProcessorSplitChar_ProcessPassesThroughMetric(t *testing.T) {
	processor := &ProcessorSplitChar{
		SplitSep:  ",",
		SplitKeys: []string{"k1"},
		SourceKey: "src",
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}
