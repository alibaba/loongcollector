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
	"strconv"
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

func newProcessor(format string, utcOffset int, enablePreciseTimestamp bool) (*Strptime, error) {
	ctx := mock.NewEmptyContext("p", "c", "l")
	processor := newStrptime()
	processor.Format = format
	if utcOffset != nilUTCOffset {
		processor.UTCOffset = utcOffset
		processor.AdjustUTCOffset = true
	}
	processor.EnablePreciseTimestamp = enablePreciseTimestamp
	err := processor.Init(ctx)
	return processor, err
}

func TestSourceKey(t *testing.T) {
	format := "%Y/%m/%d"
	processor, err := newProcessor(format, nilUTCOffset, true)
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
		if len(test.SourceKey) == 0 {
			require.Equal(t, len(log.Contents), len(test.Keys))
		} else {
			require.Equal(t, len(log.Contents), len(test.Keys)+1)
			require.Equal(t, log.GetContents()[len(test.Keys)].Key, processor.PreciseTimestampKey)
		}
	}
}

func TestFormat(t *testing.T) {
	time.Local = time.UTC
	logger.ClearMemoryLog()
	tests := []struct {
		InputTimeStr     string
		Format           string
		ExpectedTime     int64
		PreciseTimeStamp int64
	}{
		{"2016/01/02", "%Y/%m/%d",
			time.Date(2016, time.January, 2, 0, 0, 0, 0, time.UTC).Unix(),
			time.Date(2016, time.January, 2, 0, 0, 0, 0, time.UTC).UnixNano() / 1e6},
		{"2016/01/02 12:59:59", "%Y/%m/%d %H:%M:%S",
			time.Date(2016, time.January, 2, 12, 59, 59, 0, time.UTC).Unix(),
			time.Date(2016, time.January, 2, 12, 59, 59, 0, time.UTC).UnixNano() / 1e6},
		{"2016/01/02-12:59:59", "%Y/%m/%d-%H:%M:%S",
			time.Date(2016, time.January, 2, 12, 59, 59, 0, time.UTC).Unix(),
			time.Date(2016, time.January, 2, 12, 59, 59, 0, time.UTC).UnixNano() / 1e6},
		{"2016/01/02 12:59:59.1234", "%Y/%m/%d %H:%M:%S.%f",
			time.Date(2016, time.January, 2, 12, 59, 59, 123400000, time.UTC).Unix(),
			time.Date(2016, time.January, 2, 12, 59, 59, 123400000, time.UTC).UnixNano() / 1e6},
		{"2016/01/02 12:59:59.987654321 +0700 (UTC)", "%Y/%m/%d %H:%M:%S.%f %z (%Z)",
			time.Date(2016, time.January, 2, 12, 59, 59, 987654321, time.FixedZone("UTC+7", 7*60*60)).Unix(),
			time.Date(2016, time.January, 2, 12, 59, 59, 987654321, time.FixedZone("UTC+7", 7*60*60)).UnixNano() / 1e6},
		{"1451710799", "%s",
			time.Date(2016, time.January, 2, 12, 59, 59, 0, time.FixedZone("UTC+8", 8*60*60)).Unix(),
			time.Date(2016, time.January, 2, 12, 59, 59, 0, time.FixedZone("UTC+8", 8*60*60)).UnixNano() / 1e6},
		{"1451710799123", "%s",
			time.Date(2016, time.January, 2, 12, 59, 59, 0, time.FixedZone("UTC+8", 8*60*60)).Unix(),
			time.Date(2016, time.January, 2, 12, 59, 59, 0, time.FixedZone("UTC+8", 8*60*60)).UnixNano() / 1e6},
		{"1451710799123456", "%s",
			time.Date(2016, time.January, 2, 12, 59, 59, 0, time.FixedZone("UTC+8", 8*60*60)).Unix(),
			time.Date(2016, time.January, 2, 12, 59, 59, 0, time.FixedZone("UTC+8", 8*60*60)).UnixNano() / 1e6},
		{"2016/Jan/02 12:59:59,123456", "%Y/%b/%d %H:%M:%S,%f",
			time.Date(2016, time.January, 2, 12, 59, 59, 123456000, time.UTC).Unix(),
			time.Date(2016, time.January, 2, 12, 59, 59, 123456000, time.UTC).UnixNano() / 1e6},
		{"2019-07-15T04:16:47:123Z",
			"%Y-%m-%dT%H:%M:%S:%f",
			time.Date(2019, time.July, 15, 4, 16, 47, 123000000, time.UTC).Unix(),
			time.Date(2019, time.July, 15, 4, 16, 47, 123000000, time.UTC).UnixNano() / 1e6,
		},
	}
	for idx, test := range tests {
		processor, err := newProcessor(test.Format, nilUTCOffset, true)
		require.NoError(t, err)

		log := &protocol.Log{Time: 0}
		log.Contents = append(log.Contents,
			&protocol.Log_Content{Key: defaultSourceKey, Value: test.InputTimeStr})
		processor.processLog(log)
		require.Equal(t, 0, logger.GetMemoryLogCount())
		require.Equal(t, test.ExpectedTime, int64(log.Time), "Test case %v failed", idx)

		require.Equal(t, len(log.Contents), 2) // time + precise_timestamp
		require.Equal(t, defaultSourceKey, log.GetContents()[0].Key)
		require.Equal(t, test.InputTimeStr, log.GetContents()[0].Value)
		require.Equal(t, defaultPreciseTimestampKey, log.GetContents()[1].Key)
		require.Equal(t, strconv.FormatInt(test.PreciseTimeStamp, 10), log.GetContents()[1].Value)
	}
}

