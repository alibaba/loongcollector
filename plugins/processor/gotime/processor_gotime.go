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
	"strconv"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const (
	fixedSecondsTimestampPattern      = "seconds"
	fixedMillisecondsTimestampPattern = "milliseconds"
	fixedMicrosecondsTimestampPattern = "microseconds"
	fixedNanosecondsTimestampPattern  = "nanoseconds"
)

type ProcessorGotime struct {
	SourceKey      string `comment:"the source key prepared to be formatted"`
	SourceFormat   string `comment:"the source key formatted pattern, more details please see [here](https://golang.org/pkg/time/#Time.Format). Furthermore，there are 3 fixed pattern supported to parse timestamp, which are 'seconds','milliseconds' and 'microseconds'."`
	SourceLocation int    `comment:"the source key time zone, such beijing timezone is 8. And the parameter would be ignored when 'SourceFormat' configured with timestamp format pattern."`
	DestKey        string `comment:"the generated key name."`
	DestFormat     string `comment:"the generated key formatted pattern, more details please see [here](https://golang.org/pkg/time/#Time.Format)."`
	DestLocation   int    `comment:"the generated key time zone, such beijing timezone is 8."`
	SetTime        bool   `comment:"Whether to config the unix time of the source key to the log time. "`
	KeepSource     bool   `comment:"Whether to keep the source key in the log content after the processing."`
	NoKeyError     bool   `comment:"Whether to alarm when not found the source key to parse and format."`
	AlarmIfFail    bool   `comment:"Whether to alarm when the source key is failed to parse."`

	sourceLocation     *time.Location
	destLocation       *time.Location
	context            pipeline.Context
	timestampFormat    bool
	timestampParseFunc func(timestamp int64) time.Time
}

const (
	pluginType      = "processor_gotime"
	machineTimeZone = -100
)

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorGotime) Init(context pipeline.Context) error {
	if p.SourceKey == "" {
		return fmt.Errorf("must specify SourceKey for plugin %v", pluginType)
	}
	if p.SourceFormat == "" {
		return fmt.Errorf("must specify SourceFormat for plugin %v", pluginType)
	}
	if p.DestKey == "" {
		return fmt.Errorf("must specify DestKey for plugin %v", pluginType)
	}
	if p.DestFormat == "" {
		return fmt.Errorf("must specify DestFormat for plugin %v", pluginType)
	}
	p.sourceLocation = time.Local
	if p.SourceLocation != machineTimeZone {
		p.sourceLocation = time.FixedZone("SpecifiedTimezone", p.SourceLocation*60*60)
	}
	p.destLocation = time.Local
	if p.DestLocation != machineTimeZone {
		p.destLocation = time.FixedZone("SpecifiedTimezone", p.DestLocation*60*60)
	}

	switch p.SourceFormat {
	case fixedSecondsTimestampPattern:
		p.timestampParseFunc = func(timestamp int64) time.Time {
			return time.Unix(timestamp, 0)
		}
		p.timestampFormat = true
	case fixedMicrosecondsTimestampPattern:
		p.timestampParseFunc = func(timestamp int64) time.Time {
			return time.Unix(timestamp/1e6, (timestamp%1e6)*1e3)
		}
		p.timestampFormat = true
	case fixedMillisecondsTimestampPattern:
		p.timestampParseFunc = func(timestamp int64) time.Time {
			return time.Unix(timestamp/1e3, (timestamp%1e3)*1e6)
		}
		p.timestampFormat = true
	case fixedNanosecondsTimestampPattern:
		p.timestampParseFunc = func(timestamp int64) time.Time {
			return time.Unix(0, timestamp)
		}
		p.timestampFormat = true
	}
	p.context = context
	return nil
}

func (*ProcessorGotime) Description() string {
	return "the time format processor to parse time field with golang format pattern. More details please see [here](https://golang.org/pkg/time/#Time.Format)"
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
			var parsedTime time.Time
			if p.timestampFormat {
				i, err := strconv.ParseInt(content.Value, 10, 64)
				if err != nil && p.AlarmIfFail {
					logger.Warningf(p.context.GetRuntimeContext(), "GOTIME_PARSE_ALARM", "ParseInt(%v, %v) failed: %v",
						p.SourceFormat, content.Value, err)
					return
				}
				parsedTime = p.timestampParseFunc(i)
			} else {
				parsedStringTime, err := time.ParseInLocation(p.SourceFormat, content.Value, p.sourceLocation)
				if err != nil && p.AlarmIfFail {
					logger.Warningf(p.context.GetRuntimeContext(), "GOTIME_PARSE_ALARM", "ParseInLocation(%v, %v, %v) failed: %v",
						p.SourceFormat, content.Value, p.sourceLocation, err)
					return
				}
				parsedTime = parsedStringTime
			}
			if p.SetTime {
				if p.context.GetPipelineScopeConfig().EnableTimestampNanosecond {
					protocol.SetLogTimeWithNano(log, uint32(parsedTime.Unix()), uint32(parsedTime.Nanosecond()))
				} else {
					protocol.SetLogTime(log, uint32(parsedTime.Unix()))
				}
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
	pipeline.Processors[pluginType] = func() pipeline.Processor {
		return &ProcessorGotime{
			SourceKey:      "",
			SourceFormat:   "",
			SourceLocation: machineTimeZone,
			DestKey:        "",
			DestFormat:     "",
			DestLocation:   machineTimeZone,
			SetTime:        true,
			KeepSource:     true,
			NoKeyError:     true,
			AlarmIfFail:    true,
		}
	}
}
