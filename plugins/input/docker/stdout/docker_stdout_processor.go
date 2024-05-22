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
	"bytes"
	"errors"
	"regexp"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

var (
	delimiter         = []byte{' '}
	contianerdFullTag = []byte{'F'}
	contianerdPartTag = []byte{'P'}
	lineSuffix        = []byte{'\n'}
)

type DockerJSONLog struct {
	LogContent string `json:"log"`
	StreamType string `json:"stream"`
	Time       string `json:"time"`
}

type LogMessage struct {
	Time       string
	StreamType string
	Content    []byte
	Safe       bool
}

// safeContent allocate self memory for content to avoid modifying.
func (l *LogMessage) safeContent() {
	if !l.Safe {
		b := make([]byte, len(l.Content))
		copy(b, l.Content)
		l.Content = b
		l.Safe = true
	}
}

// // Parse timestamp
// ts, err := time.Parse(time.RFC3339, msg.Timestamp)
// if err != nil {
// 	return message, errors.Wrap(err, "parsing docker timestamp")
// }

type DockerStdoutProcessor struct {
	beginLineReg         *regexp.Regexp
	beginLineTimeout     time.Duration
	beginLineCheckLength int
	maxLogSize           int
	stdout               bool
	stderr               bool
	context              pipeline.Context
	collector            pipeline.Collector

	needCheckStream bool
	source          string
	tags            []protocol.Log_Content
	fieldNum        int

	// save last parsed logs
	lastLogs      []*LogMessage
	lastLogsCount int
}

func NewDockerStdoutProcessor(beginLineReg *regexp.Regexp, beginLineTimeout time.Duration, beginLineCheckLength int,
	maxLogSize int, stdout bool, stderr bool, context pipeline.Context, collector pipeline.Collector,
	tags map[string]string, source string) *DockerStdoutProcessor {
	processor := &DockerStdoutProcessor{
		beginLineReg:         beginLineReg,
		beginLineTimeout:     beginLineTimeout,
		beginLineCheckLength: beginLineCheckLength,
		maxLogSize:           maxLogSize,
		stdout:               stdout,
		stderr:               stderr,
		context:              context,
		collector:            collector,
		source:               source,
	}

	if stdout && stderr {
		processor.needCheckStream = false
	} else {
		processor.needCheckStream = true
	}
	for k, v := range tags {
		processor.tags = append(processor.tags, protocol.Log_Content{Key: k, Value: v})
	}
	processor.fieldNum = len(processor.tags) + 3
	return processor
}

// parseCRILog parses logs in CRI log format.
// CRI log format example :
// 2017-09-12T22:32:21.212861448Z stdout 2017-09-12 22:32:21.212 [INFO][88] table.go 710: Invalidating dataplane cache
func parseCRILog(line []byte) (*LogMessage, error) {
	// Ref: https://github.com/kubernetes/kubernetes/blob/master/pkg/kubelet/kuberuntime/logs/logs.go#L125-L169
	log := &LogMessage{}
	idx := bytes.Index(line, delimiter)
	if idx < 0 {
		return &LogMessage{
			Content: line,
		}, errors.New("invalid CRI log, timestamp not found")
	}
	log.Time = string(line[:idx])

	temp := line[idx+1:]
	idx = bytes.Index(temp, delimiter)
	if idx < 0 {
		return &LogMessage{
			Content: line,
		}, errors.New("invalid CRI log, stream type not found")
	}
	log.StreamType = string(temp[:idx])

	temp = temp[idx+1:]

	switch {
	case bytes.HasPrefix(temp, contianerdFullTag):
		i := bytes.Index(temp, delimiter)
		if i < 0 {
			return &LogMessage{
				Content: line,
			}, errors.New("invalid CRI log, log content not found")
		}
		log.Content = temp[i+1:]
	case bytes.HasPrefix(temp, contianerdPartTag):
		i := bytes.Index(temp, delimiter)
		if i < 0 {
			return &LogMessage{
				Content: line,
			}, errors.New("invalid CRI log, log content not found")
		}
		if bytes.HasSuffix(temp, lineSuffix) {
			log.Content = temp[i+1 : len(temp)-1]
		} else {
			log.Content = temp[i+1:]
		}
	default:
		log.Content = temp
	}
	return log, nil
}

// parseReaderLog parses logs in Docker JSON log format.
// Docker JSON log format example:
// {"log":"1:M 09 Nov 13:27:36.276 # User requested shutdown...\n","stream":"stdout", "time":"2018-05-16T06:28:41.2195434Z"}
func parseDockerJSONLog(line []byte) (*LogMessage, error) {
	dockerLog := &DockerJSONLog{}
	if err := dockerLog.UnmarshalJSON(line); err != nil {
		lm := new(LogMessage)
		lm.Content = line
		return lm, err
	}
	l := &LogMessage{
		Time:       dockerLog.Time,
		StreamType: dockerLog.StreamType,
		Content:    util.ZeroCopyStringToBytes(dockerLog.LogContent),
		Safe:       true,
	}
	dockerLog.LogContent = ""
	return l, nil
}

func (p *DockerStdoutProcessor) ParseContainerLogLine(line []byte) *LogMessage {
	if len(line) == 0 {
		logger.Warning(p.context.GetRuntimeContext(), "PARSE_DOCKER_LINE_ALARM", "parse docker line error", "empty line")
		return &LogMessage{}
	}
	if line[0] == '{' {
		log, err := parseDockerJSONLog(line)
		if err != nil {
			logger.Warning(p.context.GetRuntimeContext(), "PARSE_DOCKER_LINE_ALARM", "parse json docker line error", err.Error(), "line", util.CutString(string(line), 512))
		}
		return log
	}
	log, err := parseCRILog(line)
	if err != nil {
		logger.Warning(p.context.GetRuntimeContext(), "PARSE_DOCKER_LINE_ALARM", "parse cri docker line error", err.Error(), "line", util.CutString(string(line), 512))
	}
	return log
}

