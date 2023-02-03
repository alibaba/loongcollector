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

package char

import (
	"context"
	"testing"

	"github.com/stretchr/testify/require"

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
	s.processor = pipeline.Processors["processor_split_char"]().(pipeline.ProcessorV1)
	require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))
	logger.Info(context.Background(), "set up", s.processor.Description())
}

func (s *processorTestSuite) TearDownTest(c *check.C) {
}

func (s *processorTestSuite) TestInitError(c *check.C) {
	processor, _ := s.processor.(*ProcessorSplitChar)
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
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = false
		processor.KeepSource = true
		processor.NoKeyError = true
		processor.SplitSep = "]"
		processor.SourceKey = "content"
		processor.SplitKeys = []string{"key1", "key2"}
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

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
		c.Assert(outLogs[0].Contents[3].GetValue(), check.Equals, "你好[2017-12-12 00:00:00")

		c.Assert(outLogs[0].Contents[4].GetKey(), check.Equals, "key2")
		c.Assert(outLogs[0].Contents[4].GetValue(), check.Equals, " 你好\n  hello\n\t\n[2017xxxxxx")
	}

	{
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = true
		processor.KeepSource = false
		processor.NoKeyError = false
		processor.SplitSep = "["
		processor.SourceKey = "content"
		processor.SplitKeys = []string{"key1", "key2"}
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t你好[你好"
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
		c.Assert(outLogs[0].Contents[3].GetValue(), check.Equals, "2017-12-12 00:00:00] 你好\n  hello\n\t\n")

		c.Assert(outLogs[0].Contents[4].GetValue(), check.Equals, "2017xxxxxx]yyyy\nzzzz\n\t你好[你好")
	}

	{
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = true
		processor.KeepSource = false
		processor.NoKeyError = false
		processor.SplitSep = "["
		processor.SourceKey = "content"
		processor.SplitKeys = []string{"key1", "key2"}
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t你好[你好"
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
		c.Assert(outLogs[0].Contents[3].GetValue(), check.Equals, "2017-12-12 00:00:00] 你好\n  hello\n\t\n")

		c.Assert(outLogs[0].Contents[4].GetValue(), check.Equals, "2017xxxxxx]yyyy\nzzzz\n\t你好[你好")
	}
}

func (s *processorTestSuite) TestQuote(c *check.C) {
	{
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = false
		processor.NoKeyError = true
		processor.SplitSep = ","
		processor.SourceKey = "content"
		processor.QuoteFlag = true
		processor.Quote = "\""
		processor.SplitKeys = []string{"key1", "key2", "key3", "key4", "key5"}
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

		var log = `1999,Chevy,"Venture ""Extended Edition, Very Large""","",5000.00`
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 5)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "key1")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "1999")

		c.Assert(outLogs[0].Contents[1].GetKey(), check.Equals, "key2")
		c.Assert(outLogs[0].Contents[1].GetValue(), check.Equals, "Chevy")

		c.Assert(outLogs[0].Contents[2].GetKey(), check.Equals, "key3")
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, `Venture "Extended Edition, Very Large"`)

		c.Assert(outLogs[0].Contents[3].GetKey(), check.Equals, "key4")
		c.Assert(outLogs[0].Contents[3].GetValue(), check.Equals, "")

		c.Assert(outLogs[0].Contents[4].GetKey(), check.Equals, "key5")
		c.Assert(outLogs[0].Contents[4].GetValue(), check.Equals, "5000.00")
	}
	{
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = false
		processor.NoKeyError = true
		processor.SplitSep = ","
		processor.SourceKey = "content"
		processor.QuoteFlag = true
		processor.Quote = "\""
		processor.SplitKeys = []string{"key1", "key2", "key3", "key4", "key5"}
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

		var log = `1999,Chevy,"Venture ""Extended Edition, Very Large""","","5000.00"""`
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 5)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "key1")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "1999")

		c.Assert(outLogs[0].Contents[1].GetKey(), check.Equals, "key2")
		c.Assert(outLogs[0].Contents[1].GetValue(), check.Equals, "Chevy")

		c.Assert(outLogs[0].Contents[2].GetKey(), check.Equals, "key3")
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, `Venture "Extended Edition, Very Large"`)

		c.Assert(outLogs[0].Contents[3].GetKey(), check.Equals, "key4")
		c.Assert(outLogs[0].Contents[3].GetValue(), check.Equals, "")

		c.Assert(outLogs[0].Contents[4].GetKey(), check.Equals, "key5")
		c.Assert(outLogs[0].Contents[4].GetValue(), check.Equals, "5000.00\"")
	}
	{
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = true
		processor.NoKeyError = true
		processor.SplitSep = ","
		processor.SourceKey = "content"
		processor.QuoteFlag = true
		processor.Quote = "\""
		processor.SplitKeys = []string{"key1", "key2", "key3", "key4", "key5"}
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

		var log = `1999,Chevy,"Venture ""Extended Edition, Very Large""","","5000.00""", ,,,`
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 6)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "key1")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "1999")

		c.Assert(outLogs[0].Contents[1].GetKey(), check.Equals, "key2")
		c.Assert(outLogs[0].Contents[1].GetValue(), check.Equals, "Chevy")

		c.Assert(outLogs[0].Contents[2].GetKey(), check.Equals, "key3")
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, `Venture "Extended Edition, Very Large"`)

		c.Assert(outLogs[0].Contents[3].GetKey(), check.Equals, "key4")
		c.Assert(outLogs[0].Contents[3].GetValue(), check.Equals, "")

		c.Assert(outLogs[0].Contents[4].GetKey(), check.Equals, "key5")
		c.Assert(outLogs[0].Contents[4].GetValue(), check.Equals, "5000.00\"")

		c.Assert(outLogs[0].Contents[5].GetKey(), check.Equals, "_split_preserve_")
		c.Assert(outLogs[0].Contents[5].GetValue(), check.Equals, " ,,,")
	}
	{
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = true
		processor.NoKeyError = true
		processor.SplitSep = ","
		processor.SourceKey = "content"
		processor.QuoteFlag = true
		processor.Quote = "\""
		processor.SplitKeys = []string{"key1", "key2", "key3", "key4", "key5"}
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

		var log = `,Chevy,"Venture ""Extended Edition, Very Large""",,"5000.00""", `
		logPb := test.CreateLogs("content", log)
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 6)
		c.Assert(outLogs[0].Contents[0].GetKey(), check.Equals, "key1")
		c.Assert(outLogs[0].Contents[0].GetValue(), check.Equals, "")

		c.Assert(outLogs[0].Contents[1].GetKey(), check.Equals, "key2")
		c.Assert(outLogs[0].Contents[1].GetValue(), check.Equals, "Chevy")

		c.Assert(outLogs[0].Contents[2].GetKey(), check.Equals, "key3")
		c.Assert(outLogs[0].Contents[2].GetValue(), check.Equals, `Venture "Extended Edition, Very Large"`)

		c.Assert(outLogs[0].Contents[3].GetKey(), check.Equals, "key4")
		c.Assert(outLogs[0].Contents[3].GetValue(), check.Equals, "")

		c.Assert(outLogs[0].Contents[4].GetKey(), check.Equals, "key5")
		c.Assert(outLogs[0].Contents[4].GetValue(), check.Equals, "5000.00\"")

		c.Assert(outLogs[0].Contents[5].GetKey(), check.Equals, "_split_preserve_")
		c.Assert(outLogs[0].Contents[5].GetValue(), check.Equals, " ")
	}
}

