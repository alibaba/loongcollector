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

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"github.com/pingcap/check"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
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

// stdoutCapture keeps the observed log order across incremental reads in parser tests.
type stdoutCapture struct {
	pipeline.PipelineContext
	observed []*models.Log
}

func newStdoutCapture() *stdoutCapture {
	return &stdoutCapture{PipelineContext: helper.NewObservePipelineContext(1024)}
}

func (c *stdoutCapture) logs() []*models.Log {
	for _, group := range c.Collector().ToArray() {
		for _, event := range group.Events {
			c.observed = append(c.observed, event.(*models.Log))
		}
	}
	return c.observed
}

func (c *stdoutCapture) reset() {
	c.Collector().ToArray()
	c.observed = nil
}

type inputProcessorTestSuite struct {
	context          helper.LocalContext
	collector        *stdoutCapture
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
	s.collector = newStdoutCapture()
	s.context.InitContext("project", "logstore", "config")
}

func (s *inputProcessorTestSuite) TearDownTest(c *check.C) {

}

func (s *inputProcessorTestSuite) TestNormal(c *check.C) {
	processor := NewDockerStdoutProcessor(nil, time.Duration(0), 0, 512*1024, true, true, &s.context, s.collector.Collector(), s.tag, s.source)
	bytes := []byte(s.allLog)
	str := util.ZeroCopyBytesToString(bytes)
	n := processor.Process(bytes, time.Duration(0))
	c.Assert(n, check.Equals, len(s.allLog))
	c.Assert(len(s.collector.logs()), check.Equals, len(s.allLogContent))
	for index, content := range s.normalContent {

		c.Assert(util.ZeroCopyBytesToString(s.collector.logs()[index].GetBody()), check.Equals, content)
		if s.stdoutFlag[index] {
			c.Assert(s.collector.logs()[index].GetIndices().Get("_source_").(string), check.Equals, "stdout")
		} else {
			c.Assert(s.collector.logs()[index].GetIndices().Get("_source_").(string), check.Equals, "stderr")
		}
		value := s.collector.logs()[index].GetIndices().Get("_time_").(string)
		if index < 2 {
			c.Assert(value, check.Equals, "2018-05-16T06:28:41.2195434Z")
		} else {
			c.Assert(value, check.Equals, "2017-09-12T22:32:21.212861448Z")
		}
		c.Assert(util.IsSafeString(str, value), check.IsTrue)
		log := s.collector.logs()[index]
		c.Assert(log.GetIndices().Get("container_id"), check.Equals, "id")
		c.Assert(log.GetIndices().Get("container"), check.Equals, "test")
		key, ok := models.FirstLogContentKey(log.GetIndices())
		c.Assert(ok, check.IsTrue)
		c.Assert(key, check.Equals, models.BodyKey)
	}
}