func TestUTCOffsetWithTimeZone(t *testing.T) {
	tests := []struct {
		Format       string
		InputTimeStr string
	}{
		{"%Y/%m/%d %H:%M:%S %f", "2016/01/02 12:59:59 1"},
		{"%Y/%m/%d %H:%M:%S.%f %z (%Z)", "2016/01/02 12:59:59.12 +0700 (UTC)"},
	}

	// Default processor, without UTCOffset configuration.
	{
		results := []time.Time{
			time.Date(2016, time.January, 2, 12, 59, 59, 100000000, time.Now().Location()),
			time.Date(2016, time.January, 2, 5, 59, 59, 120000000, time.FixedZone("UTC", 0)),
		}
		for idx, test := range tests {
			processor, err := newProcessor(test.Format, nilUTCOffset, true)
			require.NoError(t, err)
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents,
				&protocol.Log_Content{Key: defaultSourceKey, Value: test.InputTimeStr})
			processor.processLog(log)
			require.Equal(t, 0, logger.GetMemoryLogCount())
			require.Equal(t, results[idx].Unix(), int64(log.Time))

			require.Equal(t, len(log.Contents), 2) // time + precise_timestamp
			require.Equal(t, defaultSourceKey, log.GetContents()[0].Key)
			require.Equal(t, test.InputTimeStr, log.GetContents()[0].Value)
			require.Equal(t, defaultPreciseTimestampKey, log.GetContents()[1].Key)
			require.Equal(t, strconv.FormatInt(results[idx].UnixNano()/1e6, 10), log.GetContents()[1].Value)
		}
	}

	// Processor with UTCOffset enabled.
	{
		offset := -8 * 60 * 60
		results := []time.Time{
			time.Date(2016, time.January, 2, 12, 59, 59, 100000000, time.FixedZone("", offset)),
			time.Date(2016, time.January, 2, 5, 59, 59, 120000000, time.FixedZone("", offset)),
		}
		for idx, test := range tests {
			processor, err := newProcessor(test.Format, offset, true)
			require.NoError(t, err)
			processor.AdjustUTCOffset = true
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents,
				&protocol.Log_Content{Key: defaultSourceKey, Value: test.InputTimeStr})
			processor.processLog(log)
			require.Equal(t, 0, logger.GetMemoryLogCount())
			require.Equal(t, int64(log.Time), results[idx].Unix())

			require.Equal(t, len(log.Contents), 2) // time + precise_timestamp
			require.Equal(t, defaultSourceKey, log.GetContents()[0].Key)
			require.Equal(t, test.InputTimeStr, log.GetContents()[0].Value)
			require.Equal(t, defaultPreciseTimestampKey, log.GetContents()[1].Key)
			require.Equal(t, strconv.FormatInt(results[idx].UnixNano()/1e6, 10), log.GetContents()[1].Value)
		}
	}
}