func (s *processorTestSuite) TestNoKeyAlarmAndPreserve(c *check.C) {
	{
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = false
		processor.NoKeyError = true
		processor.SplitSep = "["
		processor.SourceKey = "no_this_key"
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t["
		logPb := test.CreateLogs("preserve_1", "1", "content", log, "preserve_2", "2")
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 3)
	}

	{
		processor, _ := s.processor.(*ProcessorSplitChar)
		processor.PreserveOthers = true
		processor.NoKeyError = true
		processor.KeepSource = false
		processor.SplitSep = "["
		processor.SourceKey = "no_this_key"
		processor.SplitKeys = []string{"key_1", "key_2"}
		require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

		var log = "[2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t["
		logPb := test.CreateLogs("preserve_1", "1", "content", log, "preserve_2", "2")
		logArray := make([]*protocol.Log, 1)
		logArray[0] = logPb
		outLogs := s.processor.ProcessLogs(logArray)
		c.Assert(len(outLogs), check.Equals, 1)
		c.Assert(len(outLogs[0].Contents), check.Equals, 3)
		c.Assert(outLogs[0].Contents[1].GetValue(), check.Equals, log)
	}
}

func (s *processorTestSuite) TestKeepSourceIfParseError(c *check.C) {
	processor, _ := s.processor.(*ProcessorSplitChar)
	processor.PreserveOthers = true
	processor.KeepSource = false
	processor.KeepSourceIfParseError = true
	processor.NoKeyError = false
	processor.SplitSep = "["
	processor.SourceKey = "content"
	processor.QuoteFlag = true
	processor.Quote = "\""
	processor.SplitKeys = []string{"key1", "key2", "key3", "key4", "key5"}
	require.NoError(c, s.processor.Init(mock.NewEmptyContext("p", "l", "c")))

	var log = "2017-12-12 00:00:00] 你好\n  hello\n\t\n[2017xxxxxx]yyyy\nzzzz\n\t你好[你好[\"[hello\"[\"\"invalid\""
	logPb := test.CreateLogs("preserve_1", "1", "content", log, "preserve_2", "2")
	logArray := make([]*protocol.Log, 1)
	logArray[0] = logPb
	outLogs := s.processor.ProcessLogs(logArray)

	require.Equal(c, 1, len(outLogs))
	require.Equal(c, 7, len(outLogs[0].Contents))

	require.Equal(c, "preserve_1", outLogs[0].Contents[0].Key)
	require.Equal(c, "1", outLogs[0].Contents[0].Value)

	// Encounter parsing error, source is kept.
	require.Equal(c, "content", outLogs[0].Contents[1].Key)
	require.Equal(c, log, outLogs[0].Contents[1].Value)

	require.Equal(c, "preserve_2", outLogs[0].Contents[2].Key)
	require.Equal(c, "2", outLogs[0].Contents[2].Value)

	// Partial keys are extracted.
	require.Equal(c, "key1", outLogs[0].Contents[3].Key)
	require.Equal(c, "2017-12-12 00:00:00] 你好\n  hello\n\t\n", outLogs[0].Contents[3].Value)
	require.Equal(c, "key2", outLogs[0].Contents[4].Key)
	require.Equal(c, "2017xxxxxx]yyyy\nzzzz\n\t你好", outLogs[0].Contents[4].Value)
	require.Equal(c, "key3", outLogs[0].Contents[5].Key)
	require.Equal(c, "你好", outLogs[0].Contents[5].Value)
	require.Equal(c, "key4", outLogs[0].Contents[6].Key)
	require.Equal(c, "[hello", outLogs[0].Contents[6].Value)
}
