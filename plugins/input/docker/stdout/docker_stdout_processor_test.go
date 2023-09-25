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

package stdout

import (
	"fmt"
	"os"
	"regexp"
	"strings"
	"testing"
	"time"

	"github.com/pingcap/check"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

// go test -check.f "inputProcessorTestSuite.*" -args -debug=true -console=true
var _ = check.Suite(&inputProcessorTestSuite{})

func Test(t *testing.T) {
	logger.InitTestLogger()
	check.TestingT(t)
}

// error json
var context1 = `1:M 09 Nov 13:27:36.276 # User requested shutdown...`
var log1 = `{"log":"1:M 09 Nov 13:27:36.276 # User requested shutdown...\n","stream":"stdout", "time":"2018-05-16T06:28:41.2195434Z"}
`

var context2 = `1:M 09 Nov 13:27:36.276 # User requested begin...`
var log2 = `{"log":"1:M 09 Nov 13:27:36.276 # User requested begin...\n","stream":"stderr", "time":"2018-05-16T06:28:41.2195434Z"}
`

var context3 = `2017-09-12 22:32:21.212 [INFO][88] table.go 710: Invalidating dataplane cache`
var log3 = `2017-09-12T22:32:21.212861448Z stdout 2017-09-12 22:32:21.212 [INFO][88] table.go 710: Invalidating dataplane cache
`

var context4 = `2017-09-12 22:32:21.212 [INFO][88] table.go 710:`
var log4 = `2017-09-12T22:32:21.212861448Z stderr 2017-09-12 22:32:21.212 [INFO][88] table.go 710:
`

var log5 = `2017-09-12T22:32:21.212861448Z stderr 2017-09-12 22:32:21.212 [INFO][88] exception 1: 
`
var log6 = `2017-09-12T22:32:21.212861448Z stderr    java.io.xxx.exception1....
`
var log7 = `2017-09-12T22:32:21.212861448Z stderr    java.io.xxx.exception2....
`
var context5 = `2017-09-12 22:32:21.212 [INFO][88] exception 1: 
   java.io.xxx.exception1....
   java.io.xxx.exception2....`
var log8 = `2017-09-12T22:32:21.212861448Z stderr 2017-09-12 22:32:21.212 [INFO][88] exception 2: 
`
var log9 = `2017-09-12T22:32:21.212861448Z stderr    java.io.xxx.exception11....
`
var log10 = `2017-09-12T22:32:21.212861448Z stderr    java.io.xxx.exception22....
`
var context6 = `2017-09-12 22:32:21.212 [INFO][88] exception 2: 
   java.io.xxx.exception11....
   java.io.xxx.exception22....`

var logErrorJSON = `{"log":"1:M 09 Nov 13:27:36.276 # User requested shutdown...\n","stream":"stdout", "time":"2018-05-16T06:28:41.2195434Z"`

var logErrorContainerd1 = `2017-09-12T22:32:21.212861448Zstderr2017-09-1222:32:21.212[INFO][88]exception2:`
var logErrorContainerd2 = `2017-09-12T22:32:21.212861448Z stderr2017-09-1222:32:21.212[INFO][88]exception2:`
var logErrorContainerd3 = `2017-09-12T22:32:21.212861448Z stderr F2017-09-1222:32:21.212[INFO][88]exception2:`

var splitedlog1 = `{"log":"1","stream":"stdout", "time":"2018-05-16T06:28:41.2195434Z"}
`
var splitedlog2 = `{"log":"22","stream":"stdout", "time":"2018-05-16T06:28:41.2195434Z"}
`
var splitedlog3 = `{"log":"333\n","stream":"stdout", "time":"2018-05-16T06:28:41.2195434Z"}
`

type inputProcessorTestSuite struct {
	context          helper.LocalContext
	collector        helper.LocalCollector
	tag              map[string]string
	allLog           string
	allLogContent    []string
	normalContent    []string
	multiLineContent []string
	stdoutFlag       []bool
	multiLineLen     int
	source           string
}

func (s *inputProcessorTestSuite) SetUpSuite(c *check.C) {
	fmt.Printf("##############SetUpSuite##################\n")
	s.tag = make(map[string]string)
	s.tag["container"] = "test"
	s.tag["container_id"] = "id"
	s.source = "ABCEDFG1234567890-"

	s.normalContent = append(s.normalContent, context1)
	s.normalContent = append(s.normalContent, context2)
	s.normalContent = append(s.normalContent, context3)
	s.normalContent = append(s.normalContent, context4)

	s.multiLineContent = append(s.multiLineContent, context5)
	s.multiLineContent = append(s.multiLineContent, context6)

	s.allLog += log1
	s.allLogContent = append(s.allLogContent, log1)
	s.stdoutFlag = append(s.stdoutFlag, true)
	s.allLog += log2
	s.allLogContent = append(s.allLogContent, log2)
	s.stdoutFlag = append(s.stdoutFlag, false)
	s.allLog += log3
	s.allLogContent = append(s.allLogContent, log3)
	s.stdoutFlag = append(s.stdoutFlag, true)
	s.allLog += log4
	s.allLogContent = append(s.allLogContent, log4)
	s.stdoutFlag = append(s.stdoutFlag, false)
	s.allLog += log5
	s.allLogContent = append(s.allLogContent, log5)
	s.stdoutFlag = append(s.stdoutFlag, false)
	s.allLog += log6
	s.allLogContent = append(s.allLogContent, log6)
	s.stdoutFlag = append(s.stdoutFlag, false)
	s.allLog += log7
	s.allLogContent = append(s.allLogContent, log7)
	s.stdoutFlag = append(s.stdoutFlag, false)
	s.allLog += log8
	s.allLogContent = append(s.allLogContent, log8)
	s.stdoutFlag = append(s.stdoutFlag, false)
	s.allLog += log9
	s.allLogContent = append(s.allLogContent, log9)
	s.stdoutFlag = append(s.stdoutFlag, false)
	s.allLog += log10
	s.allLogContent = append(s.allLogContent, log10)
	s.stdoutFlag = append(s.stdoutFlag, false)
	s.multiLineLen = len(s.allLog)
}

func (s *inputProcessorTestSuite) TearDownSuite(c *check.C) {
	fmt.Printf("#############TearDownSuite##################\n")
}

func (s *inputProcessorTestSuite) SetUpTest(c *check.C) {
	s.collector.Logs = s.collector.Logs[:0]
	s.context.InitContext("project", "logstore", "config")
}

func (s *inputProcessorTestSuite) TearDownTest(c *check.C) {

}

func (s *inputProcessorTestSuite) TestNormal(c *check.C) {
	processor := NewDockerStdoutProcessor(nil, time.Duration(0), 0, 512*1024, true, true, &s.context, &s.collector, s.tag, s.source)
	bytes := []byte(s.allLog)
	str := util.ZeroCopyBytesToString(bytes)
	n := processor.Process(bytes, time.Duration(0))
	c.Assert(n, check.Equals, len(s.allLog))
	c.Assert(len(s.collector.Logs), check.Equals, len(s.allLogContent))
	for index, content := range s.normalContent {
		c.Assert(s.collector.Logs[index].Contents[0].GetKey(), check.Equals, "content")
		c.Assert(s.collector.Logs[index].Contents[1].GetKey(), check.Equals, "_time_")
		c.Assert(s.collector.Logs[index].Contents[2].GetKey(), check.Equals, "_source_")
		c.Assert(s.collector.Logs[index].Contents[0].GetValue(), check.Equals, content)
		if s.stdoutFlag[index] {
			c.Assert(s.collector.Logs[index].Contents[2].GetValue(), check.Equals, "stdout")
		} else {
			c.Assert(s.collector.Logs[index].Contents[2].GetValue(), check.Equals, "stderr")
		}
		value := s.collector.Logs[index].Contents[1].GetValue()
		if index < 2 {
			c.Assert(value, check.Equals, "2018-05-16T06:28:41.2195434Z")
		} else {
			c.Assert(value, check.Equals, "2017-09-12T22:32:21.212861448Z")
		}
		c.Assert(util.IsSafeString(str, value), check.IsTrue)
		switch s.collector.Logs[index].Contents[3].GetKey() {
		case "container_id":
			c.Assert(s.collector.Logs[index].Contents[3].GetKey(), check.Equals, "container_id")
			c.Assert(s.collector.Logs[index].Contents[3].GetValue(), check.Equals, "id")
			c.Assert(s.collector.Logs[index].Contents[4].GetKey(), check.Equals, "container")
			c.Assert(s.collector.Logs[index].Contents[4].GetValue(), check.Equals, "test")
		default:
			c.Assert(s.collector.Logs[index].Contents[4].GetKey(), check.Equals, "container_id")
			c.Assert(s.collector.Logs[index].Contents[4].GetValue(), check.Equals, "id")
			c.Assert(s.collector.Logs[index].Contents[3].GetKey(), check.Equals, "container")
			c.Assert(s.collector.Logs[index].Contents[3].GetValue(), check.Equals, "test")
		}
	}
}

func (s *inputProcessorTestSuite) TestSplitedLine(c *check.C) {
	processor := NewDockerStdoutProcessor(nil, time.Second, 0, 512*1024, true, true, &s.context, &s.collector, s.tag, s.source)
	splitedlog1Bytes := []byte(splitedlog1)
	str1 := util.ZeroCopyBytesToString(splitedlog1Bytes)
	splited2og1Bytes := []byte(splitedlog2)
	str2 := util.ZeroCopyBytesToString(splited2og1Bytes)
	splited3og1Bytes := []byte(splitedlog3)
	str3 := util.ZeroCopyBytesToString(splited3og1Bytes)

	n1 := processor.Process(splitedlog1Bytes, time.Duration(0))
	c.Assert(util.IsSafeString(str1, util.ZeroCopyBytesToString(processor.lastLogs[0].Content)), check.IsTrue)
	c.Assert(n1, check.Equals, len(splitedlog1))

	n2 := processor.Process(splited2og1Bytes, time.Duration(0))
	c.Assert(util.IsSafeString(str2, util.ZeroCopyBytesToString(processor.lastLogs[1].Content)), check.IsTrue)
	c.Assert(n2, check.Equals, len(splitedlog2))

	n3 := processor.Process(splited3og1Bytes, time.Duration(0))
	c.Assert(n3, check.Equals, len(splitedlog3))

	c.Assert(len(s.collector.Logs), check.Equals, 1)
	value := s.collector.Logs[0].Contents[0].GetValue()
	c.Assert(len(value), check.Equals, 1+2+3)
	c.Assert(value, check.Equals, "122333")
	c.Assert(util.IsSafeString(str1, value), check.IsTrue)
	c.Assert(util.IsSafeString(str2, value), check.IsTrue)
	c.Assert(util.IsSafeString(str3, value), check.IsTrue)

	nTimeout := processor.Process(splitedlog1Bytes, time.Minute)
	c.Assert(nTimeout, check.Equals, len(splitedlog1))
	c.Assert(len(s.collector.Logs), check.Equals, 2)
	value = s.collector.Logs[1].Contents[0].GetValue()
	c.Assert(value, check.Equals, "1")
	c.Assert(util.IsSafeString(str1, value), check.IsTrue)
}

func (s *inputProcessorTestSuite) TestError(c *check.C) {
	processor := NewDockerStdoutProcessor(nil, time.Duration(0), 0, 512*1024, true, true, &s.context, &s.collector, s.tag, s.source)

	bytes := []byte(logErrorJSON)
	str := util.ZeroCopyBytesToString(bytes)

	n := processor.Process(bytes, time.Duration(0))
	c.Assert(n, check.Equals, len(logErrorJSON))
	c.Assert(s.collector.Logs[0].Contents[0].GetKey(), check.Equals, "content")
	c.Assert(s.collector.Logs[0].Contents[1].GetKey(), check.Equals, "_time_")
	c.Assert(s.collector.Logs[0].Contents[2].GetKey(), check.Equals, "_source_")
	value := s.collector.Logs[0].Contents[0].GetValue()
	c.Assert(value, check.Equals, logErrorJSON)
	c.Assert(util.IsSafeString(value, str), check.IsTrue)

	bytes = []byte(logErrorJSON + "\n")
	str = util.ZeroCopyBytesToString(bytes)
	n = processor.Process(bytes, time.Duration(0))
	c.Assert(n, check.Equals, len(logErrorJSON)+1)
	c.Assert(s.collector.Logs[1].Contents[0].GetKey(), check.Equals, "content")
	c.Assert(s.collector.Logs[1].Contents[1].GetKey(), check.Equals, "_time_")
	c.Assert(s.collector.Logs[1].Contents[2].GetKey(), check.Equals, "_source_")
	value = s.collector.Logs[1].Contents[0].GetValue()
	c.Assert(value, check.Equals, logErrorJSON)
	c.Assert(util.IsSafeString(value, str), check.IsTrue)

	bytes = []byte(logErrorContainerd1)
	str = util.ZeroCopyBytesToString(bytes)
	n = processor.Process(bytes, time.Duration(0))
	c.Assert(n, check.Equals, len(logErrorContainerd1))
	c.Assert(s.collector.Logs[2].Contents[0].GetKey(), check.Equals, "content")
	c.Assert(s.collector.Logs[2].Contents[1].GetKey(), check.Equals, "_time_")
	c.Assert(s.collector.Logs[2].Contents[2].GetKey(), check.Equals, "_source_")
	value = s.collector.Logs[2].Contents[0].GetValue()
	c.Assert(value, check.Equals, logErrorContainerd1)
	c.Assert(util.IsSafeString(value, str), check.IsTrue)

	bytes = []byte(logErrorContainerd1 + "\n")
	str = util.ZeroCopyBytesToString(bytes)
	n = processor.Process(bytes, time.Duration(0))
	c.Assert(n, check.Equals, len(logErrorContainerd1)+1)
	c.Assert(s.collector.Logs[3].Contents[0].GetKey(), check.Equals, "content")
	c.Assert(s.collector.Logs[3].Contents[1].GetKey(), check.Equals, "_time_")
	c.Assert(s.collector.Logs[3].Contents[2].GetKey(), check.Equals, "_source_")
	value = s.collector.Logs[3].Contents[0].GetValue()
	c.Assert(value, check.Equals, logErrorContainerd1)
	c.Assert(util.IsSafeString(value, str), check.IsTrue)

	bytes = []byte(logErrorContainerd2 + "\n")
	str = util.ZeroCopyBytesToString(bytes)
	n = processor.Process(bytes, time.Duration(0))
	c.Assert(n, check.Equals, len(logErrorContainerd2)+1)
	c.Assert(s.collector.Logs[4].Contents[0].GetKey(), check.Equals, "content")
	c.Assert(s.collector.Logs[4].Contents[1].GetKey(), check.Equals, "_time_")
	c.Assert(s.collector.Logs[4].Contents[2].GetKey(), check.Equals, "_source_")
	value = s.collector.Logs[4].Contents[0].GetValue()
	c.Assert(value, check.Equals, logErrorContainerd2)
	c.Assert(util.IsSafeString(value, str), check.IsTrue)

	bytes = []byte(logErrorContainerd3 + "\n")
	str = util.ZeroCopyBytesToString(bytes)
	n = processor.Process(bytes, time.Duration(0))
	c.Assert(n, check.Equals, len(logErrorContainerd3)+1)
	c.Assert(s.collector.Logs[5].Contents[0].GetKey(), check.Equals, "content")
	c.Assert(s.collector.Logs[5].Contents[1].GetKey(), check.Equals, "_time_")
	c.Assert(s.collector.Logs[5].Contents[2].GetKey(), check.Equals, "_source_")
	value = s.collector.Logs[5].Contents[0].GetValue()
	c.Assert(value, check.Equals, logErrorContainerd3)
	c.Assert(util.IsSafeString(value, str), check.IsTrue)
}

func (s *inputProcessorTestSuite) TestMultiLine(c *check.C) {
	processor := NewDockerStdoutProcessor(regexp.MustCompile(`\d+-\d+-\d+.*`), time.Second, 12, 512*1024, true, true, &s.context, &s.collector, s.tag, s.source)

	bytes := []byte(s.allLog)
	str := util.ZeroCopyBytesToString(bytes)
	n := processor.Process(bytes, time.Second*time.Duration(0))
	c.Assert(n, check.Equals, s.multiLineLen)

	c.Assert(len(s.collector.Logs), check.Equals, 4)
	value0 := s.collector.Logs[0].Contents[0].GetValue()
	c.Assert(value0, check.Equals, context1+"\n"+context2)
	c.Assert(util.IsSafeString(value0, str), check.IsTrue)

	value1 := s.collector.Logs[1].Contents[0].GetValue()
	c.Assert(value1, check.Equals, context3)
	c.Assert(util.IsSafeString(value1, str), check.IsTrue)

	value2 := s.collector.Logs[2].Contents[0].GetValue()
	c.Assert(value2, check.Equals, context4)
	c.Assert(util.IsSafeString(value2, str), check.IsTrue)

	value3 := s.collector.Logs[3].Contents[0].GetValue()
	c.Assert(value3, check.Equals, context5)
	c.Assert(util.IsSafeString(value3, str), check.IsTrue)
}

func (s *inputProcessorTestSuite) TestMultiLineTimeout(c *check.C) {
	processor := NewDockerStdoutProcessor(regexp.MustCompile(`\d+-\d+-\d+.*`), time.Second, 12, 512*1024, true, true, &s.context, &s.collector, s.tag, s.source)
	lastLine := strings.LastIndexByte(s.allLog, '\n')
	lastLine = strings.LastIndexByte(s.allLog[0:lastLine], '\n')
	originBytes := []byte(s.allLog)
	str := util.ZeroCopyBytesToString(originBytes)
	bytes := originBytes[0 : lastLine+1]

	n := processor.Process(bytes, time.Second*time.Duration(0))
	c.Assert(n, check.Equals, lastLine+1)
	c.Assert(len(s.collector.Logs), check.Equals, 4)

	value0 := s.collector.Logs[0].Contents[0].GetValue()
	c.Assert(value0, check.Equals, context1+"\n"+context2)
	c.Assert(util.IsSafeString(value0, str), check.IsTrue)

	value1 := s.collector.Logs[1].Contents[0].GetValue()
	c.Assert(value1, check.Equals, context3)
	c.Assert(util.IsSafeString(value1, str), check.IsTrue)

	value2 := s.collector.Logs[2].Contents[0].GetValue()
	c.Assert(value2, check.Equals, context4)
	c.Assert(util.IsSafeString(value2, str), check.IsTrue)

	value3 := s.collector.Logs[3].Contents[0].GetValue()
	c.Assert(value3, check.Equals, context5)
	c.Assert(util.IsSafeString(value3, str), check.IsTrue)

	bytes = originBytes[lastLine+1:]
	n = processor.Process(bytes, time.Second*time.Duration(0))
	c.Assert(n, check.Equals, len(s.allLog)-lastLine-1)
	c.Assert(len(s.collector.Logs), check.Equals, 4)

	processor.Process(nil, time.Second*time.Duration(2))
	c.Assert(len(s.collector.Logs), check.Equals, 5)
	value4 := s.collector.Logs[4].Contents[0].GetValue()
	c.Assert(value4, check.Equals, context6)
	c.Assert(util.IsSafeString(value3, str), check.IsTrue)
}

func (s *inputProcessorTestSuite) TestMultiLineMaxLength(c *check.C) {
	processor := NewDockerStdoutProcessor(regexp.MustCompile(`\d+-\d+-\d+.*`), time.Second, 12, 512*1056, true, true, &s.context, &s.collector, s.tag, s.source)
	bytes := make([]byte, 3000)
	str := util.ZeroCopyBytesToString(bytes)
	content := util.RandomString(1024)
	for i := 0; i < 513; i++ {
		realLine := fmt.Sprintf("2017-09-12T22:32:21.212861448Z stdout [%d] %s\n", i, content)
		copy(bytes, realLine)
		n := processor.Process(bytes[:len(realLine)], time.Second*time.Duration(0))
		c.Assert(n, check.Equals, len(realLine))
		if i != 512 {
			c.Assert(len(s.collector.Logs), check.Equals, 0)
			c.Assert(util.IsSafeString(util.ZeroCopyBytesToString(processor.lastLogs[i].Content), str), check.IsTrue)
		}
	}

	c.Assert(len(s.collector.Logs), check.Equals, 1)
	value := s.collector.Logs[0].Contents[0].GetValue()
	c.Assert(len(value), check.Greater, 500*1000)
	c.Assert(util.IsSafeString(value, str), check.IsTrue)
}

func (s *inputProcessorTestSuite) TestMultiLineError(c *check.C) {
	processor := NewDockerStdoutProcessor(regexp.MustCompile(`\d+-\d+-\d+.*`), time.Second, 12, 512*1024, true, true, &s.context, &s.collector, s.tag, s.source)

	bytes := []byte(logErrorJSON)
	str := util.ZeroCopyBytesToString(bytes)
	n := processor.Process(bytes, time.Second*time.Duration(0))
	c.Assert(n, check.Equals, len(logErrorJSON))

	c.Assert(len(s.collector.Logs), check.Equals, 1)
	value := s.collector.Logs[0].Contents[0].GetValue()
	c.Assert(value, check.Equals, logErrorJSON)
	c.Assert(util.IsSafeString(value, str), check.IsTrue)
}

func (s *inputProcessorTestSuite) TestBigLine(c *check.C) {
	bigline, _ := os.ReadFile("./big_data.json")
	if len(bigline) == 0 {
		return
	}
	processor := NewDockerStdoutProcessor(nil, time.Duration(0), 0, 512*1024, true, true, &s.context, &s.collector, s.tag, s.source)
	for i := 0; i < 1000; i++ {
		processor.Process(bigline, time.Second*time.Duration(0))
		c.Assert(util.IsSafeString(s.collector.Logs[i].Contents[0].GetValue(), util.ZeroCopyBytesToString(bigline)), check.IsTrue)
	}
	c.Assert(len(s.collector.Logs), check.Equals, 1000)
}

func TestParseCRILine(t *testing.T) {
	var context helper.LocalContext
	var collector helper.LocalCollector
	tags := map[string]string{
		"container":    "test",
		"container_id": "id",
	}
	source := "ABCEDFG1234567890-"

	duration := time.Second * time.Duration(0)
	assertKeyValue := func(log *protocol.Log, key string, value string) {
		for _, c := range log.Contents {
			if c.Key == key {
				require.Equal(t, c.Value, value)
				return
			}
		}
		require.Fail(t, "NotFound: "+key)
	}

	// Single line.
	{
		bytes := make([]byte, 100)
		str := util.ZeroCopyBytesToString(bytes)

		processor := NewDockerStdoutProcessor(nil, time.Duration(0),
			0, 512*1024, true, true, &context, &collector, tags, source)

		single := "2021-07-13T16:32:21.212861448Z stdout F full line\n"
		copy(bytes, single)
		processor.Process(bytes[:len(single)], duration)
		require.Equal(t, len(collector.Logs), 1)
		assertKeyValue(collector.Logs[0], "content", "full line")
		assert.True(t, util.IsSafeString(collector.Logs[0].Contents[0].GetValue(), str))

		single = "2021-07-13T16:32:21.212861448Z stdout P partial line:\n"
		copy(bytes, single)
		processor.Process(bytes[:len(single)], duration)
		assert.True(t, util.IsSafeString(util.ZeroCopyBytesToString(processor.lastLogs[0].Content), str))

		single = "2021-07-13T16:32:21.212861448Z stdout F full line of partial line\n"
		copy(bytes, single)
		processor.Process(bytes[:len(single)], duration)
		require.Equal(t, len(collector.Logs), 2)
		assertKeyValue(collector.Logs[1], "content", "partial line:full line of partial line")
		assert.True(t, util.IsSafeString(collector.Logs[1].Contents[0].GetValue(), str))
	}

	// Multiple lines.
	collector.Logs = nil
	{
		bytes := make([]byte, 100)
		str := util.ZeroCopyBytesToString(bytes)

		timeoutDuration := time.Duration(1) * time.Second
		processor := NewDockerStdoutProcessor(
			regexp.MustCompile(`\d+-\d+-\d+.*`), timeoutDuration,
			10, 512*1024, true, true, &context, &collector, tags, source)

		single := "2021-07-13T16:32:21.212861448Z stdout F 2021-07-13 full line line 1\n"
		copy(bytes, single)

		processor.Process(bytes[:len(single)], duration)
		assert.True(t, util.IsSafeString(util.ZeroCopyBytesToString(processor.lastLogs[0].Content), str))

		single = "2021-07-13T16:32:21.212861448Z stdout F   full line line end\n"
		copy(bytes, single)
		processor.Process(bytes[:len(single)], duration)
		require.Equal(t, len(collector.Logs), 0)
		assert.True(t, util.IsSafeString(util.ZeroCopyBytesToString(processor.lastLogs[1].Content), str))

		single = "2021-07-13T16:32:21.212861448Z stdout P 2021-07-13 partial line line 1 partial\n"
		copy(bytes, single)
		processor.Process(bytes[:len(single)], duration)
		require.Equal(t, len(collector.Logs), 1)
		assertKeyValue(collector.Logs[0], "content", "2021-07-13 full line line 1\n  full line line end")
		assert.True(t, util.IsSafeString(collector.Logs[0].Contents[0].GetValue(), str))
		assert.Equal(t, len(processor.lastLogs), 1)
		assert.True(t, util.IsSafeString(util.ZeroCopyBytesToString(processor.lastLogs[0].Content), str))

		single = "2021-07-13T16:32:21.212861448Z stdout F   partial line line 1 full\n"
		copy(bytes, single)
		processor.Process(bytes[:len(single)], duration)
		assert.Equal(t, len(processor.lastLogs), 2)
		assert.True(t, util.IsSafeString(util.ZeroCopyBytesToString(processor.lastLogs[1].Content), str))

		single = "2021-07-13T16:32:21.212861448Z stdout P   partial line line 2 partial\n"
		copy(bytes, single)
		processor.Process(bytes[:len(single)], duration)
		assert.Equal(t, len(processor.lastLogs), 3)
		assert.True(t, util.IsSafeString(util.ZeroCopyBytesToString(processor.lastLogs[2].Content), str))

		single = "2021-07-13T16:32:21.212861448Z stdout F   partial line line 2 full\n"
		copy(bytes, single)
		processor.Process(bytes[:len(single)], duration)
		assert.Equal(t, len(processor.lastLogs), 4)
		assert.True(t, util.IsSafeString(util.ZeroCopyBytesToString(processor.lastLogs[3].Content), str))

		require.Equal(t, len(collector.Logs), 1)

		processor.Process([]byte(""), timeoutDuration+time.Microsecond)
		require.Equal(t, len(collector.Logs), 2)
		assertKeyValue(collector.Logs[1], "content",
			"2021-07-13 partial line line 1 partial  partial line line 1 full\n"+
				"  partial line line 2 partial  partial line line 2 full")
		assert.True(t, util.IsSafeString(collector.Logs[1].Contents[0].GetValue(), str))
	}
}

func TestSingleLineChangeBlock(t *testing.T) {
	var context helper.LocalContext
	var collector helper.LocalCollector
	tags := map[string]string{
		"container":    "test",
		"container_id": "id",
	}
	source := "ABCEDFG1234567890-"
	processor := NewDockerStdoutProcessor(nil, time.Duration(0),
		0, 512*1024, true, true, &context, &collector, tags, source)

	// valid single line when change block
	{
		str := "{\"log\":\"1:M 09 Nov 13:27:36.276 # User requested shutdown...\\n\",\"stream\":\"stdout\", \"time\":\"2018-05-16T06:28:41.2195434Z\"}\n"
		dockerSingleLine := util.ZeroCopyStringToBytes(str)
		assert.Equal(t, processor.Process(dockerSingleLine, time.Hour), len(dockerSingleLine))
		value := collector.Logs[0].Contents[0].GetValue()
		assert.Equal(t, value, "1:M 09 Nov 13:27:36.276 # User requested shutdown...")
		assert.True(t, util.IsSafeString(str, value))

		assert.Equal(t, collector.Logs[0].Contents[0].GetValue(), "1:M 09 Nov 13:27:36.276 # User requested shutdown...")

		str = "2021-07-13T16:32:21.212861448Z stdout F full line line end\n"
		contianerdSingleLine := util.ZeroCopyStringToBytes(str)
		assert.Equal(t, processor.Process(contianerdSingleLine, time.Hour), len(contianerdSingleLine))
		value = collector.Logs[1].Contents[0].GetValue()
		assert.Equal(t, value, "full line line end")
		assert.True(t, util.IsSafeString(str, value))
		collector.Logs = collector.Logs[:0]
	}

	// valid split line when change block
	{
		part1 := []byte("2021-07-13T16:32:21.212861448Z stdout P partial line:\n")
		str := util.ZeroCopyBytesToString(part1)
		assert.Equal(t, processor.Process(part1, 0), len(part1))
		assert.Equal(t, string(processor.lastLogs[0].Content), "partial line:")
		assert.Equal(t, len(collector.Logs), 0)
		part1[38] -= 10
		assert.Equal(t, processor.Process(part1, 0), len(part1))
		assert.Equal(t, len(collector.Logs), 1)
		value := collector.Logs[0].Contents[0].GetValue()
		assert.Equal(t, value, "partial line:partial line:")
		assert.True(t, util.IsSafeString(str, value))
		collector.Logs = collector.Logs[:0]
	}

	// valid containerd multi line when change block
	{
		processor := NewDockerStdoutProcessor(regexp.MustCompile(`^\d+-\d+-\d+.*`), time.Duration(0),
			10, 512*1024, true, true, &context, &collector, tags, source)
		part1 := []byte("2017-09-12T22:32:21.212861448Z stderr 2017-09-12 22:32:21.212 [INFO][88] exception 1: \n")
		str := util.ZeroCopyBytesToString(part1)

		assert.Equal(t, processor.Process(part1, 0), len(part1))
		assert.Equal(t, len(collector.Logs), 0)
		part1[38] += 40
		assert.Equal(t, processor.Process(part1, 0), len(part1))
		assert.Equal(t, len(collector.Logs), 0)
		part1[38] -= 40
		assert.Equal(t, processor.Process(part1, 0), len(part1))
		assert.Equal(t, len(collector.Logs), 1)
		value := collector.Logs[0].Contents[0].GetValue()
		assert.Equal(t, value, "2017-09-12 22:32:21.212 [INFO][88] exception 1: \nZ017-09-12 22:32:21.212 [INFO][88] exception 1: ")
		assert.True(t, util.IsSafeString(str, value))
		collector.Logs = collector.Logs[:0]
	}

	// valid docker multi line when change block
	{
		processor := NewDockerStdoutProcessor(regexp.MustCompile(`^\d+-\d+-\d+.*`), time.Duration(0),
			10, 512*1024, true, true, &context, &collector, tags, source)
		part1 := []byte("{\"log\":\"2017-09-12 22:32:21.212 13:27:36.276 # User requested shutdown...\\n\",\"stream\":\"stdout\", \"time\":\"2018-05-16T06:28:41.2195434Z\"}\n")
		str := util.ZeroCopyBytesToString(part1)

		assert.Equal(t, processor.Process(part1, 0), len(part1))
		assert.Equal(t, len(collector.Logs), 0)
		part1[8] += 40
		assert.Equal(t, processor.Process(part1, 0), len(part1))
		assert.Equal(t, len(collector.Logs), 0)
		part1[8] -= 40
		assert.Equal(t, processor.Process(part1, 0), len(part1))
		assert.Equal(t, len(collector.Logs), 1)
		value := collector.Logs[0].Contents[0].GetValue()
		assert.Equal(t, value, "2017-09-12 22:32:21.212 13:27:36.276 # User requested shutdown...\nZ017-09-12 22:32:21.212 13:27:36.276 # User requested shutdown...")
		assert.True(t, util.IsSafeString(str, value))
		collector.Logs = collector.Logs[:0]
	}

}
