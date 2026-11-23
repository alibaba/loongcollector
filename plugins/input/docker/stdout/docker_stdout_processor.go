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
	"maps"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/models"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
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
	collector            pipeline.PipelineCollector

	needCheckStream bool
	source          string
	groupTags       map[string]string
	contentTags     map[string]string

	// save last parsed logs
	lastLogs      []*LogMessage
	lastLogsCount int
}

func NewDockerStdoutProcessor(beginLineReg *regexp.Regexp, beginLineTimeout time.Duration, beginLineCheckLength int,
	maxLogSize int, stdout bool, stderr bool, context pipeline.Context, collector pipeline.PipelineCollector,
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
		contentTags:          maps.Clone(tags),
	}

	if stdout && stderr {
		processor.needCheckStream = false
	} else {
		processor.needCheckStream = true
	}
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
		logger.Warning(p.context.GetRuntimeContext(), selfmonitor.ParseDockerLineAlarm, "parse docker line error", "empty line")
		return &LogMessage{}
	}
	if line[0] == '{' {
		log, err := parseDockerJSONLog(line)
		if err != nil {
			logger.Warning(p.context.GetRuntimeContext(), selfmonitor.ParseDockerLineAlarm, "parse json docker line error", err.Error(), "line", util.CutString(string(line), 512))
		}
		return log
	}
	log, err := parseCRILog(line)
	if err != nil {
		logger.Warning(p.context.GetRuntimeContext(), selfmonitor.ParseDockerLineAlarm, "parse cri docker line error", err.Error(), "line", util.CutString(string(line), 512))
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
				p.collect(p.newLogBySingleLine(thisLog))
			case p.beginLineReg == nil:
				// collect spilt multi lines, such as containerd.
				if lastChar != '\n' {
					thisLog.safeContent()
				}
				p.lastLogs = append(p.lastLogs, thisLog)
				p.lastLogsCount += len(thisLog.Content) + 24
				if lastChar == '\n' {
					p.collect(p.newLogByMultiLine())
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
						p.collect(p.newLogByMultiLine())
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
		p.collect(p.newLogByMultiLine())
	}

	// no new line
	if nowIndex == 0 && len(fileBlock) > 0 {
		thisLog := p.ParseContainerLogLine(fileBlock)
		if p.StreamAllowed(thisLog) {
			p.collect(p.newLogBySingleLine(thisLog))
			processedCount = len(fileBlock)
		}
	}
	return processedCount
}

func (p *DockerStdoutProcessor) collect(log *models.Log) {
	tags := models.NewTags()
	tags.AddAll(p.groupTags)
	metadata := models.NewMetadataWithKeyValues("source", p.source)
	p.collector.Collect(models.NewGroup(metadata, tags), log)
}

func (p *DockerStdoutProcessor) newLog(body []byte, timestamp, stream string) *models.Log {
	const tagPrefix = "__tag__:"
	log := &models.Log{
		Timestamp: uint64(time.Now().Truncate(time.Second).UnixNano()),
		Contents:  models.NewOrderedLogContents(len(p.contentTags) + 3),
		Tags:      models.NewTags(),
	}
	log.SetBody(body)
	log.Contents.Add("_time_", timestamp)
	log.Contents.Add("_source_", stream)
	// Older file-based stdout payloads encode tags and offsets with reserved prefixes.
	for key, value := range p.contentTags {
		switch {
		case key == models.BodyKey:
			log.SetBody([]byte(value))
		case key == tagPrefix+"__file_offset__":
			if offset, err := strconv.ParseInt(value, 10, 64); err == nil {
				log.SetOffset(uint64(offset))
			}
		case strings.HasPrefix(key, tagPrefix):
			log.Tags.Add(strings.TrimPrefix(key, tagPrefix), value)
		default:
			log.Contents.Add(key, value)
		}
	}
	return log
}

func (p *DockerStdoutProcessor) newLogBySingleLine(msg *LogMessage) *models.Log {
	if len(msg.Content) > 0 && msg.Content[len(msg.Content)-1] == '\n' {
		msg.Content = msg.Content[:len(msg.Content)-1]
	}
	msg.safeContent()
	return p.newLog(msg.Content, msg.Time, msg.StreamType)
}

func (p *DockerStdoutProcessor) newLogByMultiLine() *models.Log {
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

	log := p.newLog(util.ZeroCopyStringToBytes(multiLine.String()), lastOne.Time, lastOne.StreamType)
	// reset multiline cache
	p.lastLogs = p.lastLogs[:0]
	p.lastLogsCount = 0
	return log
}
