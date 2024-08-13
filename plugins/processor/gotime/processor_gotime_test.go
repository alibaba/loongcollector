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
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"strings"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func init() {
	logger.InitTestLogger(logger.OptionOpenMemoryReceiver)
}

func newProcessor() (*ProcessorGotime, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorGotime{
		SourceKey:      "s_key",
		SourceFormat:   "2006-01-02 15:04:05",
		SourceLocation: 8,       // 8，为空表示本机时区
		DestKey:        "d_key", // 目标Key，为空不生效
		DestFormat:     "2006/01/02 15:04:05",
		DestLocation:   9,    // 8，为空表示本机时区
		SetTime:        true, // 是否设置到时间字段，默认为true
		KeepSource:     true, // 是否保留源字段
		NoKeyError:     true,
		AlarmIfFail:    true,
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestSourceKey(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	log := &protocol.Log{Time: 0}
	timeStr := "2019-07-05 19:28:01"
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "s_key", Value: timeStr})
	processor.processLog(log)
	assert.Equal(t, "d_key", log.Contents[1].Key)
	assert.Equal(t, "2019/07/05 20:28:01", log.Contents[1].Value)
}

func TestSetTime(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	timeStr := "2019-07-05 19:28:01"
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "s_key", Value: timeStr})
	processor.processLog(log)
	destLocation := time.FixedZone("SpecifiedTimezone", 9*60*60)
	time1, _ := time.ParseInLocation("2006-01-02 15:04:05", "2019-07-05 20:28:01", destLocation)
	unixTime1 := uint32(time1.Unix())
	assert.Equal(t, unixTime1, log.Time)
}

func TestKeepSource(t *testing.T) {
	processor, err := newProcessor()
	if processor == nil {
		return
	}
	processor.KeepSource = false
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	timeStr := "2019-07-05 19:28:01"
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "s_key", Value: timeStr})
	processor.processLog(log)
	assert.Equal(t, "d_key", log.Contents[0].Key)
}

func TestNoKeyError(t *testing.T) {
	logger.ClearMemoryLog()
	processor, err := newProcessor()
	require.NoError(t, err)

	log := &protocol.Log{Time: 0}
	processor.processLog(log)
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.True(t, strings.Contains(memoryLog, "GOTIME_FIND_ALARM\tcannot find key s_key"))
}

func TestAlarmIfFail(t *testing.T) {
	logger.ClearMemoryLog()
	processor, err := newProcessor()
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	timeStr := "2019-07-05-19:28:01"
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "s_key", Value: timeStr})
	processor.processLog(log)
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.True(t, strings.Contains(memoryLog, "GOTIME_PARSE_ALARM\tParseInLocation(2006-01-02 15:04:05, 2019-07-05-19:28:01, SpecifiedTimezone) "+
		"failed: parsing time \"2019-07-05-19:28:01\" as \"2006-01-02 15:04:05\": cannot parse \"-19:28:01\" as \" \""))
}

func TestSourceFormatTimestampSeconds(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	log := &protocol.Log{Time: 0}
	unixStr := "1645595256"
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "s_key", Value: unixStr})
	processor.SourceFormat = fixedSecondsTimestampPattern
	_ = processor.Init(processor.context)

	processor.processLog(log)
	assert.Equal(t, "d_key", log.Contents[1].Key)
	assert.Equal(t, "2022/02/23 14:47:36", log.Contents[1].Value)
}

func TestSourceFormatTimestampMilliseconds(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	processor.SourceFormat = fixedMillisecondsTimestampPattern
	processor.DestFormat = "2006/01/02 15:04:05.000"
	_ = processor.Init(processor.context)

	log := &protocol.Log{Time: 0}
	unixStr := "1645595256807"
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "s_key", Value: unixStr})

	processor.processLog(log)
	assert.Equal(t, "d_key", log.Contents[1].Key)
	assert.Equal(t, "2022/02/23 14:47:36.807", log.Contents[1].Value)
}

func TestSourceFormatTimestampMicroseconds(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	log := &protocol.Log{Time: 0}
	unixStr := "1645595256807000"
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "s_key", Value: unixStr})
	processor.SourceFormat = fixedMicrosecondsTimestampPattern
	processor.DestFormat = "2006/01/02 15:04:05.000000"
	_ = processor.Init(processor.context)
	processor.processLog(log)
	assert.Equal(t, "d_key", log.Contents[1].Key)
	assert.Equal(t, "2022/02/23 14:47:36.807000", log.Contents[1].Value)
}

func TestSourceFormatTimestampNanoseconds(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	log := &protocol.Log{Time: 0}
	unixStr := "1645595256807000123"
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "s_key", Value: unixStr})
	processor.SourceFormat = fixedNanosecondsTimestampPattern
	processor.DestFormat = "2006/01/02 15:04:05.000000000"
	_ = processor.Init(processor.context)
	processor.processLog(log)
	assert.Equal(t, "d_key", log.Contents[1].Key)
	assert.Equal(t, "2022/02/23 14:47:36.807000123", log.Contents[1].Value)
}

func TestEmptyTimezone(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorGotime{
		SourceKey:      "s_key",
		SourceFormat:   "2006-01-02 15:04:05",
		SourceLocation: machineTimeZone,
		DestKey:        "d_key",
		DestFormat:     "2006/01/02 15:04:05",
		DestLocation:   9,
		SetTime:        true,
		KeepSource:     true,
		NoKeyError:     true,
		AlarmIfFail:    true,
	}
	err := processor.Init(ctx)
	require.NoError(t, err)

	log := &protocol.Log{Time: 0}
	timeStr := "2019-07-05 19:28:01"
	localTime, _ := time.ParseInLocation("2006-01-02 15:04:05", timeStr, time.Local)
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "s_key", Value: localTime.Format(processor.SourceFormat)})
	processor.processLog(log)
	destLocation := time.FixedZone("SpecifiedTimezone", 9*60*60)
	processedTime := localTime.In(destLocation)
	assert.Equal(t, "d_key", log.Contents[1].Key)
	assert.Equal(t, processedTime.Format(processor.DestFormat), log.Contents[1].Value)
}
