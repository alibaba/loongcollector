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
	"io/ioutil"
	"regexp"
	"strings"
	"testing"
	"time"

	"github.com/pingcap/check"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

// go test -check.f "inputProcessorTestSuite.*" -args -debug=true -console=true
var _ = check.Suite(&inputProcessorTestSuite{})

var log1 = `{"log":"1:M 09 Nov 13:27:36.276 # User requested shutdown...\n","stream":"stdout", "time":"2018-05-16T06:28:41.2195434Z"
`

// error json
var context1 = `{"log":"1:M 09 Nov 13:27:36.276 # User requested shutdown...\n","stream":"stdout", "time":"2018-05-16T06:28:41.2195434Z"`
var log2 = `{"log":"1:M 09 Nov 13:27:36.276 # User requested begin...\n","stream":"stderr", "time":"2018-05-16T06:28:41.2195434Z"}
`
var context2 = `1:M 09 Nov 13:27:36.276 # User requested begin...`
var log3 = `2017-09-12T22:32:21.212861448Z stdout 2017-09-12 22:32:21.212 [INFO][88] table.go 710: Invalidating dataplane cache
`
var context3 = `2017-09-12 22:32:21.212 [INFO][88] table.go 710: Invalidating dataplane cache`
var log4 = `2017-09-12T22:32:21.212861448Z stderr 2017-09-12 22:32:21.212 [INFO][88] table.go 710:
`
var context4 = `2017-09-12 22:32:21.212 [INFO][88] table.go 710:`
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
}

func (s *inputProcessorTestSuite) SetUpSuite(c *check.C) {
	fmt.Printf("##############SetUpSuite##################\n")
	s.tag = make(map[string]string)
	s.tag["container"] = "test"
	s.tag["container_id"] = "id"

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
	processor := NewDockerStdoutProcessor(nil, time.Duration(0), 0, 512*1024, true, true, &s.context, &s.collector, s.tag)

	n := processor.Process([]byte(s.allLog), time.Duration(0))
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
		if index < 2 {
			c.Assert(s.collector.Logs[index].Contents[1].GetValue(), check.Equals, "2018-05-16T06:28:41.2195434Z")
		} else {
			c.Assert(s.collector.Logs[index].Contents[1].GetValue(), check.Equals, "2017-09-12T22:32:21.212861448Z")
		}
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
	processor := NewDockerStdoutProcessor(nil, time.Second, 0, 512*1024, true, true, &s.context, &s.collector, s.tag)
	n1 := processor.Process([]byte(splitedlog1), time.Duration(0))
	n2 := processor.Process([]byte(splitedlog2), time.Duration(0))
	n3 := processor.Process([]byte(splitedlog3), time.Duration(0))
	c.Assert(n1, check.Equals, len(splitedlog1))
	c.Assert(n2, check.Equals, len(splitedlog2))
	c.Assert(n3, check.Equals, len(splitedlog3))
	c.Assert(len(s.collector.Logs), check.Equals, 1)
	fmt.Println(s.collector.Logs[0].Contents[0].GetValue())
	c.Assert(len(s.collector.Logs[0].Contents[0].GetValue()), check.Equals, 1+2+3)
	fmt.Println("TestSplitedLine : ", s.collector.Logs)
	nTimeout := processor.Process([]byte(splitedlog1), time.Minute)
	c.Assert(nTimeout, check.Equals, len(splitedlog1))
	c.Assert(len(s.collector.Logs), check.Equals, 2)
	c.Assert(len(s.collector.Logs[1].Contents[0].GetValue()), check.Equals, 1)
}

