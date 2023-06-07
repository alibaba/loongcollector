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

	"github.com/pingcap/check"

	"github.com/alibaba/ilogtail/pkg/helper"
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
	s.processor = pipeline.Processors["processor_split_log_regex"]().(pipeline.ProcessorV1)
	_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))
	logger.Info(context.Background(), "set up", s.processor.Description())
}

func (s *processorTestSuite) TearDownTest(c *check.C) {

}

func (s *processorTestSuite) TestDefault(c *check.C) {
	{
		var log = "xxxx\nyyyy\nzzzz"
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 3)
	}
	{
		var log = "xxxx\nyyyy\nzzzz\n"
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 3)
	}
	{
		var log = "\nxxxx\nyyyy\nzzzz\n"
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 4)
	}
}

func (s *processorTestSuite) TestMultiLine(c *check.C) {
	processor, _ := s.processor.(*ProcessorSplitRegex)
	processor.SplitRegex = "\\[.*"
	_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

	{
		var log = "[2017-12-12 00:00:00] 你好\nhello\n\n[2017xxxxxx]yyyy\n [zzzz\n["
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 3)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "[2017-12-12 00:00:00] 你好\nhello\n")
		c.Assert(outLogs[1].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[1].Contents[0].GetValue(), check.Equals, "[2017xxxxxx]yyyy\n [zzzz")
		c.Assert(outLogs[2].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[2].Contents[0].GetValue(), check.Equals, "[")
	}

	// Case: only one matched line.
	{
		var log = "[2017-12-12 00:00:00] xxxxxx"
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, log)
	}

	// Case: only one matched line ends with line feed.
	{
		var log = "[2017-12-12 00:00:00] xxxxxx\n"
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, log)
	}

	// Case: one unmatched line.
	{
		var log = "xxxxxx"
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, log)
	}

	// Case: one unmatched line ends with line feed.
	{
		var log = "xxxxxx\n"
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, log)
	}

	// Case: multiple unmatched lines.
	{
		var log = "xxxxxx\nyyyyy\nzzzzzz"
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, log)
	}

	// Case: multiple unmatched lines end with line feed.
	{
		var log = "xxxxxx\nyyyyy\nzzzzzz\n"
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, log)
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
		logPb := test.CreateLogs("preserve_1", "1", "content", log, "preserve_2", "2")
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 3)
		c.Assert(len(outLogs[0].Contents), check.Equals, 3)
		c.Assert(len(outLogs[1].Contents), check.Equals, 3)
		c.Assert(len(outLogs[2].Contents), check.Equals, 3)
		c.Assert(outLogs[0].Contents[2].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, "[2017-12-12 00:00:00] 你好\n  hello\n\t")
		c.Assert(outLogs[1].Contents[2].GetKey(), check.Equals, "content")
		c.Assert(outLogs[1].Contents[2].GetValue(), check.Equals, "[2017xxxxxx]yyyy")
		c.Assert(outLogs[2].Contents[2].GetKey(), check.Equals, "content")
		c.Assert(outLogs[2].Contents[2].GetValue(), check.Equals, "zzzz\n\t[")
		c.Assert(outLogs[1].Contents[0].GetKey(), check.Equals, "preserve_1")
		c.Assert(outLogs[1].Contents[0].GetValue(), check.Equals, "1")
		c.Assert(outLogs[1].Contents[1].GetKey(), check.Equals, "preserve_2")
		c.Assert(outLogs[1].Contents[1].GetValue(), check.Equals, "2")
	}

	{
		processor, _ := s.processor.(*ProcessorSplitRegex)
		processor.PreserveOthers = false
		processor.SplitRegex = "\\S+.*"
		processor.SplitKey = "content"
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t["
		logPb := test.CreateLogs("preserve_1", "1", "content", log, "preserve_2", "2")
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 3)
		c.Assert(len(outLogs[0].Contents), check.Equals, 1)
		c.Assert(len(outLogs[1].Contents), check.Equals, 1)
		c.Assert(len(outLogs[2].Contents), check.Equals, 1)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "[2017-12-12 00:00:00] 你好\n  hello\n\t")
		c.Assert(outLogs[1].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[1].Contents[0].GetValue(), check.Equals, "[2017xxxxxx]yyyy")
		c.Assert(outLogs[2].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[2].Contents[0].GetValue(), check.Equals, "zzzz\n\t[")
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
		logPb := test.CreateLogs("preserve_1", "1", "content", log, "preserve_2", "2")
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
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
		logPb := test.CreateLogs("preserve_1", "1", "content", log, "preserve_2", "2")
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)

		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "preserve_1")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "1")

		c.Assert(outLogs[0].Contents[1].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[1].GetValue(), check.Equals, "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t[")

		c.Assert(outLogs[0].Contents[2].GetKey(), check.Equals, "preserve_2")
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, "2")
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
			logPb := test.CreateLogs("content", log, helper.FileOffsetKey, "1000")
			logArray := make([]*protocol.Log, 1)
			logArray[0] = logPb
			outLogs := s.processor.ProcessLogs(logArray)
			c.Assert(len(outLogs), check.Equals, 3)
			s.assertLogPosition(c, outLogs[0], "1000")
			s.assertLogPosition(c, outLogs[1], "1036")
			s.assertLogPosition(c, outLogs[2], "1063")
		}
	}

	{
		{
			var log = "[2017-12-12 00:00:00] 你好\nhello\n\n [2017-12-12 00:00:00] yyyy\n[2017-12-12 00:00:00] 123"
			logPb := test.CreateLogs("content", log, helper.FileOffsetKey, "1000")
			logArray := make([]*protocol.Log, 1)
			logArray[0] = logPb
			outLogs := s.processor.ProcessLogs(logArray)
			c.Assert(len(outLogs), check.Equals, 2)
			s.assertLogPosition(c, outLogs[0], "1000")
			s.assertLogPosition(c, outLogs[1], "1064")
		}
	}
}

func (s *processorTestSuite) assertLogPosition(c *check.C, log *protocol.Log, offset string) {
	cont := helper.GetFileOffsetTag(log)
	c.Assert(cont, check.NotNil)
	c.Assert(cont.GetValue(), check.Equals, offset)
}
