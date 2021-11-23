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

package gotime

import (
	"fmt"
	"time"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type ProcessorGotime struct {
	SourceKey      string
	SourceFormat   string
	SourceLocation int    // 为空表示本机时区
	DestKey        string // 目标Key，为空不生效
	DestFormat     string
	DestLocation   int  // 为空表示本机时区
	SetTime        bool // 是否设置到时间字段，默认为true
	KeepSource     bool // 是否保留源字段
	NoKeyError     bool
	AlarmIfFail    bool

	sourceLocation *time.Location
	destLocation   *time.Location
	context        ilogtail.Context
}

const pluginName = "processor_gotime"

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorGotime) Init(context ilogtail.Context) error {
	if p.SourceKey == "" {
		return fmt.Errorf("must specify SourceKey for plugin %v", pluginName)
	}
	if p.SourceFormat == "" {
		return fmt.Errorf("must specify SourceFormat for plugin %v", pluginName)
	}
	if p.DestKey == "" {
		return fmt.Errorf("must specify DestKey for plugin %v", pluginName)
	}
	if p.DestFormat == "" {
		return fmt.Errorf("must specify DestFormat for plugin %v", pluginName)
	}
	p.sourceLocation = time.Local
	if p.SourceLocation != 0 {
		p.sourceLocation = time.FixedZone("SpecifiedTimezone", p.SourceLocation*60*60)
	}
	p.destLocation = time.Local
	if p.DestLocation != 0 {
		p.destLocation = time.FixedZone("SpecifiedTimezone", p.DestLocation*60*60)
	}
	p.context = context
	return nil
}

func (*ProcessorGotime) Description() string {
	return "gotime processor for logtail"
}

func (p *ProcessorGotime) ProcessLogs(logs []*protocol.Log) []*protocol.Log {
	for _, log := range logs {
		p.processLog(log)
	}
	return logs
}

func (p *ProcessorGotime) processLog(log *protocol.Log) {
	found := false
	for idx, content := range log.Contents {
		if content.Key == p.SourceKey {
			parsedTime, err := time.ParseInLocation(p.SourceFormat, content.Value, p.sourceLocation)
			if err != nil && p.AlarmIfFail {
				logger.Warningf(p.context.GetRuntimeContext(), "GOTIME_PARSE_ALARM", "ParseInLocation(%v, %v, %v) failed: %v",
					p.SourceFormat, content.Value, p.sourceLocation, err)
				return
			}
			if p.SetTime {
				log.Time = uint32(parsedTime.Unix())
			}
			if !p.KeepSource {
				log.Contents = append(log.Contents[:idx], log.Contents[idx+1:]...)
			}
			if p.DestKey != "" {
				destLocationTime := parsedTime.In(p.destLocation)
				parsedValue := destLocationTime.Format(p.DestFormat)
				newContent := &protocol.Log_Content{
					Key:   p.DestKey,
					Value: parsedValue,
				}
				log.Contents = append(log.Contents, newContent)
			}
			found = true
			break
		}
	}
	if !found && p.NoKeyError {
		logger.Warningf(p.context.GetRuntimeContext(), "GOTIME_FIND_ALARM", "cannot find key %v", p.SourceKey)
	}
}

func init() {
	ilogtail.Processors[pluginName] = func() ilogtail.Processor {
		return &ProcessorGotime{
			SourceKey:      "",
			SourceFormat:   "",
			SourceLocation: 0,
			DestKey:        "",
			DestFormat:     "",
			DestLocation:   0,
			SetTime:        true,
			KeepSource:     true,
			NoKeyError:     true,
			AlarmIfFail:    true,
		}
	}
}
