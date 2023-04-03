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

package logstring

import (
	"context"
	"fmt"
	"strconv"
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	_ "github.com/alibaba/ilogtail/pkg/logger/test"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"github.com/pingcap/check"
)

var _ = check.Suite(&processorTestSuite{})

func Test(t *testing.T) {
	check.TestingT(t)
}

func TestDefault(t *testing.T) {
	p := &ProcessorSplit{SplitSep: "\n", PreserveOthers: true}

	log := &protocol.Log{}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: "hello1\nhello2\nhello3"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "__log_topic__", Value: "test"})

	logs := p.ProcessLogs([]*protocol.Log{log})
	require.Equal(t, 3, len(logs), "logs: %v", logs)
	for idx, l := range logs {
		require.Equal(t, 2, len(l.Contents))
		require.Equal(t, "__log_topic__", l.Contents[0].Key)
		require.Equal(t, "test", l.Contents[0].Value)
		require.Equal(t, "content", l.Contents[1].Key)
		require.Equal(t, "hello"+strconv.Itoa(idx+1), l.Contents[1].Value)
	}

	fmt.Println(p.ProcessLogs([]*protocol.Log{log}))
}

type processorTestSuite struct {
	processor pipeline.ProcessorV1
}

func (s *processorTestSuite) SetUpTest(c *check.C) {
	s.processor = pipeline.Processors["processor_split_log_string"]().(pipeline.ProcessorV1)
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
		c.Assert(len(outLogs), check.Equals, 3)
	}
}

func (s *processorTestSuite) TestNormal(c *check.C) {
	{
		processor, _ := s.processor.(*ProcessorSplit)
		processor.PreserveOthers = false
		processor.NoKeyError = true
		processor.SplitSep = "你好"
		processor.SplitKey = "content"
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

		var log = "你好[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t你好[你好"
		logPb := test.CreateLogs("preserve_1", "1", "content", log, "preserve_2", "2")
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 3)
		c.Assert(len(outLogs[0].Contents), check.Equals, 1)
		c.Assert(len(outLogs[1].Contents), check.Equals, 1)
		c.Assert(len(outLogs[2].Contents), check.Equals, 1)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "[2017-12-12 00:00:00] ")

		c.Assert(outLogs[1].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[1].Contents[0].GetValue(), check.Equals, "\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t")

		c.Assert(outLogs[2].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(outLogs[2].Contents[0].GetValue(), check.Equals, "[")
	}

	{
		processor, _ := s.processor.(*ProcessorSplit)
		processor.PreserveOthers = true
		processor.NoKeyError = true
		processor.SplitSep = "你好"
		processor.SplitKey = "content"
		_ = s.processor.Init(mock.NewEmptyContext("p", "l", "c"))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t你好["
		logPb := test.CreateLogs("preserve_1", "1", "content", log, "preserve_2", "2")
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 3)
		c.Assert(len(outLogs[0].Contents), check.Equals, 3)
		c.Assert(len(outLogs[1].Contents), check.Equals, 3)
		c.Assert(len(outLogs[2].Contents), check.Equals, 3)

		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "preserve_1")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "1")
		c.Assert(outLogs[0].Contents[1].GetKey(), check.Equals, "preserve_2")
		c.Assert(outLogs[0].Contents[1].GetValue(), check.Equals, "2")
		c.Assert(outLogs[0].Contents[2].GetKey(), check.Equals, "content")
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, "[2017-12-12 00:00:00] ")

		c.Assert(outLogs[1].Contents[0].GetKey(), check.Equals, "preserve_1")
		c.Assert(outLogs[1].Contents[0].GetValue(), check.Equals, "1")
		c.Assert(outLogs[1].Contents[1].GetKey(), check.Equals, "preserve_2")
		c.Assert(outLogs[1].Contents[1].GetValue(), check.Equals, "2")
		c.Assert(outLogs[1].Contents[2].GetKey(), check.Equals, "content")
		c.Assert(outLogs[1].Contents[2].GetValue(), check.Equals, "\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t")

		c.Assert(outLogs[2].Contents[0].GetKey(), check.Equals, "preserve_1")
		c.Assert(outLogs[2].Contents[0].GetValue(), check.Equals, "1")
		c.Assert(outLogs[2].Contents[1].GetKey(), check.Equals, "preserve_2")
		c.Assert(outLogs[2].Contents[1].GetValue(), check.Equals, "2")
		c.Assert(outLogs[2].Contents[2].GetKey(), check.Equals, "content")
		c.Assert(outLogs[2].Contents[2].GetValue(), check.Equals, "[")
	}
}

func (s *processorTestSuite) TestNoKeyAlarmAndPreserve(c *check.C) {
	{
		processor, _ := s.processor.(*ProcessorSplit)
		processor.PreserveOthers = false
		processor.NoKeyError = true
		processor.SplitSep = "\\S+.*"
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
		processor, _ := s.processor.(*ProcessorSplit)
		processor.PreserveOthers = true
		processor.NoKeyError = true
		processor.SplitSep = "\\S+.*"
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

	{
		processor, _ := s.processor.(*ProcessorSplit)
		processor.EnableLogPositionMeta = true
		var log = "xxxx\nyyyy\nzzzz\n"
		logPb := test.CreateLogs("content", log, helper.FileOffsetKey, "1000")
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 3)
		for i := 0; i < len(outLogs); i++ {
			cont := helper.GetFileOffsetTag(outLogs[i])
			c.Assert(cont, check.NotNil)
			off, _ := strconv.ParseInt(cont.Value, 10, 64)
			c.Assert(off, check.Equals, int64(1000+i*5))
		}
	}

	{
		processor, _ := s.processor.(*ProcessorSplit)
		processor.EnableLogPositionMeta = true
		var log = "xxxx\nyyyy\nzzzz\n"
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 3)
		for i := 0; i < len(outLogs); i++ {
			cont := helper.GetFileOffsetTag(outLogs[i])
			c.Assert(cont, check.IsNil)
		}
	}

	{
		processor, _ := s.processor.(*ProcessorSplit)
		processor.EnableLogPositionMeta = false
		var log = "xxxx\nyyyy\nzzzz\n"
		logPb := test.CreateLogs("content", log, helper.FileOffsetKey, "1000")
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 3)
		for i := 0; i < len(outLogs); i++ {
			cont := helper.GetFileOffsetTag(outLogs[i])
			c.Assert(cont, check.NotNil)
			off, _ := strconv.ParseInt(cont.Value, 10, 64)
			c.Assert(off, check.Equals, int64(1000))
		}
	}
}