func (s *inputProcessorTestSuite) TestStreamAllowed(c *check.C) {
	stdoutAllowed := true
	stderrAllowed := false
	// docker
	{
		// stdout
		{
			// 合法的json
			s.collector.reset()
			s.context.InitContext("project", "logstore", "config")

			splitedlogNoNewLine := `{"log":"123456","stream":"stdout", "time":"2018-05-16T06:28:41.2195434Z"}`
			processor := NewDockerStdoutProcessor(nil, time.Second, 0, 512*1024, stdoutAllowed, stderrAllowed, &s.context, s.collector.Collector(), s.tag, s.source)
			splitedlogNoNewLineBytes := []byte(splitedlogNoNewLine)
			processor.Process(splitedlogNoNewLineBytes, time.Duration(0))

			c.Assert(len(s.collector.logs()), check.Equals, 1)

			c.Assert(util.ZeroCopyBytesToString(s.collector.logs()[0].GetBody()), check.Equals, "123456")
			c.Assert(s.collector.logs()[0].GetIndices().Get("_time_").(string), check.Equals, "2018-05-16T06:28:41.2195434Z")
			c.Assert(s.collector.logs()[0].GetIndices().Get("_source_").(string), check.Equals, "stdout")

			// 非法的json
			s.collector.reset()
			s.context.InitContext("project", "logstore", "config")
			splitedlogNoNewLine = `{"log":"123456","stream":"stdout", "time":"2018-05-16`
			processor = NewDockerStdoutProcessor(nil, time.Second, 0, 512*1024, stdoutAllowed, stderrAllowed, &s.context, s.collector.Collector(), s.tag, s.source)
			splitedlogNoNewLineBytes = []byte(splitedlogNoNewLine)
			processor.Process(splitedlogNoNewLineBytes, time.Duration(0))

			c.Assert(len(s.collector.logs()), check.Equals, 1)

			c.Assert(util.ZeroCopyBytesToString(s.collector.logs()[0].GetBody()), check.Equals, `{"log":"123456","stream":"stdout", "time":"2018-05-16`)
			c.Assert(s.collector.logs()[0].GetIndices().Get("_time_").(string), check.Equals, "")
			c.Assert(s.collector.logs()[0].GetIndices().Get("_source_").(string), check.Equals, "")
		}
		// stderr
		{
			// 合法的json
			s.collector.reset()
			s.context.InitContext("project", "logstore", "config")

			splitedlogNoNewLine := `{"log":"123456","stream":"stderr", "time":"2018-05-16T06:28:41.2195434Z"}`
			processor := NewDockerStdoutProcessor(nil, time.Second, 0, 512*1024, stdoutAllowed, stderrAllowed, &s.context, s.collector.Collector(), s.tag, s.source)
			splitedlogNoNewLineBytes := []byte(splitedlogNoNewLine)
			processor.Process(splitedlogNoNewLineBytes, time.Duration(0))
			c.Assert(len(s.collector.logs()), check.Equals, 0)

			// 非法的json
			s.collector.reset()
			s.context.InitContext("project", "logstore", "config")

			splitedlogNoNewLine = `{"log":"123456","stream":"stderr", "time":"2018-05-16T06:28`
			processor = NewDockerStdoutProcessor(nil, time.Second, 0, 512*1024, stdoutAllowed, stderrAllowed, &s.context, s.collector.Collector(), s.tag, s.source)
			splitedlogNoNewLineBytes = []byte(splitedlogNoNewLine)
			processor.Process(splitedlogNoNewLineBytes, time.Duration(0))

			c.Assert(len(s.collector.logs()), check.Equals, 1)

			c.Assert(util.ZeroCopyBytesToString(s.collector.logs()[0].GetBody()), check.Equals, `{"log":"123456","stream":"stderr", "time":"2018-05-16T06:28`)
			c.Assert(s.collector.logs()[0].GetIndices().Get("_time_").(string), check.Equals, "")
			c.Assert(s.collector.logs()[0].GetIndices().Get("_source_").(string), check.Equals, "")
		}
	}
	// containerd
	{
		// stdout
		{
			// 合法的containerd日志
			s.collector.reset()
			s.context.InitContext("project", "logstore", "config")

			splitedlogNoNewLine := `2018-05-16T06:28:41.2195434Z stdout F 123456`
			processor := NewDockerStdoutProcessor(nil, time.Second, 0, 512*1024, stdoutAllowed, stderrAllowed, &s.context, s.collector.Collector(), s.tag, s.source)
			splitedlogNoNewLineBytes := []byte(splitedlogNoNewLine)
			processor.Process(splitedlogNoNewLineBytes, time.Duration(0))

			c.Assert(len(s.collector.logs()), check.Equals, 1)

			c.Assert(util.ZeroCopyBytesToString(s.collector.logs()[0].GetBody()), check.Equals, "123456")
			c.Assert(s.collector.logs()[0].GetIndices().Get("_time_").(string), check.Equals, "2018-05-16T06:28:41.2195434Z")
			c.Assert(s.collector.logs()[0].GetIndices().Get("_source_").(string), check.Equals, "stdout")

			// 非法的containerd日志
			s.collector.reset()
			s.context.InitContext("project", "logstore", "config")

			splitedlogNoNewLine = `2018-05-16T06:28:41.2195434Z stdout`
			processor = NewDockerStdoutProcessor(nil, time.Second, 0, 512*1024, stdoutAllowed, stderrAllowed, &s.context, s.collector.Collector(), s.tag, s.source)
			splitedlogNoNewLineBytes = []byte(splitedlogNoNewLine)
			processor.Process(splitedlogNoNewLineBytes, time.Duration(0))

			c.Assert(len(s.collector.logs()), check.Equals, 1)

			c.Assert(util.ZeroCopyBytesToString(s.collector.logs()[0].GetBody()), check.Equals, `2018-05-16T06:28:41.2195434Z stdout`)
			c.Assert(s.collector.logs()[0].GetIndices().Get("_time_").(string), check.Equals, "")
			c.Assert(s.collector.logs()[0].GetIndices().Get("_source_").(string), check.Equals, "")
		}
		// stderr
		{
			// 合法的containerd日志
			s.collector.reset()
			s.context.InitContext("project", "logstore", "config")

			splitedlogNoNewLine := `2018-05-16T06:28:41.2195434Z stderr F 123456`
			processor := NewDockerStdoutProcessor(nil, time.Second, 0, 512*1024, stdoutAllowed, stderrAllowed, &s.context, s.collector.Collector(), s.tag, s.source)
			splitedlogNoNewLineBytes := []byte(splitedlogNoNewLine)
			processor.Process(splitedlogNoNewLineBytes, time.Duration(0))
			c.Assert(len(s.collector.logs()), check.Equals, 0)

			// 非法的containerd日志
			s.collector.reset()
			s.context.InitContext("project", "logstore", "config")

			splitedlogNoNewLine = `2018-05-16T06:28:41.2195434Z stderr`
			processor = NewDockerStdoutProcessor(nil, time.Second, 0, 512*1024, stdoutAllowed, stderrAllowed, &s.context, s.collector.Collector(), s.tag, s.source)
			splitedlogNoNewLineBytes = []byte(splitedlogNoNewLine)
			processor.Process(splitedlogNoNewLineBytes, time.Duration(0))

			c.Assert(len(s.collector.logs()), check.Equals, 1)

			c.Assert(util.ZeroCopyBytesToString(s.collector.logs()[0].GetBody()), check.Equals, `2018-05-16T06:28:41.2195434Z stderr`)
			c.Assert(s.collector.logs()[0].GetIndices().Get("_time_").(string), check.Equals, "")
			c.Assert(s.collector.logs()[0].GetIndices().Get("_source_").(string), check.Equals, "")
		}
	}
}

