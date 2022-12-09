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

package http

import (
	"context"
	"io/ioutil"
	"net/http"
	"testing"
	"time"

	"github.com/jarcoal/httpmock"
	. "github.com/smartystreets/goconvey/convey"
	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
)

func TestHttpFlusherInit(t *testing.T) {
	Convey("Given a http flusher with empty RemoteURL", t, func() {
		flusher := &FlusherHTTP{
			RemoteURL: "",
		}
		Convey("Then Init() should return error", func() {
			err := flusher.Init(mockContext{})
			So(err, ShouldNotBeNil)
		})
	})

	Convey("Given a http flusher with empty converter", t, func() {
		flusher := &FlusherHTTP{
			RemoteURL: "http://localhost:8086/write",
			Convert:   helper.ConvertConfig{},
		}
		Convey("Then Init() should return error", func() {
			err := flusher.Init(mockContext{})
			So(err, ShouldNotBeNil)
		})
	})

	Convey("Given a http flusher with empty converter", t, func() {
		flusher := &FlusherHTTP{
			RemoteURL: "http://localhost:8086/write",
			Convert:   helper.ConvertConfig{},
		}
		Convey("Then Init() should return error", func() {
			err := flusher.Init(mockContext{})
			So(err, ShouldNotBeNil)
		})
	})

	Convey("Given a http flusher with Query contains variable ", t, func() {
		flusher := &FlusherHTTP{
			RemoteURL: "http://localhost:8086/write",
			Convert: helper.ConvertConfig{
				Protocol: converter.ProtocolCustomSingle,
				Encoding: converter.EncodingJSON,
			},
			Timeout:     defaultTimeout,
			Concurrency: 1,
			Query: map[string]string{
				"name": "_%{var}",
			},
		}
		Convey("Then Init() should build the variable keys", func() {
			err := flusher.Init(mockContext{})
			So(err, ShouldBeNil)
			So(flusher.queryVarKeys, ShouldResemble, []string{"var"})
		})
	})
}