func TestUTCOffset(t *testing.T) {
	invalidUTCOffsets := []int{
		-13 * 60 * 60, 19 * 60 * 60,
	}
	for idx, offset := range invalidUTCOffsets {
		_, err := newProcessor("%Y", offset, true)
		t.Log(err)
		require.Error(t, err, "Test case %v failed", idx)
	}

	validUTCOffsets := []int{
		8 * 60 * 60, -4 * 60 * 60, 12 * 60 * 60, -10 * 60 * 60,
	}
	for idx, offset := range validUTCOffsets {
		processor, err := newProcessor("%Y/%m/%d", offset, true)
		require.NoError(t, err, "Test case %v failed", idx)

		log := &protocol.Log{Time: 0}
		log.Contents = append(log.Contents,
			&protocol.Log_Content{Key: defaultSourceKey, Value: "2016/01/02"})
		processor.processLog(log)
		require.Equal(t, time.Date(
			2016, time.January, 2, 0, 0, 0, 0, time.FixedZone("FixedZone", offset)).Unix(),
			int64(log.Time), "Test case %v failed", idx)

		processor, err = newProcessor("%Y/%m/%d %z", offset, true)
		require.NoError(t, err, "Test case {} failed", idx)
		log.Contents[0].Value = "2016/01/02 +0400"
		processor.processLog(log)
		require.Equal(t, time.Date(
			2016, time.January, 1, 20, 0, 0, 0, time.FixedZone("FixedZone", offset)).Unix(),
			int64(log.Time), "Test case %v failed", idx)
	}
}

func TestKeepSource(t *testing.T) {
	processor, err := newProcessor("%Y/%m/%d", nilUTCOffset, true)
	require.NoError(t, err)
	{
		log := &protocol.Log{Time: 0}
		log.Contents = append(log.Contents,
			&protocol.Log_Content{Key: defaultSourceKey, Value: "2016/01/02"})
		processor.processLog(log)
		require.Equal(t, time.Date(
			2016, time.January, 2, 0, 0, 0, 0, time.Now().Location()).Unix(), int64(log.Time))
		require.Equal(t, 2, len(log.Contents)) // time + precise_timestamp
	}

	{
		log := &protocol.Log{Time: 0}
		log.Contents = append(log.Contents,
			&protocol.Log_Content{Key: defaultSourceKey, Value: "2016/01/02"})
		processor.KeepSource = false
		log.Time = 0
		processor.processLog(log)
		require.Equal(t, time.Date(
			2016, time.January, 2, 0, 0, 0, 0, time.Now().Location()).Unix(), int64(log.Time))
		require.Equal(t, 1, len(log.Contents)) // precise_timestamp
		require.Equal(t, defaultPreciseTimestampKey, log.GetContents()[0].Key)
	}
}

func TestAlarmIfFail(t *testing.T) {
	processor, err := newProcessor("%Y/%m/%d", nilUTCOffset, true)
	require.NoError(t, err)

	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents,
		&protocol.Log_Content{Key: defaultSourceKey, Value: "/01/02"})
	processor.processLog(log)
	require.Equal(t, 1, logger.GetMemoryLogCount())
	require.Equal(t, len(log.Contents), 1)

	processor.AlarmIfFail = false
	logger.ClearMemoryLog()
	processor.processLog(log)
	require.Equal(t, 0, logger.GetMemoryLogCount())
	require.Equal(t, len(log.Contents), 1)
}

