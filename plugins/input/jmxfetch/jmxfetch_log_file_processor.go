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

package jmxfetch

import (
	"bytes"
	"regexp"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
)

type Processor struct {
	reg     *regexp.Regexp
	content strings.Builder
	level   string
}

func NewProcessor() *Processor {
	p := new(Processor)
	p.reg = regexp.MustCompile(`^\d{4}-\d{2}-\d{2}\s\d{2}:\d{2}:\d{2}`)
	return p
}

func (p *Processor) Process(fileBlock []byte, noChangeInterval time.Duration) int {
	nowIndex := 0
	processedCount := 0
	for nextIndex := bytes.IndexByte(fileBlock, '\n'); nextIndex >= 0; nextIndex = bytes.IndexByte(fileBlock[nowIndex:], '\n') {
		nextIndex += nowIndex
		originalLine := fileBlock[nowIndex : nextIndex+1]
		if p.reg.Match(originalLine[:19]) {
			p.flush()
			p.content.Reset()
			first := bytes.IndexByte(originalLine, '|')
			second := bytes.IndexByte(originalLine[first+1:], '|') + first + 1
			third := bytes.IndexByte(originalLine[second+1:], '|') + second + 1
			p.level = strings.TrimSpace(string(originalLine[second+1 : third]))
		}
		p.content.Write(originalLine)
		processedCount = nextIndex + 1
		nowIndex = nextIndex + 1
	}
	return processedCount
}

func (p *Processor) flush() {
	if p.content.Len() == 0 {
		return
	}
	switch p.level {
	case "WARN":
		logger.Warning(manager.managerMeta.GetContext(), JMXAlarmType, "log", p.content.String())
	case "ERROR":
		logger.Error(manager.managerMeta.GetContext(), JMXAlarmType, "log", p.content.String())
	case "INFO":
		logger.Info(manager.managerMeta.GetContext(), "log", p.content.String())
	case "DEBUG":
		logger.Debug(manager.managerMeta.GetContext(), "log", p.content.String())
	default:
		logger.Warning(manager.managerMeta.GetContext(), JMXAlarmType, "illegal log", p.content.String())
	}
}