func (s *inputProcessorTestSuite) TestError(c *check.C) {
	processor := NewDockerStdoutProcessor(nil, time.Duration(0), 0, 512*1024, true, true, &s.context, &s.collector, s.tag)

	n := processor.Process([]byte(logErrorJSON), time.Duration(0))
	c.Assert(n, check.Equals, len(logErrorJSON))
	c.Assert(s.collector.Logs[0].Contents[0].GetKey(), check.Equals, "content")
	c.Assert(s.collector.Logs[0].Contents[1].GetKey(), check.Equals, "_time_")
	c.Assert(s.collector.Logs[0].Contents[2].GetKey(), check.Equals, "_source_")
	c.Assert(s.collector.Logs[0].Contents[0].GetValue(), check.Equals, logErrorJSON)

	n = processor.Process([]byte(logErrorJSON+"\n"), time.Duration(0))
	c.Assert(n, check.Equals, len(logErrorJSON)+1)
	c.Assert(s.collector.Logs[1].Contents[0].GetKey(), check.Equals, "content")
	c.Assert(s.collector.Logs[1].Contents[1].GetKey(), check.Equals, "_time_")
	c.Assert(s.collector.Logs[1].Contents[2].GetKey(), check.Equals, "_source_")
	c.Assert(s.collector.Logs[1].Contents[0].GetValue(), check.Equals, logErrorJSON)
}

func (s *inputProcessorTestSuite) TestMultiLine(c *check.C) {
	processor := NewDockerStdoutProcessor(regexp.MustCompile(`\d+-\d+-\d+.*`), time.Second, 12, 512*1024, true, true, &s.context, &s.collector, s.tag)

	n := processor.Process([]byte(s.allLog), time.Second*time.Duration(0))
	c.Assert(n, check.Equals, s.multiLineLen)

	c.Assert(len(s.collector.Logs), check.Equals, 4)
	c.Assert(s.collector.Logs[0].Contents[0].GetValue(), check.Equals, context1+"\n"+context2)
	c.Assert(s.collector.Logs[1].Contents[0].GetValue(), check.Equals, context3)
	c.Assert(s.collector.Logs[2].Contents[0].GetValue(), check.Equals, context4)
	c.Assert(s.collector.Logs[3].Contents[0].GetValue(), check.Equals, context5)
}

func (s *inputProcessorTestSuite) TestMultiLineTimeout(c *check.C) {
	processor := NewDockerStdoutProcessor(regexp.MustCompile(`\d+-\d+-\d+.*`), time.Second, 12, 512*1024, true, true, &s.context, &s.collector, s.tag)
	lastLine := strings.LastIndexByte(s.allLog, '\n')
	lastLine = strings.LastIndexByte(s.allLog[0:lastLine], '\n')
	n := processor.Process([]byte(s.allLog[0:lastLine+1]), time.Second*time.Duration(0))
	c.Assert(n, check.Equals, lastLine+1)
	c.Assert(len(s.collector.Logs), check.Equals, 4)
	c.Assert(s.collector.Logs[0].Contents[0].GetValue(), check.Equals, context1+"\n"+context2)
	c.Assert(s.collector.Logs[1].Contents[0].GetValue(), check.Equals, context3)
	c.Assert(s.collector.Logs[2].Contents[0].GetValue(), check.Equals, context4)
	c.Assert(s.collector.Logs[3].Contents[0].GetValue(), check.Equals, context5)
	n = processor.Process([]byte(s.allLog[lastLine+1:]), time.Second*time.Duration(0))
	c.Assert(n, check.Equals, len(s.allLog)-lastLine-1)
	c.Assert(len(s.collector.Logs), check.Equals, 4)
	processor.Process(nil, time.Second*time.Duration(2))
	c.Assert(len(s.collector.Logs), check.Equals, 5)
	c.Assert(s.collector.Logs[4].Contents[0].GetValue(), check.Equals, context6)
}

func (s *inputProcessorTestSuite) TestMultiLineMaxLength(c *check.C) {
	processor := NewDockerStdoutProcessor(regexp.MustCompile(`\d+-\d+-\d+.*`), time.Second, 12, 512*1024, true, true, &s.context, &s.collector, s.tag)

	content := util.RandomString(1024)
	for i := 0; i < 513; i++ {
		realLine := fmt.Sprintf("2017-09-12T22:32:21.212861448Z stdout [%d] %s\n", i, content)
		n := processor.Process([]byte(realLine), time.Second*time.Duration(0))
		c.Assert(n, check.Equals, len(realLine))
	}

	c.Assert(len(s.collector.Logs), check.Equals, 1)
	c.Assert(len(s.collector.Logs[0].Contents[0].GetValue()), check.Greater, 500*1000)
}

