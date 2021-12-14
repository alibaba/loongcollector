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
	"testing"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"github.com/stretchr/testify/require"
)

func init() {
	logger.InitTestLogger(logger.OptionOpenMemoryReceiver)
}

func newProcessor(format string, utcOffset int) (*Strptime, error) {
	ctx := mock.NewEmptyContext("p", "c", "l")
	processor := newStrptime()
	processor.Format = format
	if utcOffset != nilUTCOffset {
		processor.UTCOffset = utcOffset
		processor.AdjustUTCOffset = true
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestSourceKey(t *testing.T) {
	format := "%Y/%m/%d"
	processor, err := newProcessor(format, nilUTCOffset)
	require.NoError(t, err)

	nowTime := time.Now()
	tests := []struct {
		SourceKey    string
		Keys         []string
		Values       []string
		ExpectedTime int64
	}{
		// Empty SourceKey, leave time field alone.
		{"", []string{"K1", "K2"}, []string{"2016/01/02", "..."}, 0},
		{"", []string{"K2", "K1"}, []string{"2016/01/03", "..."}, 0},
		// Has SourceKey, use it.
		{"K2", []string{"K0", "K2", "K1"}, []string{"K0 value", "2016/01/02", "K1 value"}, time.Date(2016, time.January, 2, 0, 0, 0, 0, nowTime.Location()).Unix()},
		{"K0", []string{"K0", "K2", "K1"}, []string{"2016/01/02", "K2 value", "K1 value"}, time.Date(2016, time.January, 2, 0, 0, 0, 0, nowTime.Location()).Unix()},
		{"K1", []string{"K0", "K2", "K1"}, []string{"K0 value", "K2 value", "2016/01/02"}, time.Date(2016, time.January, 2, 0, 0, 0, 0, nowTime.Location()).Unix()},
	}

	for idx, test := range tests {
		log := &protocol.Log{Time: 0}
		for idx, key := range test.Keys {
			log.Contents = append(log.Contents,
				&protocol.Log_Content{Key: key, Value: test.Values[idx]})
		}

		logger.ClearMemoryLog()
		processor.SourceKey = test.SourceKey
		processor.processLog(log)
		require.Equal(t, 0, logger.GetMemoryLogCount())
		require.Equal(t, test.ExpectedTime, int64(log.Time), "Test case %v failed", idx)
	}
}

func TestFormat(t *testing.T) {
	time.Local = time.UTC
	logger.ClearMemoryLog()
	tests := []struct {
		Value        string
		Format       string
		ExpectedTime int64
	}{
		{"2016/01/02", "%Y/%m/%d", time.Date(2016, time.January, 2, 0, 0, 0, 0, time.UTC).Unix()},
		{"2016/01/02 12:59:59", "%Y/%m/%d %H:%M:%S", time.Date(2016, time.January, 2, 12, 59, 59, 0, time.UTC).Unix()},
		{"2016/01/02-12:59:59", "%Y/%m/%d-%H:%M:%S", time.Date(2016, time.January, 2, 12, 59, 59, 0, time.UTC).Unix()},
		{"2016/01/02 12:59:59.123", "%Y/%m/%d %H:%M:%S", time.Date(2016, time.January, 2, 12, 59, 59, 0, time.UTC).Unix()},
		{"2016/01/02 12:59:59 +0700 (UTC)", "%Y/%m/%d %H:%M:%S %z (%Z)", time.Date(2016, time.January, 2, 12, 59, 59, 0, time.FixedZone("UTC+7", 7*60*60)).Unix()},
		{"1451710799", "%s", time.Date(2016, time.January, 2, 12, 59, 59, 0, time.FixedZone("UTC+8", 8*60*60)).Unix()},
		{"1451710799000", "%s", time.Date(2016, time.January, 2, 12, 59, 59, 0, time.FixedZone("UTC+8", 8*60*60)).Unix()},
		{"1451710799000000", "%s", time.Date(2016, time.January, 2, 12, 59, 59, 0, time.FixedZone("UTC+8", 8*60*60)).Unix()},
		{"2016/Jan/02 12:59:59", "%Y/%b/%d %H:%M:%S", time.Date(2016, time.January, 2, 12, 59, 59, 0, time.UTC).Unix()},
		{"2019-07-15T04:16:47Z", "%Y-%m-%dT%H:%M:%S", time.Date(2019, time.July, 15, 4, 16, 47, 0, time.UTC).Unix()},
	}
	for idx, test := range tests {
		processor, err := newProcessor(test.Format, nilUTCOffset)
		require.NoError(t, err)

		log := &protocol.Log{Time: 0}
		log.Contents = append(log.Contents,
			&protocol.Log_Content{Key: defaultSourceKey, Value: test.Value})
		processor.processLog(log)
		require.Equal(t, 0, logger.GetMemoryLogCount())
		require.Equal(t, test.ExpectedTime, int64(log.Time), "Test case %v failed", idx)
	}
}

func TestUTCOffsetWithTimeZone(t *testing.T) {
	tests := []struct {
		Format string
		Value  string
	}{
		{"%Y/%m/%d %H:%M:%S", "2016/01/02 12:59:59"},
		{"%Y/%m/%d %H:%M:%S %z (%Z)", "2016/01/02 12:59:59 +0700 (UTC)"},
	}

	// Default processor, without UTCOffset configuration.
	{
		results := []time.Time{
			time.Date(2016, time.January, 2, 12, 59, 59, 0, time.Now().Location()),
			time.Date(2016, time.January, 2, 5, 59, 59, 0, time.FixedZone("UTC", 0)),
		}
		for idx, test := range tests {
			processor, err := newProcessor(test.Format, nilUTCOffset)
			require.NoError(t, err)
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents,
				&protocol.Log_Content{Key: defaultSourceKey, Value: test.Value})
			processor.processLog(log)
			require.Equal(t, 0, logger.GetMemoryLogCount())
			require.Equal(t, int64(log.Time), results[idx].Unix())
		}
	}

	// Processor with UTCOffset enabled.
	{
		offset := -8 * 60 * 60
		results := []time.Time{
			time.Date(2016, time.January, 2, 12, 59, 59, 0, time.FixedZone("", offset)),
			time.Date(2016, time.January, 2, 5, 59, 59, 0, time.FixedZone("", offset)),
		}
		for idx, test := range tests {
			processor, err := newProcessor(test.Format, offset)
			require.NoError(t, err)
			processor.AdjustUTCOffset = true
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents,
				&protocol.Log_Content{Key: defaultSourceKey, Value: test.Value})
			processor.processLog(log)
			require.Equal(t, 0, logger.GetMemoryLogCount())
			require.Equal(t, int64(log.Time), results[idx].Unix())
		}
	}
}

