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

	"github.com/pingcap/check"

	"github.com/alibaba/ilogtail/pkg/logger"
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
		c.Assert(processor.limitMetric.Get(), check.Equals, int64(2))
		c.Assert(processor.processedMetric.Get(), check.Equals, int64(8))
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
		c.Assert(processor.limitMetric.Get(), check.Equals, int64(1))
		c.Assert(processor.processedMetric.Get(), check.Equals, int64(6))
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
		c.Assert(processor.limitMetric.Get(), check.Equals, int64(3))
		c.Assert(processor.processedMetric.Get(), check.Equals, int64(12))
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
		c.Assert(processor.limitMetric.Get(), check.Equals, int64(10004))
		c.Assert(processor.processedMetric.Get(), check.Equals, int64(10010))
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
		c.Assert(processor.limitMetric.Get(), check.Equals, int64(10004))
		c.Assert(processor.processedMetric.Get(), check.Equals, int64(10010))
	}
}