func (s *inputProcessorTestSuite) TestSplitedLine(c *check.C) {
	processor := NewDockerStdoutProcessor(nil, time.Second, 0, 512*1024, true, true, &s.context, s.collector.Collector(), s.tag, s.source)
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

	c.Assert(len(s.collector.logs()), check.Equals, 1)
	value := util.ZeroCopyBytesToString(s.collector.logs()[0].GetBody())
	c.Assert(len(value), check.Equals, 1+2+3)
	c.Assert(value, check.Equals, "122333")
	c.Assert(util.IsSafeString(str1, value), check.IsTrue)
	c.Assert(util.IsSafeString(str2, value), check.IsTrue)
	c.Assert(util.IsSafeString(str3, value), check.IsTrue)

	nTimeout := processor.Process(splitedlog1Bytes, time.Minute)
	c.Assert(nTimeout, check.Equals, len(splitedlog1))
	c.Assert(len(s.collector.logs()), check.Equals, 2)
	value = util.ZeroCopyBytesToString(s.collector.logs()[1].GetBody())
	c.Assert(value, check.Equals, "1")
	c.Assert(util.IsSafeString(str1, value), check.IsTrue)
}

func (s *inputProcessorTestSuite) TestError(c *check.C) {
	processor := NewDockerStdoutProcessor(nil, time.Duration(0), 0, 512*1024, true, true, &s.context, s.collector.Collector(), s.tag, s.source)

	bytes := []byte(logErrorJSON)
	str := util.ZeroCopyBytesToString(bytes)

	n := processor.Process(bytes, time.Duration(0))
	c.Assert(n, check.Equals, len(logErrorJSON))

	value := util.ZeroCopyBytesToString(s.collector.logs()[0].GetBody())
	c.Assert(value, check.Equals, logErrorJSON)
	c.Assert(util.IsSafeString(value, str), check.IsTrue)

	bytes = []byte(logErrorJSON + "\n")
	str = util.ZeroCopyBytesToString(bytes)
	n = processor.Process(bytes, time.Duration(0))
	c.Assert(n, check.Equals, len(logErrorJSON)+1)

	value = util.ZeroCopyBytesToString(s.collector.logs()[1].GetBody())
	c.Assert(value, check.Equals, logErrorJSON)
	c.Assert(util.IsSafeString(value, str), check.IsTrue)

	bytes = []byte(logErrorContainerd1)
	str = util.ZeroCopyBytesToString(bytes)
	n = processor.Process(bytes, time.Duration(0))
	c.Assert(n, check.Equals, len(logErrorContainerd1))

	value = util.ZeroCopyBytesToString(s.collector.logs()[2].GetBody())
	c.Assert(value, check.Equals, logErrorContainerd1)
	c.Assert(util.IsSafeString(value, str), check.IsTrue)

	bytes = []byte(logErrorContainerd1 + "\n")
	str = util.ZeroCopyBytesToString(bytes)
	n = processor.Process(bytes, time.Duration(0))
	c.Assert(n, check.Equals, len(logErrorContainerd1)+1)

	value = util.ZeroCopyBytesToString(s.collector.logs()[3].GetBody())
	c.Assert(value, check.Equals, logErrorContainerd1)
	c.Assert(util.IsSafeString(value, str), check.IsTrue)

	bytes = []byte(logErrorContainerd2 + "\n")
	str = util.ZeroCopyBytesToString(bytes)
	n = processor.Process(bytes, time.Duration(0))
	c.Assert(n, check.Equals, len(logErrorContainerd2)+1)

	value = util.ZeroCopyBytesToString(s.collector.logs()[4].GetBody())
	c.Assert(value, check.Equals, logErrorContainerd2)
	c.Assert(util.IsSafeString(value, str), check.IsTrue)

	bytes = []byte(logErrorContainerd3 + "\n")
	str = util.ZeroCopyBytesToString(bytes)
	n = processor.Process(bytes, time.Duration(0))
	c.Assert(n, check.Equals, len(logErrorContainerd3)+1)

	value = util.ZeroCopyBytesToString(s.collector.logs()[5].GetBody())
	c.Assert(value, check.Equals, logErrorContainerd3)
	c.Assert(util.IsSafeString(value, str), check.IsTrue)
}