func TestUTCOffset(t *testing.T) {
	invalidUTCOffsets := []int{
		-13 * 60 * 60, 19 * 60 * 60,
	}
	for idx, offset := range invalidUTCOffsets {
		_, err := newProcessor("%Y", offset)
		t.Log(err)
		require.Error(t, err, "Test case %v failed", idx)
	}

	validUTCOffsets := []int{
		8 * 60 * 60, -4 * 60 * 60, 12 * 60 * 60, -10 * 60 * 60,
	}
	for idx, offset := range validUTCOffsets {
		processor, err := newProcessor("%Y/%m/%d", offset)
		require.NoError(t, err, "Test case %v failed", idx)

		log := &protocol.Log{Time: 0}
		log.Contents = append(log.Contents,
			&protocol.Log_Content{Key: defaultSourceKey, Value: "2016/01/02"})
		processor.processLog(log)
		require.Equal(t, time.Date(
			2016, time.January, 2, 0, 0, 0, 0, time.FixedZone("FixedZone", offset)).Unix(),
			int64(log.Time), "Test case %v failed", idx)

		processor, err = newProcessor("%Y/%m/%d %z", offset)
		require.NoError(t, err, "Test case {} failed", idx)
		log.Contents[0].Value = "2016/01/02 +0400"
		processor.processLog(log)
		require.Equal(t, time.Date(
			2016, time.January, 1, 20, 0, 0, 0, time.FixedZone("FixedZone", offset)).Unix(),
			int64(log.Time), "Test case %v failed", idx)
	}
}

func TestKeepSource(t *testing.T) {
	processor, err := newProcessor("%Y/%m/%d", nilUTCOffset)
	require.NoError(t, err)

	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents,
		&protocol.Log_Content{Key: defaultSourceKey, Value: "2016/01/02"})
	processor.processLog(log)
	require.Equal(t, time.Date(
		2016, time.January, 2, 0, 0, 0, 0, time.Now().Location()).Unix(), int64(log.Time))
	require.Equal(t, 1, len(log.Contents))

	processor.KeepSource = false
	log.Time = 0
	processor.processLog(log)
	require.Equal(t, time.Date(
		2016, time.January, 2, 0, 0, 0, 0, time.Now().Location()).Unix(), int64(log.Time))
	require.Equal(t, 0, len(log.Contents))
}

func TestAlarmIfFail(t *testing.T) {
	processor, err := newProcessor("%Y/%m/%d", nilUTCOffset)
	require.NoError(t, err)

	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents,
		&protocol.Log_Content{Key: defaultSourceKey, Value: "/01/02"})
	processor.processLog(log)
	require.Equal(t, 1, logger.GetMemoryLogCount())

	processor.AlarmIfFail = false
	logger.ClearMemoryLog()
	processor.processLog(log)
	require.Equal(t, 0, logger.GetMemoryLogCount())
}
