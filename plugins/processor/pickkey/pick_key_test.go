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

package pickkey

import (
	"context"
	"testing"

	"github.com/alibaba/ilogtail/plugins/processor/internal/processortest"

	"github.com/pingcap/check"
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
	s.processor = pipeline.Processors["processor_pick_key"]()
	_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
	logger.Info(context.Background(), "set up", s.processor.Description())
}

func (s *processorTestSuite) TearDownTest(c *check.C) {

}

func (s *processorTestSuite) TestDefault(c *check.C) {
	{
		var log = "xxxx\nyyyy\nzzzz"
		logEvent := processortest.Log("content", log, "key1", "value1", "key2", "value2")
		processor, _ := s.processor.(*ProcessorPickKey)
		processor.Exclude = []string{"content", "key1"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key2")), check.Equals, "value2")
	}
	{
		var log = "xxxx\nyyyy\nzzzz"
		logEvent := processortest.Log("content", log, "key1", "value1", "key2", "value2")
		processor, _ := s.processor.(*ProcessorPickKey)
		processor.Include = []string{"key2", "content"}
		processor.Exclude = nil
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 2)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("content")), check.Equals, log)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key2")), check.Equals, "value2")
	}
	{
		var log = "xxxx\nyyyy\nzzzz"
		logEvent := processortest.Log("content", log, "key1", "value1", "key2", "value2")
		processor, _ := s.processor.(*ProcessorPickKey)
		processor.Exclude = []string{"content", "key1"}
		processor.Include = []string{"key2", "content"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents.Len(), check.Equals, 1)
		c.Assert(pipeline.GetStringValue(outLogs[0].Contents.Get("key2")), check.Equals, "value2")
	}

}

func (s *processorTestSuite) TestNotMatch(c *check.C) {
	{
		var log = "xxxx\nyyyy\nzzzz"
		logEvent := processortest.Log("content", log, "key1", "value1", "key2", "value2")
		processor, _ := s.processor.(*ProcessorPickKey)
		processor.Include = nil
		processor.Exclude = []string{"content", "key1", "key2"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		outLogs := processortest.Apply(s.processor, logEvent)
		c.Assert(len(outLogs), check.Equals, 0)
	}
}

func newPickKey(t *testing.T, include, exclude []string) *ProcessorPickKey {
	p := &ProcessorPickKey{Include: include, Exclude: exclude}
	require.NoError(t, p.Init(mock.NewEmptyContext("p", "l", "c")))
	return p
}

func TestProcessLogEventInclude(t *testing.T) {
	p := newPickKey(t, []string{"key2", "content"}, nil)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("content", "xxxx")
	contents.Add("key1", "value1")
	contents.Add("key2", "value2")

	p.processLogEvent(log)

	require.Equal(t, 2, contents.Len())
	require.True(t, contents.Contains("content"))
	require.True(t, contents.Contains("key2"))
	require.False(t, contents.Contains("key1"))
}

func TestProcessLogEventExclude(t *testing.T) {
	p := newPickKey(t, nil, []string{"content", "key1"})

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("content", "xxxx")
	contents.Add("key1", "value1")
	contents.Add("key2", "value2")

	p.processLogEvent(log)

	require.Equal(t, 1, contents.Len())
	require.True(t, contents.Contains("key2"))
	require.Equal(t, "value2", contents.Get("key2"))
}

func TestProcessLogEventIncludeAndExclude(t *testing.T) {
	p := newPickKey(t, []string{"key2", "content"}, []string{"content", "key1"})

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("content", "xxxx")
	contents.Add("key1", "value1")
	contents.Add("key2", "value2")

	p.processLogEvent(log)

	require.Equal(t, 1, contents.Len())
	require.True(t, contents.Contains("key2"))
}

func TestProcessDropsEmptyLogEvent(t *testing.T) {
	// When all fields are excluded the Log event is dropped, matching v1
	// (process returns false once contents are empty). Metric/Span still pass.
	p := newPickKey(t, nil, []string{"content", "key1", "key2"})

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("content", "xxxx")
	contents.Add("key1", "value1")
	contents.Add("key2", "value2")
	metric := models.NewMetric("cpu", models.MetricTypeCounter, models.NewTags(), 0, &models.MetricSingleValue{Value: 1}, nil)

	group := &models.PipelineGroupEvents{Events: []models.PipelineEvent{log, metric}}
	context := helper.NewObservePipelineContext(10)
	p.Process(group, context)

	results := context.Collector().ToArray()
	require.Equal(t, 1, len(results))
	// The emptied Log is dropped; only the Metric survives.
	require.Equal(t, 1, len(results[0].Events))
	require.Equal(t, models.EventTypeMetric, results[0].Events[0].GetType())
	require.Equal(t, 0, contents.Len())
}

func TestProcessMetricPassThrough(t *testing.T) {
	p := newPickKey(t, nil, []string{"key1"})

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("key1", "value1")
	log.GetIndices().Add("key2", "value2")
	metric := models.NewMetric("cpu", models.MetricTypeCounter, models.NewTags(), 0, &models.MetricSingleValue{Value: 1}, nil)

	group := &models.PipelineGroupEvents{Events: []models.PipelineEvent{log, metric}}
	context := helper.NewObservePipelineContext(10)
	p.Process(group, context)

	results := context.Collector().ToArray()
	require.Equal(t, 1, len(results))
	require.Equal(t, 2, len(results[0].Events))
	require.Equal(t, models.EventTypeMetric, results[0].Events[1].GetType())
	// Log event field filtering applied.
	require.False(t, log.GetIndices().Contains("key1"))
	require.True(t, log.GetIndices().Contains("key2"))
}
