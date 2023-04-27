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
		logPb := test.CreateLogs("content", log, "key1", "value1", "key2", "value2")
		processor, _ := s.processor.(*ProcessorKeyFilter)
		processor.Exclude = []string{"keyd"}
		processor.Include = []string{"content", "key1", "key2"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 3)

		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[1].GetKey(), check.Equals, "key1")
		c.Assert(outLogs[0].Contents[2].GetKey(), check.Equals, "key2")

		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, log)
		c.Assert(outLogs[0].Contents[1].GetValue(), check.Equals, "value1")
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, "value2")
	}
	{
		logPb := test.CreateLogs(`10.200.98.220`, "hello ilogtail")
		processor, _ := s.processor.(*ProcessorKeyFilter)
		reg := `([\d\.]+)`
		processor.Include = []string{reg}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 1)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, `10.200.98.220`)
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "hello ilogtail")
	}
	{
		logPb := test.CreateLogs("request_time", "20", "request_length", "2314", "browser", "aliyun-sdk-java")
		processor, _ := s.processor.(*ProcessorKeyFilter)
		processor.Include = []string{"request.*"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 3)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "request_time")
		c.Assert(outLogs[0].Contents[1].GetKey(), check.Equals, "request_length")
		c.Assert(outLogs[0].Contents[2].GetKey(), check.Equals, "browser")

		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "20")
		c.Assert(outLogs[0].Contents[1].GetValue(), check.Equals, "2314")
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, "aliyun-sdk-java")
	}
}

func (s *processorTestSuite) TestNotMatch(c *check.C) {

	{
		logPb := test.CreateLogs(`abc10.200.98.220`, "hello ilogtail")
		processor, _ := s.processor.(*ProcessorKeyFilter)
		reg := `^([\d\.]+)`
		processor.Include = []string{reg}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 0)
	}
	{
		logPb := test.CreateLogs("browser", "ali-sls-ilogtail", "status", "200")
		processor, _ := s.processor.(*ProcessorKeyFilter)
		processor.Include = []string{"request.*"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 0)
	}
	{
		logPb := test.CreateLogs("content", "log", "key1", "value1", "key2", "value2")
		processor, _ := s.processor.(*ProcessorKeyFilter)
		processor.Exclude = []string{"key1"} // exclude has higher priority
		processor.Include = []string{"key1", "key2"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 0)
	}
}