func (s *inputProcessorTestSuite) TestMultiLine(c *check.C) {
	processor := NewDockerStdoutProcessor(regexp.MustCompile(`\d+-\d+-\d+.*`), time.Second, 12, 512*1024, true, true, &s.context, s.collector.Collector(), s.tag, s.source)

	bytes := []byte(s.allLog)
	str := util.ZeroCopyBytesToString(bytes)
	n := processor.Process(bytes, time.Second*time.Duration(0))
	c.Assert(n, check.Equals, s.multiLineLen)

	c.Assert(len(s.collector.logs()), check.Equals, 4)
	value0 := util.ZeroCopyBytesToString(s.collector.logs()[0].GetBody())
	c.Assert(value0, check.Equals, context1+"\n"+context2)
	c.Assert(util.IsSafeString(value0, str), check.IsTrue)

	value1 := util.ZeroCopyBytesToString(s.collector.logs()[1].GetBody())
	c.Assert(value1, check.Equals, context3)
	c.Assert(util.IsSafeString(value1, str), check.IsTrue)

	value2 := util.ZeroCopyBytesToString(s.collector.logs()[2].GetBody())
	c.Assert(value2, check.Equals, context4)
	c.Assert(util.IsSafeString(value2, str), check.IsTrue)

	value3 := util.ZeroCopyBytesToString(s.collector.logs()[3].GetBody())
	c.Assert(value3, check.Equals, context5)
	c.Assert(util.IsSafeString(value3, str), check.IsTrue)
}

