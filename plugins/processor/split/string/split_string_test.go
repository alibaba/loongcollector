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

package string

import (
	"context"
	"testing"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"github.com/pingcap/check"
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
	s.processor = pipeline.Processors["processor_split_string"]().(pipeline.ProcessorV1)
	_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
	logger.Info(context.Background(), "set up", s.processor.Description())
}

func (s *processorTestSuite) TearDownTest(c *check.C) {

}

func (s *processorTestSuite) TestInitError(c *check.C) {
	processor, _ := s.processor.(*ProcessorSplitString)
	processor.SplitSep = ""
	c.Assert(s.processor.Init(mock.NewEmptyContext("p", "l", "c")), check.NotNil)
}

func (s *processorTestSuite) TestDefault(c *check.C) {
	{
		var log = "xxxx\nyyyy\nzzzz"
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 1)
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, log)
	}
	{
		var log = "xxxx\nyyyy\nzzzz\n"
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 1)
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, log)
	}
	{
		var log = "\nxxxx\nyyyy\nzzzz\n"
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 1)
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, log)
	}
}

func (s *processorTestSuite) TestNormal(c *check.C) {
	{
		processor, _ := s.processor.(*ProcessorSplitString)
		processor.PreserveOthers = false
		processor.KeepSource = true
		processor.NoKeyError = true
		processor.SplitSep = "你好"
		processor.SourceKey = "content"
		processor.SplitKeys = []string{"key1", "key2"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

		var log = "你好[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t你好[你好"
		logPb := test.CreateLogs("preserve_1", "1", "content", log, "preserve_2", "2")
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 5)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "preserve_1")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "1")

		c.Assert(outLogs[0].Contents[1].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[1].GetValue(), check.Equals, log)

		c.Assert(outLogs[0].Contents[2].GetKey(), check.Equals, "preserve_2")
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, "2")

		c.Assert(outLogs[0].Contents[3].GetKey(), check.Equals, "key1")
		c.Assert(outLogs[0].Contents[3].GetValue(), check.Equals, "")

		c.Assert(outLogs[0].Contents[4].GetKey(), check.Equals, "key2")
		c.Assert(outLogs[0].Contents[4].GetValue(), check.Equals, "[2017-12-12 00:00:00] ")
	}

	{
		processor, _ := s.processor.(*ProcessorSplitString)
		processor.PreserveOthers = true
		processor.KeepSource = false
		processor.NoKeyError = false
		processor.SplitSep = "你好"
		processor.SourceKey = "content"
		processor.SplitKeys = []string{"key1", "key2"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

		var log = "你好[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t你好[你好"
		logPb := test.CreateLogs("preserve_1", "1", "content", log, "preserve_2", "2")
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 5)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "preserve_1")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "1")

		c.Assert(outLogs[0].Contents[1].GetKey(), check.Equals, "preserve_2")
		c.Assert(outLogs[0].Contents[1].GetValue(), check.Equals, "2")

		c.Assert(outLogs[0].Contents[2].GetKey(), check.Equals, "key1")
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, "")

		c.Assert(outLogs[0].Contents[3].GetKey(), check.Equals, "key2")
		c.Assert(outLogs[0].Contents[3].GetValue(), check.Equals, "[2017-12-12 00:00:00] ")

		c.Assert(outLogs[0].Contents[4].GetValue(), check.Equals, "\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t你好[你好")
	}
}

func (s *processorTestSuite) TestNoKeyAlarmAndPreserve(c *check.C) {
	{
		processor, _ := s.processor.(*ProcessorSplitString)
		processor.PreserveOthers = false
		processor.NoKeyError = true
		processor.SplitSep = "\\S+.*"
		processor.SourceKey = "no_this_key"
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t["
		logPb := test.CreateLogs("preserve_1", "1", "content", log, "preserve_2", "2")
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 3)
	}

	{
		processor, _ := s.processor.(*ProcessorSplitString)
		processor.PreserveOthers = true
		processor.NoKeyError = true
		processor.KeepSource = false
		processor.SplitSep = "\\S+.*"
		processor.SourceKey = "content"
		processor.SplitKeys = []string{"key_1", "key_2"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t["
		logPb := test.CreateLogs("preserve_1", "1", "content", log, "preserve_2", "2")
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 3)
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, log)
	}
}

func (s *processorTestSuite) TestExpand(c *check.C) {
	{
		processor, _ := s.processor.(*ProcessorSplitString)
		processor.PreserveOthers = true
		processor.ExpandOthers = true
		processor.ExpandKeyPrefix = "param_"
		processor.KeepSource = true
		processor.NoKeyError = true
		processor.SplitSep = "&"
		processor.SourceKey = "content"
		processor.SplitKeys = []string{}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

		var log = "abc&def&gh"
		logPb := test.CreateLogs("preserve_1", "1", "content", log, "preserve_2", "2")
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 6)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "preserve_1")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "1")

		c.Assert(outLogs[0].Contents[1].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[1].GetValue(), check.Equals, log)

		c.Assert(outLogs[0].Contents[2].GetKey(), check.Equals, "preserve_2")
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, "2")

		c.Assert(outLogs[0].Contents[3].GetKey(), check.Equals, "param_1")
		c.Assert(outLogs[0].Contents[3].GetValue(), check.Equals, "abc")

		c.Assert(outLogs[0].Contents[4].GetKey(), check.Equals, "param_2")
		c.Assert(outLogs[0].Contents[4].GetValue(), check.Equals, "def")

		c.Assert(outLogs[0].Contents[5].GetKey(), check.Equals, "param_3")
		c.Assert(outLogs[0].Contents[5].GetValue(), check.Equals, "gh")
	}

	{
		processor, _ := s.processor.(*ProcessorSplitString)
		processor.PreserveOthers = true
		processor.ExpandOthers = true
		processor.ExpandKeyPrefix = "param_"
		processor.KeepSource = true
		processor.NoKeyError = true
		processor.SplitSep = "&"
		processor.SourceKey = "content"
		processor.SplitKeys = []string{"key"}
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

		var log = "abc&def&gh"
		logPb := test.CreateLogs("preserve_1", "1", "content", log, "preserve_2", "2")
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 6)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "preserve_1")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "1")

		c.Assert(outLogs[0].Contents[1].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[1].GetValue(), check.Equals, log)

		c.Assert(outLogs[0].Contents[2].GetKey(), check.Equals, "preserve_2")
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, "2")

		c.Assert(outLogs[0].Contents[3].GetKey(), check.Equals, "key")
		c.Assert(outLogs[0].Contents[3].GetValue(), check.Equals, "abc")

		c.Assert(outLogs[0].Contents[4].GetKey(), check.Equals, "param_1")
		c.Assert(outLogs[0].Contents[4].GetValue(), check.Equals, "def")

		c.Assert(outLogs[0].Contents[5].GetKey(), check.Equals, "param_2")
		c.Assert(outLogs[0].Contents[5].GetValue(), check.Equals, "gh")
	}
}
