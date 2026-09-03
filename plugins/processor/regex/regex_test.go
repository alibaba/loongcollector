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

package regex

import (
	"context"
	"testing"

	"github.com/pingcap/check"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

var _ = check.Suite(&processorTestSuite{})

func Test(t *testing.T) {
	logger.InitTestLogger()
	check.TestingT(t)
}

type processorTestSuite struct {
	processor pipeline.ProcessorV1
}

func (s *processorTestSuite) SetUpTest(c *check.C) {
	s.processor = pipeline.Processors["processor_regex"]().(pipeline.ProcessorV1)
	require.Error(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
	logger.Info(context.Background(), "set up", s.processor.Description())
}

func (s *processorTestSuite) TearDownTest(c *check.C) {

}

func (s *processorTestSuite) TestInitError(c *check.C) {
	processor, _ := s.processor.(*ProcessorRegex)
	c.Assert(s.processor.Init(mock.NewEmptyContext("p", "l", "c")), check.NotNil)
	processor.Keys = []string{"key"}
	processor.Regex = "("
	c.Assert(s.processor.Init(mock.NewEmptyContext("p", "l", "c")), check.NotNil)
}

func (s *processorTestSuite) TestDefault(c *check.C) {
	// Case: catch all content as a field.
	{
		log := `2021-08-27 13:04:14.920 77711773 [ThreadName] INFO  content detail`
		logPb := test.CreateLogs("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"content", "time", "level"}
		processor.Regex = "((\\d{4}[-]\\d{2}[-]\\d{2}\\s\\d{2}[:]\\d{2}[:]\\d{2}[.]\\d{3})[\\S\\s]*(INFO|WARN|ERROR|DEBUG)[\\s\\S]*)"
		processor.KeepSource = false
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb

		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 3)
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, log)
		c.Assert(outLogs[0].Contents[1].GetValue(), check.Equals, "2021-08-27 13:04:14.920")
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, "INFO")
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[1].GetKey(), check.Equals, "time")
		c.Assert(outLogs[0].Contents[2].GetKey(), check.Equals, "level")
	}
	// Case: end with line feed, must use [\s\S]* if line feed needs to be included.
	{
		log := "[2021-08-27 16:37:09][ONLINE_ROLE],json\nnextline"
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"jsonlog"}

		{
			processor.Regex = "^[^,]+,(.*)$"
			require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
			logArray := make([]*protocol.Log, 1)
			logArray[0] = test.CreateLogs("content", log)
			outLogs := s.processor.ProcessLogs(logArray)
			c.Assert(len(outLogs), check.Equals, 1)
			c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "json\nnextline")
		}
		{
			processor.Regex = "^[^,]+,([\\s\\S]*)$"
			require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
			logArray := make([]*protocol.Log, 1)
			logArray[0] = test.CreateLogs("content", log)
			outLogs := s.processor.ProcessLogs(logArray)
			c.Assert(len(outLogs), check.Equals, 1)
			c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "json\nnextline")
		}
		{
			// Does not include line feed.
			processor.Regex = "^[^,]+,(.*)"
			require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
			logArray := make([]*protocol.Log, 1)
			logArray[0] = test.CreateLogs("content", log)
			outLogs := s.processor.ProcessLogs(logArray)
			c.Assert(len(outLogs), check.Equals, 1)
			c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "json\nnextline")
		}
	}
	{
		var log = "xxxx\nyyyy\nzzzz"
		logPb := test.CreateLogs("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"key1", "key2", "key3"}
		processor.Regex = `(\w+)\s(\w+)\s(\w+)`
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 3)
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "xxxx")
		c.Assert(outLogs[0].Contents[1].GetValue(), check.Equals, "yyyy")
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, "zzzz")

		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "key1")
		c.Assert(outLogs[0].Contents[1].GetKey(), check.Equals, "key2")
		c.Assert(outLogs[0].Contents[2].GetKey(), check.Equals, "key3")
	}
	{
		var log = `10.200.98.220 - - [26/Jun/2017:13:45:41 +0800] "POST /PutData?Category=YunOsAccountOpLog&AccessKeyId=U0UjpekFQOVJW45A&Date=Fri%2C%2028%20Jun%202013%2006%3A53%3A30%20GMT&Topic=raw&Signature=pD12XYLmGxKQ%2Bmkd6x7hAgQ7b1c%3D HTTP/1.1" 0.024 18204 200 37 "-" "aliyun-sdk-java" 215519025`
		logPb := test.CreateLogs("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"ip", "time", "method", "url", "request_time", "length", "status", "length", "url", "browser", "seqNo"}
		values := []string{"10.200.98.220", "26/Jun/2017:13:45:41", "POST", "/PutData?Category=YunOsAccountOpLog&AccessKeyId=U0UjpekFQOVJW45A&Date=Fri%2C%2028%20Jun%202013%2006%3A53%3A30%20GMT&Topic=raw&Signature=pD12XYLmGxKQ%2Bmkd6x7hAgQ7b1c%3D HTTP/1.1", "0.024", "18204", "200", "37", "-", "aliyun-sdk-java", "215519025"}
		processor.Regex = `([\d\.]+) \S+ \S+ \[(\S+) \S+\] "(\w+) ([^\"]*)" ([\d\.]+) (\d+) (\d+) (\d+|-) "([^\"]*)" "([^\"]*)".* (\d+)`
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, len(values))
		for i, value := range values {
			c.Assert(outLogs[0].Contents[i].GetKey(), check.Equals, processor.Keys[i])
			c.Assert(outLogs[0].Contents[i].GetValue(), check.Equals, value)
		}
	}
	{
		var log = `123dsafsfdf10.200.98.220 - - [26/Jun/2017:13:45:41 +0800] "POST /PutData?Category=YunOsAccountOpLog&AccessKeyId=U0UjpekFQOVJW45A&Date=Fri%2C%2028%20Jun%202013%2006%3A53%3A30%20GMT&Topic=raw&Signature=pD12XYLmGxKQ%2Bmkd6x7hAgQ7b1c%3D HTTP/1.1" 0.024 18204 200 37 "-" "aliyun-sdk-java" 215519025AAAA`
		logPb := test.CreateLogs("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.FullMatch = false
		processor.Keys = []string{"ip", "time", "method", "url", "request_time", "length", "status", "length", "url", "browser", "seqNo"}
		values := []string{"10.200.98.220", "26/Jun/2017:13:45:41", "POST", "/PutData?Category=YunOsAccountOpLog&AccessKeyId=U0UjpekFQOVJW45A&Date=Fri%2C%2028%20Jun%202013%2006%3A53%3A30%20GMT&Topic=raw&Signature=pD12XYLmGxKQ%2Bmkd6x7hAgQ7b1c%3D HTTP/1.1", "0.024", "18204", "200", "37", "-", "aliyun-sdk-java", "215519025"}
		processor.Regex = `([\d\.]+) \S+ \S+ \[(\S+) \S+\] "(\w+) ([^\"]*)" ([\d\.]+) (\d+) (\d+) (\d+|-) "([^\"]*)" "([^\"]*)".* (\d+)`
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, len(values))
		for i, value := range values {
			c.Assert(outLogs[0].Contents[i].GetKey(), check.Equals, processor.Keys[i])
			c.Assert(outLogs[0].Contents[i].GetValue(), check.Equals, value)
		}
	}
	{
		// not match
		var log = "xxxx\nyyyy\nzzzz"
		logPb := test.CreateLogs("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"key1", "key2", "key3"}
		processor.Regex = `(\w+)\s(\w+)\s(\w+)`
		processor.SourceKey = "content"
		processor.KeepSource = true
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 4)
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, log)
		c.Assert(outLogs[0].Contents[1].GetValue(), check.Equals, "xxxx")
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, "yyyy")
		c.Assert(outLogs[0].Contents[3].GetValue(), check.Equals, "zzzz")

		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[1].GetKey(), check.Equals, "key1")
		c.Assert(outLogs[0].Contents[2].GetKey(), check.Equals, "key2")
		c.Assert(outLogs[0].Contents[3].GetKey(), check.Equals, "key3")
	}
}

