// Copyright 2022 iLogtail Authors
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

package context

import (
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"strconv"
	"strings"
	"testing"
	"time"

	. "github.com/smartystreets/goconvey/convey"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

var packIDPrefix = [3]string{"ABCDEFGHIJKLMNOP", "ALOEJDMGNYTDEWS", "VDSRGHUKMLQETGVD"}

var (
	shortLog  = "This is short log. This log comes from source "
	mediumLog = strings.Repeat("This is medium log. ", 20) + "This log comes from source "
	longLog   = strings.Repeat("This is long log. ", 200) + "This log comes from source "
)

type SliceQueue struct {
	logGroups []*protocol.LogGroup
}

func NewSliceQueue() *SliceQueue {
	return &SliceQueue{
		logGroups: make([]*protocol.LogGroup, 0, 10),
	}
}

func (q *SliceQueue) Add(logGroup *protocol.LogGroup) error {
	q.logGroups = append(q.logGroups, logGroup)
	return nil
}

func (q *SliceQueue) AddWithWait(logGroup *protocol.LogGroup, duration time.Duration) error {
	q.logGroups = append(q.logGroups, logGroup)
	return nil
}

func (q *SliceQueue) PopAll() []*protocol.LogGroup {
	return q.logGroups
}

type contextInfo struct {
	log     string
	packSeq int
	logSeq  int
}

func newAggregatorContext() (*AggregatorContext, *SliceQueue, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	que := NewSliceQueue()
	agg := NewAggregatorContext()
	_, err := agg.Init(ctx, que)
	return agg, que, err
}

func TestAggregatorDefault(t *testing.T) {
	Convey("Given a default aggregator", t, func() {
		agg, que, err := newAggregatorContext()
		So(err, ShouldBeNil)

		Convey("When log producing pace is slow and each log is relatively small", func() {
			logNo := make([]int, len(packIDPrefix))
			generateLogs(agg, 900, true, logNo, true)
			logGroups := agg.Flush()
			generateLogs(agg, 1800, true, logNo, true)
			logGroups = append(logGroups, agg.Flush()...)

			Convey("Then no quick flush happens, and each logGroup should contain logs from the same source with chronological order", func() {
				So(logGroups, ShouldHaveLength, 6)
				checkResult(logGroups, 2700)
			})
		})

		Convey("When log producing pace is fast but each log is relatively small", func() {
			logNo := make([]int, len(packIDPrefix))
			generateLogs(agg, 18432, true, logNo, true) // 1024 * 6 * 3
			logGroups := que.PopAll()
			logGroups = append(logGroups, agg.Flush()...)

			Convey("Then quick flush happens, and each logGroup should contain logs from the same source with chronological order", func() {
				So(logGroups, ShouldHaveLength, 18)
				checkResult(logGroups, 18432)
			})
		})

		Convey("When log producing pace is slow but each log is relatively large", func() {
			logNo := make([]int, len(packIDPrefix))
			generateLogs(agg, 9216, true, logNo, false) // 1024 * 3 * 3
			logGroups := que.PopAll()
			logGroups = append(logGroups, agg.Flush()...)

			Convey("Then quick flush happens, and each logGroup should contain logs from the same source with chronological order", func() {
				So(logGroups, ShouldHaveLength, 21)
				checkResult(logGroups, 9216)
			})
		})

		Convey("When no source information is provided", func() {
			logNo := make([]int, len(packIDPrefix))
			generateLogs(agg, 20000, false, logNo, true)
			logGroups := que.PopAll()
			logGroups = append(logGroups, agg.Flush()...)

			Convey("Then each logGroup will contain logs from different source", func() {
				logSum := 0
				for _, logGroup := range logGroups {
					packIDTagFound := false
					for _, tag := range logGroup.LogTags {
						if tag.GetKey() == "__pack_id__" {
							packIDTagFound = true
							break
						}
					}
					So(packIDTagFound, ShouldBeTrue)

					logSum += len(logGroup.Logs)
				}
				So(logSum, ShouldEqual, 20000)
			})
		})
	})

	Convey("Given an aggregator without packID", t, func() {
		agg, que, err := newAggregatorContext()
		So(err, ShouldBeNil)
		agg.PackFlag = false

		Convey("When logs are added", func() {
			logNo := make([]int, len(packIDPrefix))
			generateLogs(agg, 20000, true, logNo, true)
			logGroups := que.PopAll()
			logGroups = append(logGroups, agg.Flush()...)

			Convey("Then each logGroup will contain logs from different source", func() {
				logSum := 0
				for _, logGroup := range logGroups {
					packIDTagFound := false
					for _, tag := range logGroup.LogTags {
						if tag.GetKey() == "__pack_id__" {
							packIDTagFound = true
							break
						}
					}
					So(packIDTagFound, ShouldBeFalse)

					logSum += len(logGroup.Logs)
				}
				So(logSum, ShouldEqual, 20000)
			})
		})
	})
}

func generateLogs(agg *AggregatorContext, logNum int, withCtx bool, logNo []int, isShort bool) {
	for i := 0; i < logNum; i++ {
		index := i % len(packIDPrefix)
		nowTime := time.Now()
		log := &protocol.Log{}
		protocol.SetLogTime(log, uint32(nowTime.Unix()))
		if isShort {
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: shortLog + fmt.Sprintf("%d", index)})
		} else {
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: longLog + fmt.Sprintf("%d", index)})
		}
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "no", Value: fmt.Sprintf("%d", logNo[index]+1)})
		if withCtx {
			ctx := map[string]interface{}{"source": packIDPrefix[index] + "-", "topic": "file", "tags": []*protocol.LogTag{{
				Key:   "testTag",
				Value: packIDPrefix[index],
			}}}
			agg.Add(log, ctx)
		} else {
			agg.Add(log, nil)
		}
		logNo[index]++
		time.Sleep(time.Duration(1) * time.Microsecond)
	}
}