func TestPreciseTimestamp(t *testing.T) {
	{
		time.Local = time.UTC
		logger.ClearMemoryLog()
		tests := []struct {
			InputTimeStr        string
			Format              string
			PreciseTimestampKey string
			PreciseTimeUint     string
			ExpectedTime        int64
			PreciseTimeStamp    int64
		}{
			{"2016/01/02", "%Y/%m/%d", defaultPreciseTimestampKey, timeStampMilliSecond,
				time.Date(2016, time.January, 2, 0, 0, 0, 0, time.UTC).Unix(),
				time.Date(2016, time.January, 2, 0, 0, 0, 0, time.UTC).UnixNano() / 1e6},
			{"2016/01/02 12:59:59", "%Y/%m/%d %H:%M:%S", defaultPreciseTimestampKey, timeStampMicroSecond,
				time.Date(2016, time.January, 2, 12, 59, 59, 0, time.UTC).Unix(),
				time.Date(2016, time.January, 2, 12, 59, 59, 0, time.UTC).UnixNano() / 1e3},
			{"2016/01/02-12:59:59.1", "%Y/%m/%d-%H:%M:%S.%f", defaultPreciseTimestampKey, timeStampNanoSecond,
				time.Date(2016, time.January, 2, 12, 59, 59, 100000000, time.UTC).Unix(),
				time.Date(2016, time.January, 2, 12, 59, 59, 100000000, time.UTC).UnixNano()},
			{"2016/01/02 12:59:59.1234", "%Y/%m/%d %H:%M:%S.%f", "new_key", timeStampMicroSecond,
				time.Date(2016, time.January, 2, 12, 59, 59, 123400000, time.UTC).Unix(),
				time.Date(2016, time.January, 2, 12, 59, 59, 123400000, time.UTC).UnixNano() / 1e3},
			{"2016/01/02 12:59:59.987654321 +0700 (UTC)", "%Y/%m/%d %H:%M:%S.%f %z (%Z)", "new_key", timeStampNanoSecond,
				time.Date(2016, time.January, 2, 12, 59, 59, 987654321, time.FixedZone("UTC+7", 7*60*60)).Unix(),
				time.Date(2016, time.January, 2, 12, 59, 59, 987654321, time.FixedZone("UTC+7", 7*60*60)).UnixNano()},
			{"1451710799", "%s", "new_key", timeStampNanoSecond,
				time.Date(2016, time.January, 2, 12, 59, 59, 0, time.FixedZone("UTC+8", 8*60*60)).Unix(),
				time.Date(2016, time.January, 2, 12, 59, 59, 0, time.FixedZone("UTC+8", 8*60*60)).UnixNano()},
			{"1451710799123", "%s", "new_key", timeStampMicroSecond,
				time.Date(2016, time.January, 2, 12, 59, 59, 0, time.FixedZone("UTC+8", 8*60*60)).Unix(),
				time.Date(2016, time.January, 2, 12, 59, 59, 0, time.FixedZone("UTC+8", 8*60*60)).UnixNano() / 1e3},
			{"1451710799123456", "%s", "new_key", timeStampMicroSecond,
				time.Date(2016, time.January, 2, 12, 59, 59, 0, time.FixedZone("UTC+8", 8*60*60)).Unix(),
				time.Date(2016, time.January, 2, 12, 59, 59, 0, time.FixedZone("UTC+8", 8*60*60)).UnixNano() / 1e3},
			{"2016/Jan/02 12:59:59,123456", "%Y/%b/%d %H:%M:%S,%f", "new_key", "",
				time.Date(2016, time.January, 2, 12, 59, 59, 123456000, time.UTC).Unix(),
				time.Date(2016, time.January, 2, 12, 59, 59, 123456000, time.UTC).UnixNano() / 1e6},
			{"2019-07-15T04:16:47:123Z", "%Y-%m-%dT%H:%M:%S:%f", "new_key", timeStampMicroSecond,
				time.Date(2019, time.July, 15, 4, 16, 47, 123000000, time.UTC).Unix(),
				time.Date(2019, time.July, 15, 4, 16, 47, 123000000, time.UTC).UnixNano() / 1e3,
			},
		}
		for idx, test := range tests {
			processor, err := newProcessor(test.Format, nilUTCOffset, true)
			processor.PreciseTimestampKey = test.PreciseTimestampKey
			processor.PreciseTimestampUnit = test.PreciseTimeUint
			require.NoError(t, err)

			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents,
				&protocol.Log_Content{Key: defaultSourceKey, Value: test.InputTimeStr})
			processor.processLog(log)
			require.Equal(t, 0, logger.GetMemoryLogCount())
			require.Equal(t, test.ExpectedTime, int64(log.Time), "Test case %v failed", idx)

			require.Equal(t, len(log.Contents), 2) // time + precise_timestamp
			require.Equal(t, defaultSourceKey, log.GetContents()[0].Key)
			require.Equal(t, test.InputTimeStr, log.GetContents()[0].Value)
			require.Equal(t, test.PreciseTimestampKey, log.GetContents()[1].Key)
			require.Equal(t, strconv.FormatInt(test.PreciseTimeStamp, 10), log.GetContents()[1].Value)
		}
	}

	{
		processor, _ := newProcessor("%Y/%m/%d", nilUTCOffset, false)
		log := &protocol.Log{Time: 0}
		log.Contents = append(log.Contents,
			&protocol.Log_Content{Key: defaultSourceKey, Value: "2016/01/02"})
		processor.KeepSource = false
		log.Time = 0
		processor.processLog(log)
		require.Equal(t, time.Date(
			2016, time.January, 2, 0, 0, 0, 0, time.Now().Location()).Unix(), int64(log.Time))
		require.Equal(t, 0, len(log.Contents)) // precise_timestamp
	}
}
