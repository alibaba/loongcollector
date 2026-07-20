// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package ratelimit

import (
	"context"
	"testing"
	"time"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
	"github.com/pingcap/check"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
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
	s.processor = pipeline.Processors["processor_rate_limit"]().(pipeline.ProcessorV1)
	_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
	logger.Info(context.Background(), "set up", s.processor.Description())
}

func (s *processorTestSuite) TearDownTest(c *check.C) {

}

func (s *processorTestSuite) TestDefault(c *check.C) {
	{
		// case: no configuration
		var log = "xxxx\nyyyy\nzzzz"
		processor, _ := s.processor.(*ProcessorRateLimit)
		processor.Limit = "3/s"
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		logArray := []*protocol.Log{
			test.CreateLogs("content", log, "key1", "value1", "key2", "value2"),
			test.CreateLogs("content", log, "key1", "value1", "key2", "value2"),
			test.CreateLogs("content", log, "key1", "value1", "key2", "value2"),
			test.CreateLogs("content", log, "key1", "value2", "key2", "value1"),
		}
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 3)
		c.Assert(len(outLogs[0].Contents), check.Equals, 3)
		time.Sleep(time.Second)
		outLogs = s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 3)
		c.Assert(len(outLogs[0].Contents), check.Equals, 3)
		// metric
		c.Assert(int64(processor.limitMetric.Collect().Value), check.Equals, int64(2))
	}
}

func (s *processorTestSuite) TestField(c *check.C) {
	{
		// case: single field
		var log = "xxxx\nyyyy\nzzzz"
		processor, _ := s.processor.(*ProcessorRateLimit)
		processor.Limit = "3/s"
		processor.Fields = []string{"key1"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		logArray := []*protocol.Log{
			test.CreateLogs("content", log, "key1", "value1", "key2", "value2"),
			test.CreateLogs("content", log, "key1", "value1", "key2", "value2"),
			test.CreateLogs("content", log, "key1", "value1", "key2", "value2"),
			test.CreateLogs("content", log, "key1", "value1", "key2", "value2"),

			test.CreateLogs("content", log, "key1", "value2", "key2", "value2"),
			test.CreateLogs("content", log, "key1", "value2", "key2", "value2"),
		}
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 5)
		// metric
		c.Assert(int64(processor.limitMetric.Collect().Value), check.Equals, int64(1))
	}
	{
		// case: multiple fields
		var log = "xxxx\nyyyy\nzzzz"
		processor, _ := s.processor.(*ProcessorRateLimit)
		processor.Limit = "3/s"
		processor.Fields = []string{"key1", "key2"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		logArray := []*protocol.Log{
			test.CreateLogs("content", log, "key1", "value1", "key2", "value2"),
			test.CreateLogs("content", log, "key1", "value1", "key2", "value2"),
			test.CreateLogs("content", log, "key1", "value1", "key2", "value2"),
			test.CreateLogs("content", log, "key1", "value1", "key2", "value2"),

			test.CreateLogs("content", log, "key1", "value2", "key2", "value2"),
			test.CreateLogs("content", log, "key1", "value2", "key2", "value2"),
			test.CreateLogs("content", log, "key1", "value2", "key2", "value2"),
			test.CreateLogs("content", log, "key1", "value2", "key2", "value2"),

			test.CreateLogs("content", log, "key1", "value2", "key2", "value1"),
			test.CreateLogs("content", log, "key1", "value2", "key2", "value1"),
			test.CreateLogs("content", log, "key1", "value2", "key2", "value1"),
			test.CreateLogs("content", log, "key1", "value2", "key2", "value1"),
		}
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 9)
		// metric
		c.Assert(int64(processor.limitMetric.Collect().Value), check.Equals, int64(3))
	}
}