func (s *processorTestSuite) TestNotMatch(c *check.C) {
	{
		// not match key
		var log = "xxxx\nyyyy\nzzzz"
		logPb := test.CreateLogs("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"key1", "key2", "key3", "key4"}
		processor.Regex = `(\w+)\s(\w+)\s(\w+)`
		processor.SourceKey = "content"
		processor.KeepSource = true
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 1)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, log)
	}
	{
		// not match key
		var log = "xxxx\nyyyy\nzzzz"
		logPb := test.CreateLogs("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"key1", "key2", "key3", "keyx"}
		processor.Regex = `(\w+)\s(\w+)\s(\w+)`
		processor.SourceKey = "content"
		processor.KeepSource = true
		processor.NoMatchError = true
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 1)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, log)
	}
}

func (s *processorTestSuite) TestNoKeyAlarmAndPreserve(c *check.C) {
	{
		// no key
		var log = "xxxx\nyyyy\nzzzz"
		logPb := test.CreateLogs("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"key1", "key2", "key3"}
		processor.Regex = `(\w+)\s(\w+)\s(\w+)`
		processor.SourceKey = "xxxx"
		processor.NoMatchError = true
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 1)
	}
	{
		var log = "xxxx\nyyyy\nzzzz"
		logPb := test.CreateLogs("content", log)
		processor, _ := s.processor.(*ProcessorRegex)
		processor.Keys = []string{"key1", "key2", "key3"}
		processor.Regex = `(\d+)\s(\w+)\s(\w+)`
		processor.SourceKey = "content"
		processor.KeepSource = true
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 1)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, log)
	}
}