func (p *DockerStdoutProcessor) StreamAllowed(log *LogMessage) bool {
	if p.needCheckStream {
		if len(log.StreamType) == 0 {
			return true
		}
		if p.stderr {
			return log.StreamType == "stderr"
		}
		return log.StreamType == "stdout"
	}
	return true
}

func (p *DockerStdoutProcessor) Process(fileBlock []byte, noChangeInterval time.Duration) int {
	nowIndex := 0
	processedCount := 0
	for nextIndex := bytes.IndexByte(fileBlock, '\n'); nextIndex >= 0; nextIndex = bytes.IndexByte(fileBlock[nowIndex:], '\n') {
		nextIndex += nowIndex
		thisLog := p.ParseContainerLogLine(fileBlock[nowIndex : nextIndex+1])
		if p.StreamAllowed(thisLog) {
			// last char
			lastChar := uint8('\n')
			if contentLen := len(thisLog.Content); contentLen > 0 {
				lastChar = thisLog.Content[contentLen-1]
			}
			switch {
			case p.beginLineReg == nil && len(p.lastLogs) == 0 && lastChar == '\n':
				// collect single line
				p.collector.AddRawLogWithContext(p.newRawLogBySingleLine(thisLog), map[string]interface{}{"source": p.source})
			case p.beginLineReg == nil:
				// collect spilt multi lines, such as containerd.
				if lastChar != '\n' {
					thisLog.safeContent()
				}
				p.lastLogs = append(p.lastLogs, thisLog)
				p.lastLogsCount += len(thisLog.Content) + 24
				if lastChar == '\n' {
					p.collector.AddRawLogWithContext(p.newRawLogByMultiLine(), map[string]interface{}{"source": p.source})
				}
			default:
				// collect user multi lines.
				var checkLine []byte
				if len(thisLog.Content) > p.beginLineCheckLength {
					checkLine = thisLog.Content[0:p.beginLineCheckLength]
				} else {
					checkLine = thisLog.Content
				}
				if p.beginLineReg.Match(checkLine) {
					if len(p.lastLogs) != 0 {
						p.collector.AddRawLogWithContext(p.newRawLogByMultiLine(), map[string]interface{}{"source": p.source})
					}
				}
				thisLog.safeContent()
				p.lastLogs = append(p.lastLogs, thisLog)
				p.lastLogsCount += len(thisLog.Content) + 24
			}
		}

		// always set processedCount when parse a line end with '\n'
		// if wo don't do that, the process time complexity will be o(n^2)
		processedCount = nextIndex + 1
		nowIndex = nextIndex + 1
	}

	// last line and multi line timeout expired
	if len(p.lastLogs) > 0 && (noChangeInterval > p.beginLineTimeout || p.lastLogsCount > p.maxLogSize) {
		p.collector.AddRawLogWithContext(p.newRawLogByMultiLine(), map[string]interface{}{"source": p.source})
	}

	// no new line
	if nowIndex == 0 && len(fileBlock) > 0 {
		l := &LogMessage{Time: "_time_", StreamType: "_source_", Content: fileBlock}
		p.collector.AddRawLogWithContext(p.newRawLogBySingleLine(l), map[string]interface{}{"source": p.source})
		processedCount = len(fileBlock)
	}
	return processedCount
}

// newRawLogBySingleLine convert single line log to protocol.Log.
func (p *DockerStdoutProcessor) newRawLogBySingleLine(msg *LogMessage) *protocol.Log {
	nowTime := time.Now()
	log := &protocol.Log{
		Contents: make([]*protocol.Log_Content, 0, p.fieldNum),
	}
	protocol.SetLogTime(log, uint32(nowTime.Unix()))
	if len(msg.Content) > 0 && msg.Content[len(msg.Content)-1] == '\n' {
		msg.Content = msg.Content[0 : len(msg.Content)-1]
	}
	msg.safeContent()
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "content",
		Value: util.ZeroCopyBytesToString(msg.Content),
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "_time_",
		Value: msg.Time,
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "_source_",
		Value: msg.StreamType,
	})
	for i := range p.tags {
		copy := p.tags[i]
		log.Contents = append(log.Contents, &copy)
	}
	return log
}

// newRawLogByMultiLine convert last logs to protocol.Log.
func (p *DockerStdoutProcessor) newRawLogByMultiLine() *protocol.Log {
	lastOne := p.lastLogs[len(p.lastLogs)-1]
	if len(lastOne.Content) > 0 && lastOne.Content[len(lastOne.Content)-1] == '\n' {
		lastOne.Content = lastOne.Content[:len(lastOne.Content)-1]
	}
	var multiLine strings.Builder
	var sum int
	for _, log := range p.lastLogs {
		sum += len(log.Content)
	}
	multiLine.Grow(sum)
	for index, log := range p.lastLogs {
		multiLine.Write(log.Content)
		// @note force set lastLog's content nil to let GC recycle this logs
		p.lastLogs[index] = nil
	}

	nowTime := time.Now()
	log := &protocol.Log{
		Contents: make([]*protocol.Log_Content, 0, p.fieldNum),
	}
	protocol.SetLogTime(log, uint32(nowTime.Unix()))
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "content",
		Value: multiLine.String(),
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "_time_",
		Value: lastOne.Time,
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "_source_",
		Value: lastOne.StreamType,
	})
	for i := range p.tags {
		copy := p.tags[i]
		log.Contents = append(log.Contents, &copy)
	}
	// reset multiline cache
	p.lastLogs = p.lastLogs[:0]
	p.lastLogsCount = 0
	return log
}