func (s *processorTestSuite) TestGC(c *check.C) {
	{
		// case: gc in single process
		var log = "xxxx\nyyyy\nzzzz"
		processor, _ := s.processor.(*ProcessorRateLimit)
		processor.Limit = "3/s"
		processor.Fields = []string{"key1"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		logArray := make([]*protocol.Log, 10010)
		for i := 0; i < 5; i++ {
			logArray[i] = test.CreateLogs("content", log, "key1", "value1", "key2", "value2")
		}
		for i := 5; i < 10005; i++ {
			logArray[i] = test.CreateLogs("content", log, "key1", "value2", "key2", "value2")
		}
		for i := 10005; i < 10010; i++ {
			logArray[i] = test.CreateLogs("content", log, "key1", "value1", "key2", "value2")
		}
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 6)
		// metric
		c.Assert(int64(processor.limitMetric.Collect().Value), check.Equals, int64(10004))
	}
	{
		// case: gc in multiple process
		var log = "xxxx\nyyyy\nzzzz"
		processor, _ := s.processor.(*ProcessorRateLimit)
		processor.Limit = "3/s"
		processor.Fields = []string{"key1"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		logArray := make([]*protocol.Log, 10005)
		for i := 0; i < 5; i++ {
			logArray[i] = test.CreateLogs("content", log, "key1", "value1", "key2", "value2")
		}
		for i := 5; i < 10005; i++ {
			logArray[i] = test.CreateLogs("content", log, "key1", "value2", "key2", "value2")
		}
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 6)
		logArray = make([]*protocol.Log, 5)
		for i := 0; i < 5; i++ {
			logArray[i] = test.CreateLogs("content", log, "key1", "value1", "key2", "value2")
		}
		outLogs = s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 0)
		// metric
		c.Assert(int64(processor.limitMetric.Collect().Value), check.Equals, int64(10004))
	}
}

// ---- v2 (PipelineEvent / SendPb) Process path tests ----

func newV2Log(kv ...string) *models.Log {
	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	for i := 0; i+1 < len(kv); i += 2 {
		log.GetIndices().Add(kv[i], kv[i+1])
	}
	return log
}

// TestProcessorRateLimit_ProcessV2Filter verifies that the v2 Process drops Log
// events once the shared rate limit is exhausted while keeping earlier ones and
// passing Metric events through unchanged. Order is preserved.
func TestProcessorRateLimit_ProcessV2Filter(t *testing.T) {
	processor := &ProcessorRateLimit{Limit: "3/s"}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log1 := newV2Log("k", "1")
	log2 := newV2Log("k", "2")
	log3 := newV2Log("k", "3")
	log4 := newV2Log("k", "4") // exceeds 3/s -> drop
	metric := models.NewSingleValueMetric("m", models.MetricTypeCounter, models.NewTags(), 0, 1.0)

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{
		Events: []models.PipelineEvent{log1, log2, metric, log3, log4},
	}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	events := results[0].Events
	require.Len(t, events, 4, "first three logs plus the metric survive")
	assert.Equal(t, log1, events[0])
	assert.Equal(t, log2, events[1])
	assert.Equal(t, metric, events[2], "metric events must pass through unchanged")
	assert.Equal(t, log3, events[3])
	assert.Equal(t, int64(1), int64(processor.limitMetric.Collect().Value))
}

// TestProcessorRateLimit_ProcessV2Fields verifies the per-key limiting when
// Fields are configured: separate limit keys have independent budgets.
func TestProcessorRateLimit_ProcessV2Fields(t *testing.T) {
	processor := &ProcessorRateLimit{Limit: "3/s", Fields: []string{"key1"}}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	events := []models.PipelineEvent{
		newV2Log("key1", "a"),
		newV2Log("key1", "a"),
		newV2Log("key1", "a"),
		newV2Log("key1", "a"), // 4th "a" -> drop
		newV2Log("key1", "b"), // different key -> allowed
	}

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: events}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 4, "three 'a' logs plus one 'b' log survive")
	assert.Equal(t, int64(1), int64(processor.limitMetric.Collect().Value))
}
