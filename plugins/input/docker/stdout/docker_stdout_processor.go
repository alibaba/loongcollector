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

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"
)

type DockerJSONLog struct {
	LogContent string `json:"log"`
	StreamType string `json:"stream"`
	Time       string `json:"time"`
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
	context              ilogtail.Context
	collector            ilogtail.Collector

	needCheckStream bool
	// len(ContainerNameTag map[string]string) + len(DockerJSONLog.keys)
	allLogKeys   []string
	allLogValues []string

	// save last parsed logs
	lastLogs      []*DockerJSONLog
	lastLogsCount int
}

func NewDockerStdoutProcessor(beginLineReg *regexp.Regexp, beginLineTimeout time.Duration, beginLineCheckLength int,
	maxLogSize int, stdout bool, stderr bool, context ilogtail.Context, collector ilogtail.Collector, tags map[string]string) *DockerStdoutProcessor {
	processor := &DockerStdoutProcessor{
		beginLineReg:         beginLineReg,
		beginLineTimeout:     beginLineTimeout,
		beginLineCheckLength: beginLineCheckLength,
		maxLogSize:           maxLogSize,
		stdout:               stdout,
		stderr:               stderr,
		context:              context,
		collector:            collector,
	}

	if stdout && stderr {
		processor.needCheckStream = false
	} else {
		processor.needCheckStream = true
	}

	processor.allLogKeys = append(processor.allLogKeys, "content")
	processor.allLogKeys = append(processor.allLogKeys, "_time_")
	processor.allLogKeys = append(processor.allLogKeys, "_source_")

	processor.allLogValues = append(processor.allLogValues, "content")
	processor.allLogValues = append(processor.allLogValues, "_time_")
	processor.allLogValues = append(processor.allLogValues, "_source_")

	for key, value := range tags {
		processor.allLogKeys = append(processor.allLogKeys, key)
		processor.allLogValues = append(processor.allLogValues, value)
	}
	return processor
}

// parseCRILog parses logs in CRI log format.
// CRI log format example :
// 2017-09-12T22:32:21.212861448Z stdout 2017-09-12 22:32:21.212 [INFO][88] table.go 710: Invalidating dataplane cache
func parseCRILog(line []byte) (*DockerJSONLog, error) {
	dockerLog := &DockerJSONLog{}
	log := strings.SplitN(string(line), " ", 3)
	if len(log) < 3 {
		dockerLog.LogContent = string(line)
		return dockerLog, errors.New("invalid CRI log")
	}
	dockerLog.Time = log[0]
	dockerLog.StreamType = log[1]

	// Ref: https://github.com/kubernetes/kubernetes/blob/master/pkg/kubelet/kuberuntime/logs/logs.go#L125-L169
	content := log[2]
	if strings.HasPrefix(content, "F ") {
		content = content[2:]
	} else if strings.HasPrefix(content, "P ") {
		// Partial line, trim last line feed.
		content = content[2:]
		if len(content) > 0 && content[len(content)-1] == '\n' {
			content = content[:len(content)-1]
		}
	}
	dockerLog.LogContent = content
	return dockerLog, nil
}

// parseReaderLog parses logs in Docker JSON log format.
// Docker JSON log format example:
// {"log":"1:M 09 Nov 13:27:36.276 # User requested shutdown...\n","stream":"stdout", "time":"2018-05-16T06:28:41.2195434Z"}
func parseDockerJSONLog(line []byte) (*DockerJSONLog, error) {
	dockerLog := &DockerJSONLog{}
	if err := dockerLog.UnmarshalJSON(line); err != nil {
		dockerLog.LogContent = string(line)
		return dockerLog, err
	}
	return dockerLog, nil
}

