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

package contentvaluegroup

import (
	"context"
	"sync"
	"testing"
	"time"

	. "github.com/smartystreets/goconvey/convey"
	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

func TestGroupAggregatorInit(t *testing.T) {
	Convey("Given a group aggregator without GroupKeys", t, func() {
		agg := &AggregatorContentValueGroup{}
		Convey("When Init(), should return error", func() {
			_, err := agg.Init(nil, nil)
			So(err, ShouldNotBeNil)
		})
	})

	Convey("Given a group aggregator with valid GroupKeys", t, func() {
		agg := &AggregatorContentValueGroup{GroupKeys: []string{"a", "b"}}
		Convey("When Init(), should not return error", func() {
			_, err := agg.Init(nil, nil)
			So(err, ShouldBeNil)
		})
	})
}

func TestGroupAggregatorAdd(t *testing.T) {
	Convey("Given a group aggregator with groupKeys:[a,b]", t, func() {
		agg := &AggregatorContentValueGroup{
			GroupKeys: []string{"ka", "kb"},
			lock:      &sync.Mutex{},
		}
		que := &mockQueue{}
		agg.Init(mockContext{}, que)

		Convey("When add 100 logs with mix labeled, then", func() {
			count := 100
			for i := 0; i < count; i++ {
				if i%2 == 0 {
					agg.Add(&protocol.Log{
						Contents: []*protocol.Log_Content{
							{Key: "ka", Value: "a"},
						},
					}, nil)
				} else {
					agg.Add(&protocol.Log{
						Contents: []*protocol.Log_Content{
							{Key: "kb", Value: "b"},
						},
					}, nil)
				}
			}

			Convey("Flush() should get all 100 re-grouped logs", func() {
				logGroups := agg.Flush()
				So(len(logGroups), ShouldEqual, 2)
				for _, group := range logGroups {
					tag := findFirstNoneEmptyLogTag(group, "ka", "kb")
					So(tag.Key, ShouldBeIn, []string{"ka", "kb"})
					So(len(group.Logs), ShouldEqual, 50)

					for _, log := range group.Logs {
						assert.Equal(t, log.Contents[0].Key, tag.Key)
					}
				}
			})

			Convey("no logs should be auto add to logGroupQueue", func() {
				So(que.logGroups, ShouldBeEmpty)
			})
		})

		Convey("When add 4097 logs with same labeled, then", func() {
			count := 4097
			for i := 0; i < count; i++ {
				agg.Add(&protocol.Log{
					Contents: []*protocol.Log_Content{
						{Key: "ka", Value: "a"},
					},
				}, nil)
			}

			Convey("Flush() should get 3073 logs", func() {
				logGroups := agg.Flush()
				var num int
				So(len(logGroups), ShouldEqual, 4)
				for _, group := range logGroups {
					tag := findFirstNoneEmptyLogTag(group, "ka")
					So(tag.Key, ShouldBeIn, []string{"ka"})

					for _, log := range group.Logs {
						num++
						assert.Equal(t, log.Contents[0].Key, tag.Key)
					}
				}
				So(num, ShouldEqual, 3073)
			})

			Convey("1024 logs should be added to logGroupQueue", func() {
				So(len(que.logGroups), ShouldEqual, 1)
				So(len(que.logGroups[0].Logs), ShouldEqual, 1024)
			})
		})
	})
}

func findFirstNoneEmptyLogTag(logGroup *protocol.LogGroup, tagKeys ...string) *protocol.LogTag {
	for _, tag := range logGroup.LogTags {
		for _, key := range tagKeys {
			if tag.GetKey() == key && len(tag.GetValue()) > 0 {
				return tag
			}
		}
	}
	return nil
}

type mockContext struct {
	pipeline.Context
}

func (c mockContext) GetConfigName() string {
	return "ctx"
}

func (c mockContext) GetRuntimeContext() context.Context {
	return context.Background()
}

type mockQueue struct {
	pipeline.LogGroupQueue
	logGroups []*protocol.LogGroup
}

func (q *mockQueue) Add(loggroup *protocol.LogGroup) error {
	q.logGroups = append(q.logGroups, loggroup)
	return nil
}

func (q *mockQueue) AddWithWait(loggroup *protocol.LogGroup, duration time.Duration) error {
	return q.Add(loggroup)
}