func (s *inputProcessorTestSuite) TestMultiLineTimeout(c *check.C) {
	processor := NewDockerStdoutProcessor(regexp.MustCompile(`\d+-\d+-\d+.*`), time.Second, 12, 512*1024, true, true, &s.context, s.collector.Collector(), s.tag, s.source)
	lastLine := strings.LastIndexByte(s.allLog, '\n')
	lastLine = strings.LastIndexByte(s.allLog[0:lastLine], '\n')
	originBytes := []byte(s.allLog)
	str := util.ZeroCopyBytesToString(originBytes)
	bytes := originBytes[0 : lastLine+1]

	n := processor.Process(bytes, time.Second*time.Duration(0))
	c.Assert(n, check.Equals, lastLine+1)
	c.Assert(len(s.collector.logs()), check.Equals, 4)

	value0 := util.ZeroCopyBytesToString(s.collector.logs()[0].GetBody())
	c.Assert(value0, check.Equals, context1+"\n"+context2)
	c.Assert(util.IsSafeString(value0, str), check.IsTrue)

	value1 := util.ZeroCopyBytesToString(s.collector.logs()[1].GetBody())
	c.Assert(value1, check.Equals, context3)
	c.Assert(util.IsSafeString(value1, str), check.IsTrue)

	value2 := util.ZeroCopyBytesToString(s.collector.logs()[2].GetBody())
	c.Assert(value2, check.Equals, context4)
	c.Assert(util.IsSafeString(value2, str), check.IsTrue)

	value3 := util.ZeroCopyBytesToString(s.collector.logs()[3].GetBody())
	c.Assert(value3, check.Equals, context5)
	c.Assert(util.IsSafeString(value3, str), check.IsTrue)

	bytes = originBytes[lastLine+1:]
	n = processor.Process(bytes, time.Second*time.Duration(0))
	c.Assert(n, check.Equals, len(s.allLog)-lastLine-1)
	c.Assert(len(s.collector.logs()), check.Equals, 4)

	processor.Process(nil, time.Second*time.Duration(2))
	c.Assert(len(s.collector.logs()), check.Equals, 5)
	value4 := util.ZeroCopyBytesToString(s.collector.logs()[4].GetBody())
	c.Assert(value4, check.Equals, context6)
	c.Assert(util.IsSafeString(value3, str), check.IsTrue)
}

func (s *inputProcessorTestSuite) TestMultiLineMaxLength(c *check.C) {
	processor := NewDockerStdoutProcessor(regexp.MustCompile(`\d+-\d+-\d+.*`), time.Second, 12, 512*1056, true, true, &s.context, s.collector.Collector(), s.tag, s.source)
	bytes := make([]byte, 3000)
	str := util.ZeroCopyBytesToString(bytes)
	content := util.RandomString(1024)
	for i := 0; i < 513; i++ {
		realLine := fmt.Sprintf("2017-09-12T22:32:21.212861448Z stdout [%d] %s\n", i, content)
		copy(bytes, realLine)
		n := processor.Process(bytes[:len(realLine)], time.Second*time.Duration(0))
		c.Assert(n, check.Equals, len(realLine))
		if i != 512 {
			c.Assert(len(s.collector.logs()), check.Equals, 0)
			c.Assert(util.IsSafeString(util.ZeroCopyBytesToString(processor.lastLogs[i].Content), str), check.IsTrue)
		}
	}

	c.Assert(len(s.collector.logs()), check.Equals, 1)
	value := util.ZeroCopyBytesToString(s.collector.logs()[0].GetBody())
	c.Assert(len(value), check.Greater, 500*1000)
	c.Assert(util.IsSafeString(value, str), check.IsTrue)
}

func (s *inputProcessorTestSuite) TestMultiLineError(c *check.C) {
	processor := NewDockerStdoutProcessor(regexp.MustCompile(`\d+-\d+-\d+.*`), time.Second, 12, 512*1024, true, true, &s.context, s.collector.Collector(), s.tag, s.source)

	bytes := []byte(logErrorJSON)
	str := util.ZeroCopyBytesToString(bytes)
	n := processor.Process(bytes, time.Second*time.Duration(0))
	c.Assert(n, check.Equals, len(logErrorJSON))

	c.Assert(len(s.collector.logs()), check.Equals, 1)
	value := util.ZeroCopyBytesToString(s.collector.logs()[0].GetBody())
	c.Assert(value, check.Equals, logErrorJSON)
	c.Assert(util.IsSafeString(value, str), check.IsTrue)
}

func (s *inputProcessorTestSuite) TestBigLine(c *check.C) {
	bigline, _ := os.ReadFile("./big_data.json")
	if len(bigline) == 0 {
		return
	}
	processor := NewDockerStdoutProcessor(nil, time.Duration(0), 0, 512*1024, true, true, &s.context, s.collector.Collector(), s.tag, s.source)
	for i := 0; i < 1000; i++ {
		processor.Process(bigline, time.Second*time.Duration(0))
		c.Assert(util.IsSafeString(util.ZeroCopyBytesToString(s.collector.logs()[i].GetBody()), util.ZeroCopyBytesToString(bigline)), check.IsTrue)
	}
	c.Assert(len(s.collector.logs()), check.Equals, 1000)
}