func checkResult(logGroups []*protocol.LogGroup, expectedLogNum int) {
	contextInfoMap := make(map[string]*contextInfo)
	for _, logGroup := range logGroups {
		packID, packIDTagFound := "", false
		for _, tag := range logGroup.LogTags {
			if tag.GetKey() == "__pack_id__" {
				packID = tag.GetValue()
				packIDTagFound = true
				break
			}
		}
		So(packIDTagFound, ShouldBeTrue)

		tagFound, tagTest := false, ""
		for _, tag := range logGroup.LogTags {
			if tag.GetKey() == "testTag" {
				tagTest = tag.GetValue()
				tagFound = true
				break
			}
		}
		So(tagFound, ShouldBeTrue)

		packIDComponents := strings.Split(packID, "-")
		So(packIDComponents, ShouldHaveLength, 2)
		So(packIDComponents[0], ShouldEqual, tagTest)
		ctxInfo, ok := contextInfoMap[packIDComponents[0]]
		if !ok {
			ctxInfo = &contextInfo{
				packSeq: 0,
				logSeq:  0,
			}

			contentLogKeyfound := false
			for _, logContent := range logGroup.Logs[0].Contents {
				if logContent.Key == "content" {
					ctxInfo.log = logContent.Value
					contentLogKeyfound = true
					break
				}
			}
			So(contentLogKeyfound, ShouldBeTrue)
		}

		packIDNo, err := strconv.ParseInt(packIDComponents[1], 16, 0)
		So(err, ShouldBeNil)
		So(packIDNo, ShouldEqual, ctxInfo.packSeq+1)

		for _, log := range logGroup.Logs {
			contentFound, seqFound := false, false
			for _, logContent := range log.Contents {
				if logContent.Key == "content" {
					contentFound = true
					So(logContent.Value, ShouldEqual, ctxInfo.log)
				} else if logContent.Key == "no" {
					seqFound = true
					no, err := strconv.Atoi(logContent.Value)
					So(err, ShouldBeNil)
					So(no, ShouldEqual, ctxInfo.logSeq+1)
				}
			}
			So(contentFound, ShouldBeTrue)
			So(seqFound, ShouldBeTrue)
			ctxInfo.logSeq++
		}

		ctxInfo.packSeq++
		contextInfoMap[packIDComponents[0]] = ctxInfo
	}

	So(contextInfoMap, ShouldHaveLength, len(packIDPrefix))
	logSum := 0
	for _, ctxInfo := range contextInfoMap {
		logSum += ctxInfo.logSeq
	}
	So(logSum, ShouldEqual, expectedLogNum)
}