func (s *processorTestSuite) TestKeepSourceIfParseError(c *check.C) {
	const sourceKeyName = "content"
	const sourceValue = "xxxx\nyyyy\nzzzz"

	// not match key
	processor, _ := s.processor.(*ProcessorRegex)
	processor.Keys = []string{"key1", "key2", "key3", "key4"}
	processor.Regex = `(\w+)\s(\w+)\s(\w+)`
	processor.SourceKey = sourceKeyName
	processor.KeepSource = false
	require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

	processor.KeepSourceIfParseError = false
	{
		logArray := make([]*protocol.Log, 1)
		logArray[0] = test.CreateLogs(sourceKeyName, sourceValue)
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 0)
	}

	processor.KeepSourceIfParseError = true
	{
		logArray := make([]*protocol.Log, 1)
		logArray[0] = test.CreateLogs(sourceKeyName, sourceValue)
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 1)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, sourceKeyName)
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, sourceValue)
	}
}

// ---- v2 (PipelineEvent / SendPb) Process path tests ----

// TestProcessorRegex_ProcessV2Parse verifies the v2 Process path extracts the
// configured capture groups from the SourceKey value into new keys.
func TestProcessorRegex_ProcessV2Parse(t *testing.T) {
	processor := &ProcessorRegex{
		Regex:      `(\w+)\s(\w+)\s(\w+)`,
		Keys:       []string{"key1", "key2", "key3"},
		SourceKey:  "content",
		KeepSource: true,
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "xxxx yyyy zzzz")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.Equal(t, "xxxx", contents.Get("key1"))
	assert.Equal(t, "yyyy", contents.Get("key2"))
	assert.Equal(t, "zzzz", contents.Get("key3"))
	// KeepSource=true keeps the original field.
	assert.Equal(t, "xxxx yyyy zzzz", contents.Get("content"))
}

// TestProcessorRegex_ProcessV2KeepSourceFalse verifies the source field is
// dropped on a successful parse when KeepSource is false.
func TestProcessorRegex_ProcessV2KeepSourceFalse(t *testing.T) {
	processor := &ProcessorRegex{
		Regex:      `(\w+)\s(\w+)\s(\w+)`,
		Keys:       []string{"key1", "key2", "key3"},
		SourceKey:  "content",
		KeepSource: false,
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "xxxx yyyy zzzz")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.False(t, contents.Contains("content"), "source key must be removed when KeepSource is false")
	assert.Equal(t, "xxxx", contents.Get("key1"))
	assert.Equal(t, "zzzz", contents.Get("key3"))
}

// TestProcessorRegex_ProcessV2NoMatchKeepsSource verifies that on a parse
// failure the source is kept (KeepSourceIfParseError) and no keys are added.
func TestProcessorRegex_ProcessV2NoMatchKeepsSource(t *testing.T) {
	processor := &ProcessorRegex{
		Regex:                  `(\d+)\s(\w+)\s(\w+)`,
		Keys:                   []string{"key1", "key2", "key3"},
		SourceKey:              "content",
		KeepSource:             false,
		KeepSourceIfParseError: true,
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "xxxx yyyy zzzz")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.True(t, contents.Contains("content"), "source kept on parse error (KeepSourceIfParseError)")
	assert.False(t, contents.Contains("key1"), "no keys extracted on no-match")
}

// TestProcessorRegex_ProcessV2PassesThroughMetric verifies a Metric event is
// emitted unchanged (not dropped) by the log-only processor.
func TestProcessorRegex_ProcessV2PassesThroughMetric(t *testing.T) {
	processor := &ProcessorRegex{
		Regex:     `(\w+)`,
		Keys:      []string{"key1"},
		SourceKey: "content",
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