func TestParseCRILine(t *testing.T) {
	var context helper.LocalContext
	collector := newStdoutCapture()
	tags := map[string]string{
		"container":    "test",
		"container_id": "id",
	}
	source := "ABCEDFG1234567890-"

	duration := time.Second * time.Duration(0)
	assertBody := func(log *models.Log, value string) {
		require.Equal(t, value, string(log.GetBody()))
	}

	// Single line.
	{
		bytes := make([]byte, 100)
		str := util.ZeroCopyBytesToString(bytes)

		processor := NewDockerStdoutProcessor(nil, time.Duration(0),
			0, 512*1024, true, true, &context, collector.Collector(), tags, source)

		single := "2021-07-13T16:32:21.212861448Z stdout F full line\n"
		copy(bytes, single)
		processor.Process(bytes[:len(single)], duration)
		require.Equal(t, len(collector.logs()), 1)
		assertBody(collector.logs()[0], "full line")
		assert.True(t, util.IsSafeString(util.ZeroCopyBytesToString(collector.logs()[0].GetBody()), str))

		single = "2021-07-13T16:32:21.212861448Z stdout P partial line:\n"
		copy(bytes, single)
		processor.Process(bytes[:len(single)], duration)
		assert.True(t, util.IsSafeString(util.ZeroCopyBytesToString(processor.lastLogs[0].Content), str))

		single = "2021-07-13T16:32:21.212861448Z stdout F full line of partial line\n"
		copy(bytes, single)
		processor.Process(bytes[:len(single)], duration)
		require.Equal(t, len(collector.logs()), 2)
		assertBody(collector.logs()[1], "partial line:full line of partial line")
		assert.True(t, util.IsSafeString(util.ZeroCopyBytesToString(collector.logs()[1].GetBody()), str))
	}

	// Multiple lines.
	collector.reset()
	{
		bytes := make([]byte, 100)
		str := util.ZeroCopyBytesToString(bytes)

		timeoutDuration := time.Duration(1) * time.Second
		processor := NewDockerStdoutProcessor(
			regexp.MustCompile(`\d+-\d+-\d+.*`), timeoutDuration,
			10, 512*1024, true, true, &context, collector.Collector(), tags, source)

		single := "2021-07-13T16:32:21.212861448Z stdout F 2021-07-13 full line line 1\n"
		copy(bytes, single)

		processor.Process(bytes[:len(single)], duration)
		assert.True(t, util.IsSafeString(util.ZeroCopyBytesToString(processor.lastLogs[0].Content), str))

		single = "2021-07-13T16:32:21.212861448Z stdout F   full line line end\n"
		copy(bytes, single)
		processor.Process(bytes[:len(single)], duration)
		require.Equal(t, len(collector.logs()), 0)
		assert.True(t, util.IsSafeString(util.ZeroCopyBytesToString(processor.lastLogs[1].Content), str))

		single = "2021-07-13T16:32:21.212861448Z stdout P 2021-07-13 partial line line 1 partial\n"
		copy(bytes, single)
		processor.Process(bytes[:len(single)], duration)
		require.Equal(t, len(collector.logs()), 1)
		assertBody(collector.logs()[0], "2021-07-13 full line line 1\n  full line line end")
		assert.True(t, util.IsSafeString(util.ZeroCopyBytesToString(collector.logs()[0].GetBody()), str))
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

		require.Equal(t, len(collector.logs()), 1)

		processor.Process([]byte(""), timeoutDuration+time.Microsecond)
		require.Equal(t, len(collector.logs()), 2)
		assertBody(collector.logs()[1],
			"2021-07-13 partial line line 1 partial  partial line line 1 full\n"+
				"  partial line line 2 partial  partial line line 2 full")
		assert.True(t, util.IsSafeString(util.ZeroCopyBytesToString(collector.logs()[1].GetBody()), str))
	}
}

