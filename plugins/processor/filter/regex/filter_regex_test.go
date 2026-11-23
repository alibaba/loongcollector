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
	s.processor = pipeline.Processors["processor_filter_regex"]()
	_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
	logger.Info(context.Background(), "set up", s.processor.Description())
}

func (s *processorTestSuite) TearDownTest(c *check.C) {

}

func (s *processorTestSuite) TestInitError(c *check.C) {
	c.Assert(s.processor.Init(mock.NewEmptyContext("p", "l", "c")), check.IsNil)
	processor, _ := s.processor.(*ProcessorRegexFilter)
	processor.Exclude = map[string]string{"key": "("}
	c.Assert(s.processor.Init(mock.NewEmptyContext("p", "l", "c")), check.NotNil)
}

func (s *processorTestSuite) TestDefault(c *check.C) {
	{
		var log = "xxxx\nyyyy\nzzzz"
		logEvent := processortest.Log("content", log, "key1", "value1", "key2", "value2")
		processor, _ := s.processor.(*ProcessorRegexFilter)
		processor.Exclude = map[string]string{"key": ".*"}
		processor.Include = map[string]string{"key1": ".*", "content": "[\\s\\S]*"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 3)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, log)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key1")), check.Equals, "value1")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key2")), check.Equals, "value2")
	}
	{
		var log = `10.200.98.220 - - [26/Jun/2017:13:45:41 +0800] "POST /PutData?Category=YunOsAccountOpLog&AccessKeyId=U0UjpekFQOVJW45A&Date=Fri%2C%2028%20Jun%202013%2006%3A53%3A30%20GMT&Topic=raw&Signature=pD12XYLmGxKQ%2Bmkd6x7hAgQ7b1c%3D HTTP/1.1" 0.024 18204 200 37 "-" "aliyun-sdk-java" 215519025`
		logEvent := processortest.Log("content", log)
		processor, _ := s.processor.(*ProcessorRegexFilter)
		reg := `([\d\.]+) \S+ \S+ \[(\S+) \S+\] "(\w+) ([^\"]*)" ([\d\.]+) (\d+) (\d+) (\d+|-) "([^\"]*)" "([^\"]*)".* (\d+)`
		processor.Include = map[string]string{"content": reg}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, log)
	}

}

func (s *processorTestSuite) TestNotMatch(c *check.C) {

	{
		var log = `123abc10.200.98.220 - - [26/Jun/2017:13:45:41 +0800] "POST /PutData?Category=YunOsAccountOpLog&AccessKeyId=U0UjpekFQOVJW45A&Date=Fri%2C%2028%20Jun%202013%2006%3A53%3A30%20GMT&Topic=raw&Signature=pD12XYLmGxKQ%2Bmkd6x7hAgQ7b1c%3D HTTP/1.1" 0.024 18204 200 37 "-" "aliyun-sdk-java" 215519025xxxx`
		logEvent := processortest.Log("content", log)
		processor, _ := s.processor.(*ProcessorRegexFilter)
		reg := `^([\d\.]+) \S+ \S+ \[(\S+) \S+\] "(\w+) ([^\"]*)" ([\d\.]+) (\d+) (\d+) (\d+|-) "([^\"]*)" "([^\"]*)".* (\d+)`
		processor.Include = map[string]string{"content": reg}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 0)
	}
	{
		var log = "xxxx\nyyyy\nzzzz"
		logEvent := processortest.Log("content", log, "key1", "value1", "key2", "value2")
		processor, _ := s.processor.(*ProcessorRegexFilter)
		processor.Include = map[string]string{"key": ".*"}
		processor.Exclude = map[string]string{"key1": ".*", "content": ".*"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 0)
	}
	{
		var log = "xxxx\nyyyy\nzzzz"
		logEvent := processortest.Log("content", log, "key1", "value1", "key2", "value2")
		processor, _ := s.processor.(*ProcessorRegexFilter)
		processor.Include = map[string]string{"key1": ".*", "content": ".*"}
		processor.Exclude = map[string]string{"key2": ".*"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 0)
	}
}

func newEventLog(kv ...string) *models.Log {
	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	for i := 0; i+1 < len(kv); i += 2 {
		log.GetIndices().Add(kv[i], kv[i+1])
	}
	return log
}

// TestProcessorRegexFilter_ProcessFilter verifies that Process drops
// Log events failing the Include/Exclude rules while keeping matching ones and
// passing Metric events through unchanged.
func TestProcessorRegexFilter_ProcessFilter(t *testing.T) {
	processor := &ProcessorRegexFilter{
		Include: map[string]string{"key1": "value.*"},
		Exclude: map[string]string{"key2": "drop.*"},
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	keep := newEventLog("key1", "value1", "key2", "ok")            // matches include, not exclude -> keep
	dropInclude := newEventLog("key1", "nope", "key2", "ok")       // include mismatch -> drop
	dropExclude := newEventLog("key1", "value2", "key2", "dropme") // exclude match -> drop
	metric := models.NewSingleValueMetric("m", models.MetricTypeCounter, models.NewTags(), 0, 1.0)

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{
		Events: []models.PipelineEvent{keep, dropInclude, metric, dropExclude},
	}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	events := results[0].Events
	require.Len(t, events, 2, "only the matching log and the metric survive")
	assert.Equal(t, keep, events[0])
	assert.Equal(t, metric, events[1], "metric events must pass through unchanged")
}

// TestProcessorRegexFilter_ProcessAllDropped verifies zero surviving log
// events results in no collected group.
func TestProcessorRegexFilter_ProcessAllDropped(t *testing.T) {
	processor := &ProcessorRegexFilter{
		Include: map[string]string{"key1": "value.*"},
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{
		Events: []models.PipelineEvent{newEventLog("key1", "nope")},
	}, context)

	require.Len(t, context.Collector().ToArray(), 0)
}