func (s *inputProcessorTestSuite) TestMultiLineError(c *check.C) {
	processor := NewDockerStdoutProcessor(regexp.MustCompile(`\d+-\d+-\d+.*`), time.Second, 12, 512*1024, true, true, &s.context, &s.collector, s.tag)

	n := processor.Process([]byte(logErrorJSON), time.Second*time.Duration(0))
	c.Assert(n, check.Equals, len(logErrorJSON))

	c.Assert(len(s.collector.Logs), check.Equals, 1)
	c.Assert(s.collector.Logs[0].Contents[0].GetValue(), check.Equals, logErrorJSON)
}

func (s *inputProcessorTestSuite) TestBigLine(c *check.C) {
	bigline, _ := ioutil.ReadFile("./big_data.json")
	if len(bigline) == 0 {
		return
	}
	processor := NewDockerStdoutProcessor(nil, time.Duration(0), 0, 512*1024, true, true, &s.context, &s.collector, s.tag)
	for i := 0; i < 1000; i++ {
		processor.Process(bigline, time.Second*time.Duration(0))
	}

	c.Assert(len(s.collector.Logs), check.Equals, 1000)
	fmt.Println(s.collector.Logs[0].Contents[0].GetValue())
}

func TestParseCRILine(t *testing.T) {
	var context helper.LocalContext
	var collector helper.LocalCollector
	tags := map[string]string{
		"container":    "test",
		"container_id": "id",
	}

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
		processor := NewDockerStdoutProcessor(nil, time.Duration(0),
			0, 512*1024, true, true, &context, &collector, tags)

		processor.Process([]byte("2021-07-13T16:32:21.212861448Z stdout F full line\n"), duration)
		require.Equal(t, len(collector.Logs), 1)
		assertKeyValue(collector.Logs[0], "content", "full line")

		processor.Process([]byte("2021-07-13T16:32:21.212861448Z stdout P partial line:\n"), duration)
		processor.Process([]byte("2021-07-13T16:32:21.212861448Z stdout F full line of partial line\n"), duration)
		require.Equal(t, len(collector.Logs), 2)
		assertKeyValue(collector.Logs[1], "content", "partial line:full line of partial line")
	}

	// Multiple lines.
	collector.Logs = nil
	{
		timeoutDuration := time.Duration(1) * time.Second
		processor := NewDockerStdoutProcessor(
			regexp.MustCompile(`\d+-\d+-\d+.*`), timeoutDuration,
			10, 512*1024, true, true, &context, &collector, tags)

		processor.Process([]byte("2021-07-13T16:32:21.212861448Z stdout F 2021-07-13 full line line 1\n"), duration)
		processor.Process([]byte("2021-07-13T16:32:21.212861448Z stdout F   full line line end\n"), duration)
		require.Equal(t, len(collector.Logs), 0)

		processor.Process([]byte("2021-07-13T16:32:21.212861448Z stdout P 2021-07-13 partial line line 1 partial\n"), duration)
		require.Equal(t, len(collector.Logs), 1)
		assertKeyValue(collector.Logs[0], "content", "2021-07-13 full line line 1\n  full line line end")

		processor.Process([]byte("2021-07-13T16:32:21.212861448Z stdout F   partial line line 1 full\n"), duration)
		processor.Process([]byte("2021-07-13T16:32:21.212861448Z stdout P   partial line line 2 partial\n"), duration)
		processor.Process([]byte("2021-07-13T16:32:21.212861448Z stdout F   partial line line 2 full\n"), duration)
		require.Equal(t, len(collector.Logs), 1)

		processor.Process([]byte(""), timeoutDuration+time.Microsecond)
		require.Equal(t, len(collector.Logs), 2)
		assertKeyValue(collector.Logs[1], "content",
			"2021-07-13 partial line line 1 partial  partial line line 1 full\n"+
				"  partial line line 2 partial  partial line line 2 full")
	}
}
