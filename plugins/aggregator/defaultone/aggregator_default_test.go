package defaultone

import (
	"fmt"
	"math/rand"
	"strconv"
	"strings"
	"testing"
	"time"

	. "github.com/smartystreets/goconvey/convey"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

var packIDPrefix = [3]string{"ABCDEFGHIJKLMNOP", "ALOEJDMGNYTDEWS", "VDSRGHUKMLQETGVD"}

type SliceQueue struct {
	logGroups []*protocol.LogGroup
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

func newAggregatorDefault() (*AggregatorDefault, *SliceQueue, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	que := &SliceQueue{}
	agg := NewAggregatorDefault()
	_, err := agg.Init(ctx, que)
	return agg, que, err
}

func TestAggregatorDefault(t *testing.T) {
	Convey("Given a default aggregator", t, func() {
		agg, que, err := newAggregatorDefault()
		So(err, ShouldBeNil)

		Convey("When no quick flush happens", func() {
			logNo := make([]int, len(packIDPrefix))
			generateLogs(agg, 1000, true, logNo)
			logGroups := que.PopAll()
			logGroups = append(logGroups, agg.Flush()...)
			generateLogs(agg, 2000, true, logNo)
			logGroups = append(logGroups, que.PopAll()...)
			logGroups = append(logGroups, agg.Flush()...)

			Convey("Then each logGroup should contain logs from the same source with chronological order", func() {
				checkResult(logGroups, 3000)
			})
		})

		Convey("When quick flush happens", func() {
			logNo := make([]int, len(packIDPrefix))
			generateLogs(agg, 50000, true, logNo)
			logGroups := que.PopAll()
			logGroups = append(logGroups, agg.Flush()...)

			Convey("Then each logGroup should contain logs from the same source with chronological order", func() {
				checkResult(logGroups, 50000)
			})
		})

		Convey("When no source information is provided", func() {
			logNo := make([]int, len(packIDPrefix))
			generateLogs(agg, 20000, false, logNo)
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
		agg, que, err := newAggregatorDefault()
		So(err, ShouldBeNil)
		agg.PackFlag = false

		Convey("When logs are added", func() {
			logNo := make([]int, len(packIDPrefix))
			generateLogs(agg, 20000, true, logNo)
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

func generateLogs(agg *AggregatorDefault, logNum int, withCtx bool, logNo []int) {
	for i := 0; i < logNum; i++ {
		index := rand.Intn(len(packIDPrefix))
		log := &protocol.Log{Time: uint32(time.Now().Unix())}
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: "This log comes from source " + fmt.Sprintf("%d", index)})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "no", Value: fmt.Sprintf("%d", logNo[index]+1)})
		if withCtx {
			ctx := map[string]interface{}{"source": packIDPrefix[index] + "-"}
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

		packIDComponents := strings.Split(packID, "-")
		So(packIDComponents, ShouldHaveLength, 2)

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
