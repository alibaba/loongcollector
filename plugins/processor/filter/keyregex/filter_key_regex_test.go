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

package keyregex

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
	s.processor = pipeline.Processors["processor_filter_key_regex"]()
	_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
	logger.Info(context.Background(), "set up", s.processor.Description())
}

func (s *processorTestSuite) TearDownTest(c *check.C) {

}

func (s *processorTestSuite) TestInitError(c *check.C) {
	c.Assert(s.processor.Init(mock.NewEmptyContext("p", "l", "c")), check.IsNil)
	processor, _ := s.processor.(*ProcessorKeyFilter)
	processor.Exclude = []string{"key", "("}
	c.Assert(s.processor.Init(mock.NewEmptyContext("p", "l", "c")), check.NotNil)
}

func (s *processorTestSuite) TestMatch(c *check.C) {
	{
		var log = "xxxx\nyyyy\nzzzz"
		logEvent := processortest.Log("content", log, "key1", "value1", "key2", "value2")
		processor, _ := s.processor.(*ProcessorKeyFilter)
		processor.Exclude = []string{"keyd"}
		processor.Include = []string{"content", "key1", "key2"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		outLogs := processortest.Apply(processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 3)

		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, log)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key1")), check.Equals, "value1")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key2")), check.Equals, "value2")
	}
	{
		logEvent := processortest.Log(`10.200.98.220`, "hello ilogtail")
		processor, _ := s.processor.(*ProcessorKeyFilter)
		reg := `([\d\.]+)`
		processor.Include = []string{reg}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		outLogs := processortest.Apply(processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get(`10.200.98.220`)), check.Equals, "hello ilogtail")
	}
	{
		logEvent := processortest.Log("request_time", "20", "request_length", "2314", "browser", "aliyun-sdk-java")
		processor, _ := s.processor.(*ProcessorKeyFilter)
		processor.Include = []string{"request.*"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		outLogs := processortest.Apply(processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 3)

		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("request_time")), check.Equals, "20")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("request_length")), check.Equals, "2314")
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("browser")), check.Equals, "aliyun-sdk-java")
	}
}

func (s *processorTestSuite) TestNotMatch(c *check.C) {

	{
		logEvent := processortest.Log(`abc10.200.98.220`, "hello ilogtail")
		processor, _ := s.processor.(*ProcessorKeyFilter)
		reg := `^([\d\.]+)`
		processor.Include = []string{reg}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		outLogs := processortest.Apply(processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 0)
	}
	{
		logEvent := processortest.Log("browser", "ali-sls-ilogtail", "status", "200")
		processor, _ := s.processor.(*ProcessorKeyFilter)
		processor.Include = []string{"request.*"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		outLogs := processortest.Apply(processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 0)
	}
	{
		logEvent := processortest.Log("content", "log", "key1", "value1", "key2", "value2")
		processor, _ := s.processor.(*ProcessorKeyFilter)
		processor.Exclude = []string{"key1"} // exclude has higher priority
		processor.Include = []string{"key1", "key2"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		outLogs := processortest.Apply(processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 0)
	}
}

func newEventLog(keys ...string) *models.Log {
	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	for _, key := range keys {
		log.GetIndices().Add(key, "v")
	}
	return log
}

// TestProcessorKeyFilter_ProcessFilter verifies that Process drops Log
// events whose keys fail the Include/Exclude rules while keeping matching ones
// and passing Metric events through unchanged.
func TestProcessorKeyFilter_ProcessFilter(t *testing.T) {
	processor := &ProcessorKeyFilter{
		Include: []string{"key1", "key2"},
		Exclude: []string{"secret"},
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	keep := newEventLog("key1", "key2")                  // has all required keys, no excluded key
	dropInclude := newEventLog("key1")                   // missing key2 -> drop
	dropExclude := newEventLog("key1", "key2", "secret") // excluded key present -> drop
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

// TestProcessorKeyFilter_ProcessAllDropped verifies zero surviving log events
// results in no collected group.
func TestProcessorKeyFilter_ProcessAllDropped(t *testing.T) {
	processor := &ProcessorKeyFilter{
		Include: []string{"required"},
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{
		Events: []models.PipelineEvent{newEventLog("other")},
	}, context)

	require.Len(t, context.Collector().ToArray(), 0)
}