func TestHttpFlusherFlush(t *testing.T) {
	Convey("Given a http flusher with protocol: Influxdb, encoding: custom, query: contains variable '%{tag.db}'", t, func() {

		var actualRequests []string
		httpmock.Activate()
		defer httpmock.DeactivateAndReset()

		httpmock.RegisterResponder("POST", "http://test.com/write?db=mydb", func(req *http.Request) (*http.Response, error) {
			body, _ := ioutil.ReadAll(req.Body)
			actualRequests = append(actualRequests, string(body))
			return httpmock.NewStringResponse(200, "ok"), nil
		})

		flusher := &FlusherHTTP{
			RemoteURL: "http://test.com/write",
			Convert: helper.ConvertConfig{
				Protocol: converter.ProtocolInfluxdb,
				Encoding: converter.EncodingCustom,
			},
			Timeout:     defaultTimeout,
			Concurrency: 1,
			Query: map[string]string{
				"db": "%{tag.db}",
			},
		}

		err := flusher.Init(mockContext{})
		So(err, ShouldBeNil)

		Convey("When Flush with logGroupList contains 2 valid Log in influxdb format metrics, each with LogTag: '__tag__:db'", func() {
			logGroups := []*protocol.LogGroup{
				{
					Logs: []*protocol.Log{
						{
							Contents: []*protocol.Log_Content{
								{Key: "__time_nano__", Value: "1668653452000000000"},
								{Key: "__name__", Value: "weather"},
								{Key: "__labels__", Value: "location#$#hangzhou|province#$#zhejiang"},
								{Key: "__value__", Value: "30"},
							},
						},
						{
							Contents: []*protocol.Log_Content{
								{Key: "__time_nano__", Value: "1668653452000000001"},
								{Key: "__name__", Value: "weather"},
								{Key: "__labels__", Value: "location#$#hangzhou|province#$#zhejiang"},
								{Key: "__value__", Value: "32"},
							},
						},
					},
					LogTags: []*protocol.LogTag{{Key: "__tag__:db", Value: "mydb"}},
				},
				{
					Logs: []*protocol.Log{
						{
							Contents: []*protocol.Log_Content{
								{Key: "__time_nano__", Value: "1668653452000000003"},
								{Key: "__name__", Value: "weather"},
								{Key: "__labels__", Value: "location#$#hangzhou|province#$#zhejiang"},
								{Key: "__value__", Value: "30"},
							},
						},
						{
							Contents: []*protocol.Log_Content{
								{Key: "__time_nano__", Value: "1668653452000000004"},
								{Key: "__name__", Value: "weather"},
								{Key: "__labels__", Value: "location#$#hangzhou|province#$#zhejiang"},
								{Key: "__value__", Value: "32"},
							},
						},
					},
					LogTags: []*protocol.LogTag{{Key: "__tag__:db", Value: "mydb"}},
				},
			}

			err := flusher.Flush("", "", "", logGroups)
			flusher.Stop()
			Convey("Then", func() {
				Convey("Flush() should not return error", func() {
					So(err, ShouldBeNil)
				})

				Convey("each logGroup should be sent as one single request", func() {
					reqCount := httpmock.GetTotalCallCount()
					So(reqCount, ShouldEqual, 2)
				})

				Convey("each http request body should be valid as expect", func() {
					So(actualRequests, ShouldResemble, []string{
						"weather,location=hangzhou,province=zhejiang value=30 1668653452000000000\n" +
							"weather,location=hangzhou,province=zhejiang value=32 1668653452000000001\n",

						"weather,location=hangzhou,province=zhejiang value=30 1668653452000000003\n" +
							"weather,location=hangzhou,province=zhejiang value=32 1668653452000000004\n",
					})
				})
			})
		})
	})

	Convey("Given a http flusher with protocol: custom_single, encoding: json, query: contains variable '%{tag.db}'", t, func() {

		var actualRequests []string
		httpmock.Activate()
		defer httpmock.DeactivateAndReset()

		httpmock.RegisterResponder("POST", "http://test.com/write?db=mydb", func(req *http.Request) (*http.Response, error) {
			body, _ := ioutil.ReadAll(req.Body)
			actualRequests = append(actualRequests, string(body))
			return httpmock.NewStringResponse(200, "ok"), nil
		})

		flusher := &FlusherHTTP{
			RemoteURL: "http://test.com/write",
			Convert: helper.ConvertConfig{
				Protocol: converter.ProtocolCustomSingle,
				Encoding: converter.EncodingJSON,
			},
			Timeout:     defaultTimeout,
			Concurrency: 1,
			Query: map[string]string{
				"db": "%{tag.db}",
			},
		}

		err := flusher.Init(mockContext{})
		So(err, ShouldBeNil)

		Convey("When Flush with logGroupList contains 2 valid Log in influxdb format metrics, each LOG with Content: '__tag__:db'", func() {
			logGroups := []*protocol.LogGroup{
				{
					Logs: []*protocol.Log{
						{
							Contents: []*protocol.Log_Content{
								{Key: "__time_nano__", Value: "1668653452000000000"},
								{Key: "__name__", Value: "weather"},
								{Key: "__labels__", Value: "location#$#hangzhou|province#$#zhejiang"},
								{Key: "__value__", Value: "30"},
								{Key: "__tag__:db", Value: "mydb"},
							},
						},
						{
							Contents: []*protocol.Log_Content{
								{Key: "__time_nano__", Value: "1668653452000000001"},
								{Key: "__name__", Value: "weather"},
								{Key: "__labels__", Value: "location#$#hangzhou|province#$#zhejiang"},
								{Key: "__value__", Value: "32"},
								{Key: "__tag__:db", Value: "mydb"},
							},
						},
					},
				},
				{
					Logs: []*protocol.Log{
						{
							Contents: []*protocol.Log_Content{
								{Key: "__time_nano__", Value: "1668653452000000003"},
								{Key: "__name__", Value: "weather"},
								{Key: "__labels__", Value: "location#$#hangzhou|province#$#zhejiang"},
								{Key: "__value__", Value: "30"},
								{Key: "__tag__:db", Value: "mydb"},
							},
						},
						{
							Contents: []*protocol.Log_Content{
								{Key: "__time_nano__", Value: "1668653452000000004"},
								{Key: "__name__", Value: "weather"},
								{Key: "__labels__", Value: "location#$#hangzhou|province#$#zhejiang"},
								{Key: "__value__", Value: "32"},
								{Key: "__tag__:db", Value: "mydb"},
							},
						},
					},
				},
			}

			err := flusher.Flush("", "", "", logGroups)
			flusher.Stop()
			Convey("Then", func() {
				Convey("Flush() should not return error", func() {
					So(err, ShouldBeNil)
				})

				Convey("each log in logGroups should be sent as one single request", func() {
					reqCount := httpmock.GetTotalCallCount()
					So(reqCount, ShouldEqual, 4)
				})

				Convey("each http request body should be valid as expect", func() {
					So(actualRequests, ShouldResemble, []string{
						`{"contents":{"__labels__":"location#$#hangzhou|province#$#zhejiang","__name__":"weather","__time_nano__":"1668653452000000000","__value__":"30"},"tags":{"db":"mydb","host.ip":""},"time":0}`,
						`{"contents":{"__labels__":"location#$#hangzhou|province#$#zhejiang","__name__":"weather","__time_nano__":"1668653452000000001","__value__":"32"},"tags":{"db":"mydb","host.ip":""},"time":0}`,
						`{"contents":{"__labels__":"location#$#hangzhou|province#$#zhejiang","__name__":"weather","__time_nano__":"1668653452000000003","__value__":"30"},"tags":{"db":"mydb","host.ip":""},"time":0}`,
						`{"contents":{"__labels__":"location#$#hangzhou|province#$#zhejiang","__name__":"weather","__time_nano__":"1668653452000000004","__value__":"32"},"tags":{"db":"mydb","host.ip":""},"time":0}`,
					})
				})
			})
		})
	})
}

func TestGetNextRetryDelay(t *testing.T) {
	f := &FlusherHTTP{
		Retry: retryConfig{
			Enable:        true,
			MaxRetryTimes: 3,
			InitialDelay:  time.Second,
			MaxDelay:      3 * time.Second,
		},
	}

	for i := 0; i < 1000; i++ {
		delay := f.getNextRetryDelay(0)
		assert.GreaterOrEqual(t, delay, time.Second/2)
		assert.LessOrEqual(t, delay, time.Second)

		delay = f.getNextRetryDelay(1)
		assert.GreaterOrEqual(t, delay, time.Second)
		assert.LessOrEqual(t, delay, 2*time.Second)

		delay = f.getNextRetryDelay(2)
		assert.GreaterOrEqual(t, delay, 3*time.Second/2)
		assert.LessOrEqual(t, delay, 3*time.Second)

		delay = f.getNextRetryDelay(3)
		assert.GreaterOrEqual(t, delay, 3*time.Second/2)
		assert.LessOrEqual(t, delay, 3*time.Second)
	}
}

type mockContext struct {
	ilogtail.Context
}

func (c mockContext) GetConfigName() string {
	return "ctx"
}

func (c mockContext) GetRuntimeContext() context.Context {
	return context.Background()
}
