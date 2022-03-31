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

package processorstrptime

import (
	"fmt"
	"strconv"
	"strings"
	"time"

	"github.com/knz/strtime"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const (
	pluginName       = "processor_strptime"
	defaultSourceKey = "time"
	nilUTCOffset     = 15 * 60 * 60

	defaultPreciseTimestampKey = "precise_timestamp"
	timeStampMilliSecond       = "ms"
	timeStampMicroSecond       = "us"
	timeStampNanoSecond        = "ns"
)

// Strptime use strptime to parse specified field (through SourceKey) with Format,
// and overwrite the time of log if parsing is successful.
//
// Format comment
// Format follows rules of C strptime, but %Z does not support CST.
//
// UTCOffset comment
// By default, the processor will use the local UTC offset if it can not find any
// information in Format (%z or %s).
// You can use this parameter to set customized offset, such as -28800 for UTC-8.
// Examples:
// 1. 2016/01/02 12:59:59 doesn't have offset information, assume as UTC0
//   - Default: set to local offset (UTC+8), get 2016/01/02 12:59:59 +0800.
//   - Set to UTC-8 (-28800): get 2016/01/02 12:59:59 -0800.
// 2. 2016/01/02 12:59:59 +0700 have offset information => 2016/01/02 05:59:59 +0000
//   - Default: if local offset is UTC+8, get 2016/01/02 13:59:59 +0800.
//   - Set to UTC-8 (-28800): get 2016/01/02 05:59:59 -0800.
type Strptime struct {
	SourceKey              string `comment:"The source key prepared to be parsed by strptime."`
	Format                 string `comment:"The source key formatted pattern, more details please see [here](https://golang.org/pkg/time/#Time.Format)."`
	KeepSource             bool   `comment:"Optional. Specifies whether to keep the source key in the log content after the processing."`
	AdjustUTCOffset        bool   `comment:"Optional. Specifies whether to modify the time zone."`
	UTCOffset              int    `comment:"Optional. The UTCOffset is used to modify the log time zone. For example, the value 28800 indicates that the time zone is modified to UTC+8."`
	AlarmIfFail            bool   `comment:"Optional. Specifies whether to trigger an alert if the time information fails to be extracted."`
	EnablePreciseTimestamp bool   `comment:"Optional. Specifies whether to enable precise timestamp."`
	PreciseTimestampKey    string `comment:"Optional. The generated precise timestamp key."`
	PreciseTimestampUnit   string `comment:"Optional. The generated precise timestamp unit."`

	context  ilogtail.Context
	location *time.Location
}

// Init ...
func (s *Strptime) Init(context ilogtail.Context) error {
	s.context = context
	if len(s.Format) == 0 {
		return fmt.Errorf("format can not be empty for plugin %v", pluginName)
	}
	if !s.AdjustUTCOffset {
		s.UTCOffset = nilUTCOffset
	}
	if s.UTCOffset != nilUTCOffset {
		// See https://www.wikiwand.com/en/List_of_UTC_time_offsets# for complete list.
		if !(s.UTCOffset >= -12*60*60 && s.UTCOffset <= 14*60*60) {
			return fmt.Errorf("UTCOffset %v is out of range (from -12 to +14)", s.UTCOffset)
		}
	} else {
		// No UTC offset information in Format, use local offset.
		if !strings.Contains(s.Format, "%z") && s.Format != "%s" {
			_, s.UTCOffset = time.Now().Zone()
		}
	}
	if s.UTCOffset != nilUTCOffset {
		s.location = time.FixedZone("SpecifiedTimezone", s.UTCOffset)
	}

	if len(s.SourceKey) == 0 {
		s.SourceKey = defaultSourceKey
	}

	if len(s.PreciseTimestampKey) == 0 {
		s.PreciseTimestampKey = defaultPreciseTimestampKey
	}
	return nil
}

// Description ...
func (s *Strptime) Description() string {
	return "Processor to extract time from log: strptime(SourceKey, Format)"
}

// ProcessLogs ...
func (s *Strptime) ProcessLogs(logs []*protocol.Log) []*protocol.Log {
	for _, log := range logs {
		s.processLog(log)
	}
	return logs
}

func (s *Strptime) processLog(log *protocol.Log) {
	logTime := time.Time{}
	var err error
	for idx, content := range log.Contents {
		if content.Key != s.SourceKey {
			continue
		}

		value := content.Value
		// Truncate if format is Unix timestamp.
		if s.Format == "%s" {
			value = value[0:10]
		}

		logTime, err = strtime.Strptime(value, s.Format)
		if err != nil || (logTime == time.Time{}) {
			if s.AlarmIfFail {
				logger.Warningf(s.context.GetRuntimeContext(), "STRPTIME_PARSE_ALARM", "strptime(%v, %v) failed: %v, %v",
					content.Value, s.Format, logTime, err)
			}
			break
		}

		if s.location != nil {
			logTime = logTime.In(s.location).Add(time.Second * time.Duration(-s.UTCOffset))
		}

		log.Time = uint32(logTime.Unix())
		if !s.KeepSource {
			log.Contents = append(log.Contents[:idx], log.Contents[idx+1:]...)
		}
		break
	}
	if (s.EnablePreciseTimestamp && logTime != time.Time{}) {
		var timestamp string
		switch s.PreciseTimestampUnit {
		case "", timeStampMilliSecond:
			timestamp = strconv.FormatInt(logTime.UnixNano()/1e6, 10)
		case timeStampMicroSecond:
			timestamp = strconv.FormatInt(logTime.UnixNano()/1e3, 10)
		case timeStampNanoSecond:
			timestamp = strconv.FormatInt(logTime.UnixNano(), 10)
		default:
			timestamp = strconv.FormatInt(logTime.UnixNano()/1e6, 10)
		}
		log.Contents = append(log.Contents, &protocol.Log_Content{
			Key:   s.PreciseTimestampKey,
			Value: timestamp,
		})
	}
}

func newStrptime() *Strptime {
	return &Strptime{
		SourceKey:              defaultSourceKey,
		KeepSource:             true,
		AlarmIfFail:            true,
		UTCOffset:              nilUTCOffset,
		AdjustUTCOffset:        false,
		EnablePreciseTimestamp: false,
		PreciseTimestampKey:    defaultPreciseTimestampKey,
		PreciseTimestampUnit:   timeStampMilliSecond,
	}
}

func init() {
	ilogtail.Processors[pluginName] = func() ilogtail.Processor {
		return newStrptime()
	}
}