func (p *DockerStdoutProcessor) ParseDockerLogLine(line []byte) *DockerJSONLog {
	if len(line) == 0 {
		logger.Warning(p.context.GetRuntimeContext(), "PARSE_DOCKER_LINE_ALARM", "parse docker line error", "empty line")
		return &DockerJSONLog{}
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

func (p *DockerStdoutProcessor) StreamAllowed(log *DockerJSONLog) bool {
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

func (p *DockerStdoutProcessor) flushLastLog() {
	p.allLogValues[1] = p.lastLogs[0].Time
	p.allLogValues[2] = p.lastLogs[0].StreamType
	var multiLine string
	for index, log := range p.lastLogs {
		multiLine += log.LogContent
		// @note force set lastLog's content nil to let GC recycle this logs
		p.lastLogs[index] = nil
	}
	if contentSize := len(multiLine); contentSize > 0 && multiLine[contentSize-1] == '\n' {
		multiLine = multiLine[0 : contentSize-1]
	}
	p.allLogValues[0] = multiLine
	p.collector.AddDataArray(nil, p.allLogKeys, p.allLogValues)
	p.lastLogs = p.lastLogs[:0]
	p.lastLogsCount = 0
	// @note force set log values empty to let GC recycle this logs
	p.allLogValues[0] = ""
	p.allLogValues[1] = ""
	p.allLogValues[2] = ""
}

func (p *DockerStdoutProcessor) Process(fileBlock []byte, noChangeInterval time.Duration) int {
	nowIndex := 0
	processedCount := 0
	for nextIndex := bytes.IndexByte(fileBlock, '\n'); nextIndex >= 0; nextIndex = bytes.IndexByte(fileBlock[nowIndex:], '\n') {
		nextIndex += nowIndex
		thisLog := p.ParseDockerLogLine(fileBlock[nowIndex : nextIndex+1])
		if p.StreamAllowed(thisLog) {
			// last char
			lastChar := uint8('\n')
			if contentLen := len(thisLog.LogContent); contentLen > 0 {
				lastChar = thisLog.LogContent[contentLen-1]
			}
			// single line and log not splited
			switch {
			case p.beginLineReg == nil && len(p.lastLogs) == 0 && lastChar == '\n':
				if contentSize := len(thisLog.LogContent); contentSize > 0 && thisLog.LogContent[contentSize-1] == '\n' {
					thisLog.LogContent = thisLog.LogContent[0 : contentSize-1]
				}
				p.allLogValues[0] = thisLog.LogContent
				p.allLogValues[1] = thisLog.Time
				p.allLogValues[2] = thisLog.StreamType
				p.collector.AddDataArray(nil, p.allLogKeys, p.allLogValues)
				p.allLogValues[0] = ""
				p.allLogValues[1] = ""
				p.allLogValues[2] = ""
			case p.beginLineReg == nil:
				p.lastLogs = append(p.lastLogs, thisLog)
				p.lastLogsCount += len(thisLog.LogContent) + 24
				if lastChar == '\n' {
					p.flushLastLog()
				}
			default:
				var checkLine string
				if len(thisLog.LogContent) > p.beginLineCheckLength {
					checkLine = thisLog.LogContent[0:p.beginLineCheckLength]
				} else {
					checkLine = thisLog.LogContent
				}
				if p.beginLineReg.MatchString(checkLine) {
					if len(p.lastLogs) != 0 {
						p.flushLastLog()
					}
				}
				p.lastLogs = append(p.lastLogs, thisLog)
				p.lastLogsCount += len(thisLog.LogContent) + 24
			}
		}

		// always set processedCount when parse a line end with '\n'
		// if wo don't do that, the process time complexity will be o(n^2)
		processedCount = nextIndex + 1
		nowIndex = nextIndex + 1
	}

	// last line and multi line timeout expired
	if len(p.lastLogs) > 0 && (noChangeInterval > p.beginLineTimeout || p.lastLogsCount > p.maxLogSize) {
		p.flushLastLog()
	}

	// no new line
	if nowIndex == 0 && len(fileBlock) > 0 {
		p.allLogValues[0] = string(fileBlock)
		p.collector.AddDataArray(nil, p.allLogKeys, p.allLogValues)
		p.allLogValues[0] = ""
		processedCount = len(fileBlock)
	}

	return processedCount
}