// common situation: 10 log sources, 10 logs per second per file, medium log
func BenchmarkAdd(b *testing.B) {
	agg, _, _ := newAggregatorContext()
	nowTime := time.Now()
	log := &protocol.Log{
		Contents: []*protocol.Log_Content{{Key: "content", Value: mediumLog}},
	}
	protocol.SetLogTime(log, uint32(nowTime.Unix()))
	ctx := make([]map[string]interface{}, 10)
	packIDPrefix := make([]byte, 8)
	for i := 0; i < 10; i++ {
		rand.Read(packIDPrefix)
		ctx[i] = map[string]interface{}{"source": hex.EncodeToString(packIDPrefix) + "-", "topic": "file"}
	}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		for j := 0; j < 300; j++ {
			agg.Add(log, ctx[j%10])
		}
	}
}

// 1 log source
func BenchmarkLogSource1(b *testing.B) {
	benchmarkLogSource(b, 1)
}

// 100 log sources
func BenchmarkLogSource100(b *testing.B) {
	benchmarkLogSource(b, 100)
}

func benchmarkLogSource(b *testing.B, num int) {
	agg, _, _ := newAggregatorContext()
	nowTime := time.Now()
	log := &protocol.Log{
		Contents: []*protocol.Log_Content{{Key: "content", Value: mediumLog}},
	}
	protocol.SetLogTime(log, uint32(nowTime.Unix()))
	ctx := make([]map[string]interface{}, num)
	packIDPrefix := make([]byte, 8)
	for i := 0; i < num; i++ {
		rand.Read(packIDPrefix)
		ctx[i] = map[string]interface{}{"source": hex.EncodeToString(packIDPrefix) + "-", "topic": "file"}
	}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		for j := 0; j < 30*num; j++ {
			agg.Add(log, ctx[j%num])
		}
		_ = agg.Flush()
	}
}

// 1 log per second per file
func BenchmarkLogProducinfPace1(b *testing.B) {
	benchmarkLogProducingPace(b, 1)
}

// 1000 logs per second per file
func BenchmarkLogProducinfPace1000(b *testing.B) {
	benchmarkLogProducingPace(b, 1000)
}

func benchmarkLogProducingPace(b *testing.B, num int) {
	agg, _, _ := newAggregatorContext()
	nowTime := time.Now()
	log := &protocol.Log{
		Contents: []*protocol.Log_Content{{Key: "content", Value: mediumLog}},
	}
	protocol.SetLogTime(log, uint32(nowTime.Unix()))
	ctx := make([]map[string]interface{}, 10)
	packIDPrefix := make([]byte, 8)
	for i := 0; i < 10; i++ {
		rand.Read(packIDPrefix)
		ctx[i] = map[string]interface{}{"source": hex.EncodeToString(packIDPrefix) + "-", "topic": "file"}
	}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		for j := 0; j < 3*num*10; j++ {
			agg.Add(log, ctx[j%10])
		}
		_ = agg.Flush()
	}
}

func BenchmarkLogLengthShort(b *testing.B) {
	benchmarkLogLength(b, "short")
}

func BenchmarkLogLengthLong(b *testing.B) {
	benchmarkLogLength(b, "long")
}

func benchmarkLogLength(b *testing.B, len string) {
	agg, _, _ := newAggregatorContext()
	var value string
	switch len {
	case "short":
		value = shortLog
	case "long":
		value = longLog
	default:
		value = mediumLog
	}
	nowTime := time.Now()
	log := &protocol.Log{
		Contents: []*protocol.Log_Content{{Key: "content", Value: value}},
	}
	protocol.SetLogTime(log, uint32(nowTime.Unix()))
	ctx := make([]map[string]interface{}, 10)
	packIDPrefix := make([]byte, 8)
	for i := 0; i < 10; i++ {
		rand.Read(packIDPrefix)
		ctx[i] = map[string]interface{}{"source": hex.EncodeToString(packIDPrefix) + "-", "topic": "file"}
	}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		for j := 0; j < 300; j++ {
			agg.Add(log, ctx[j%10])
		}
		_ = agg.Flush()
	}
}