func TestSingleLineChangeBlock(t *testing.T) {
	var context helper.LocalContext
	collector := newStdoutCapture()
	tags := map[string]string{
		"container":    "test",
		"container_id": "id",
	}
	source := "ABCEDFG1234567890-"
	processor := NewDockerStdoutProcessor(nil, time.Duration(0),
		0, 512*1024, true, true, &context, collector.Collector(), tags, source)

	// valid single line when change block
	{
		str := "{\"log\":\"1:M 09 Nov 13:27:36.276 # User requested shutdown...\\n\",\"stream\":\"stdout\", \"time\":\"2018-05-16T06:28:41.2195434Z\"}\n"
		dockerSingleLine := util.ZeroCopyStringToBytes(str)
		assert.Equal(t, processor.Process(dockerSingleLine, time.Hour), len(dockerSingleLine))
		value := util.ZeroCopyBytesToString(collector.logs()[0].GetBody())
		assert.Equal(t, value, "1:M 09 Nov 13:27:36.276 # User requested shutdown...")
		assert.True(t, util.IsSafeString(str, value))

		assert.Equal(t, util.ZeroCopyBytesToString(collector.logs()[0].GetBody()), "1:M 09 Nov 13:27:36.276 # User requested shutdown...")

		str = "2021-07-13T16:32:21.212861448Z stdout F full line line end\n"
		contianerdSingleLine := util.ZeroCopyStringToBytes(str)
		assert.Equal(t, processor.Process(contianerdSingleLine, time.Hour), len(contianerdSingleLine))
		value = util.ZeroCopyBytesToString(collector.logs()[1].GetBody())
		assert.Equal(t, value, "full line line end")
		assert.True(t, util.IsSafeString(str, value))
		collector.reset()
	}

	// valid split line when change block
	{
		part1 := []byte("2021-07-13T16:32:21.212861448Z stdout P partial line:\n")
		str := util.ZeroCopyBytesToString(part1)
		assert.Equal(t, processor.Process(part1, 0), len(part1))
		assert.Equal(t, string(processor.lastLogs[0].Content), "partial line:")
		assert.Equal(t, len(collector.logs()), 0)
		part1[38] -= 10
		assert.Equal(t, processor.Process(part1, 0), len(part1))
		assert.Equal(t, len(collector.logs()), 1)
		value := util.ZeroCopyBytesToString(collector.logs()[0].GetBody())
		assert.Equal(t, value, "partial line:partial line:")
		assert.True(t, util.IsSafeString(str, value))
		collector.reset()
	}

	// valid containerd multi line when change block
	{
		processor := NewDockerStdoutProcessor(regexp.MustCompile(`^\d+-\d+-\d+.*`), time.Duration(0),
			10, 512*1024, true, true, &context, collector.Collector(), tags, source)
		part1 := []byte("2017-09-12T22:32:21.212861448Z stderr 2017-09-12 22:32:21.212 [INFO][88] exception 1: \n")
		str := util.ZeroCopyBytesToString(part1)

		assert.Equal(t, processor.Process(part1, 0), len(part1))
		assert.Equal(t, len(collector.logs()), 0)
		part1[38] += 40
		assert.Equal(t, processor.Process(part1, 0), len(part1))
		assert.Equal(t, len(collector.logs()), 0)
		part1[38] -= 40
		assert.Equal(t, processor.Process(part1, 0), len(part1))
		assert.Equal(t, len(collector.logs()), 1)
		value := util.ZeroCopyBytesToString(collector.logs()[0].GetBody())
		assert.Equal(t, value, "2017-09-12 22:32:21.212 [INFO][88] exception 1: \nZ017-09-12 22:32:21.212 [INFO][88] exception 1: ")
		assert.True(t, util.IsSafeString(str, value))
		collector.reset()
	}

	// valid docker multi line when change block
	{
		processor := NewDockerStdoutProcessor(regexp.MustCompile(`^\d+-\d+-\d+.*`), time.Duration(0),
			10, 512*1024, true, true, &context, collector.Collector(), tags, source)
		part1 := []byte("{\"log\":\"2017-09-12 22:32:21.212 13:27:36.276 # User requested shutdown...\\n\",\"stream\":\"stdout\", \"time\":\"2018-05-16T06:28:41.2195434Z\"}\n")
		str := util.ZeroCopyBytesToString(part1)

		assert.Equal(t, processor.Process(part1, 0), len(part1))
		assert.Equal(t, len(collector.logs()), 0)
		part1[8] += 40
		assert.Equal(t, processor.Process(part1, 0), len(part1))
		assert.Equal(t, len(collector.logs()), 0)
		part1[8] -= 40
		assert.Equal(t, processor.Process(part1, 0), len(part1))
		assert.Equal(t, len(collector.logs()), 1)
		value := util.ZeroCopyBytesToString(collector.logs()[0].GetBody())
		assert.Equal(t, value, "2017-09-12 22:32:21.212 13:27:36.276 # User requested shutdown...\nZ017-09-12 22:32:21.212 13:27:36.276 # User requested shutdown...")
		assert.True(t, util.IsSafeString(str, value))
		collector.reset()
	}

}
