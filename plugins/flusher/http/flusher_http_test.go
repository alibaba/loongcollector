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
	"fmt"
	"io"
	"net/http"
	"sort"
	"testing"
	"time"

	"github.com/jarcoal/httpmock"
	. "github.com/smartystreets/goconvey/convey"
	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/pipeline/extensions"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
	"github.com/alibaba/ilogtail/plugins/test/mock"
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
			So(flusher.varKeys, ShouldResemble, []string{"var"})
		})
	})

	Convey("Given a http flusher with Headers contains variable ", t, func() {
		flusher := &FlusherHTTP{
			RemoteURL: "http://localhost:8086/write",
			Convert: helper.ConvertConfig{
				Protocol: converter.ProtocolCustomSingle,
				Encoding: converter.EncodingJSON,
			},
			Timeout:     defaultTimeout,
			Concurrency: 1,
			Headers: map[string]string{
				"name": "_%{var}",
			},
		}
		Convey("Then Init() should build the variable keys", func() {
			err := flusher.Init(mockContext{})
			So(err, ShouldBeNil)
			So(flusher.varKeys, ShouldResemble, []string{"var"})
		})
	})

	Convey("Given a http flusher with Query AND Headers contains variable ", t, func() {
		flusher := &FlusherHTTP{
			RemoteURL: "http://localhost:8086/write",
			Convert: helper.ConvertConfig{
				Protocol: converter.ProtocolCustomSingle,
				Encoding: converter.EncodingJSON,
			},
			Timeout:     defaultTimeout,
			Concurrency: 1,
			Query: map[string]string{
				"name": "_%{var1}",
			},
			Headers: map[string]string{
				"name": "_%{var2}",
				"tt":   "_%{var1}",
			},
		}
		Convey("Then Init() should build the variable keys", func() {
			err := flusher.Init(mockContext{})
			sort.Strings(flusher.varKeys)
			So(err, ShouldBeNil)
			So(flusher.varKeys, ShouldResemble, []string{"var1", "var2"})
		})
	})
}

func TestHttpFlusherFlush(t *testing.T) {
	Convey("Given a http flusher with protocol: Influxdb, encoding: custom, query: contains variable '%{tag.db}'", t, func() {

		var actualRequests []string
		httpmock.Activate()
		defer httpmock.DeactivateAndReset()

		httpmock.RegisterResponder("POST", "http://test.com/write?db=mydb", func(req *http.Request) (*http.Response, error) {
			assert.Equal(t, "mydb", req.Header.Get("db"))
			body, _ := io.ReadAll(req.Body)
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
			Headers: map[string]string{
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
			body, _ := io.ReadAll(req.Body)
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

func TestHttpFlusherFlushWithAuthenticator(t *testing.T) {
	Convey("Given a http flusher with protocol: Influxdb, encoding: custom,with basicauth authenticator, query: contains variable '%{tag.db}'", t, func() {

		var actualRequests []string
		httpmock.Activate()
		defer httpmock.DeactivateAndReset()

		httpmock.RegisterResponder("POST", "http://test.com/write?db=mydb", func(req *http.Request) (*http.Response, error) {
			assert.Equal(t, req.Header.Get("Authorization"), "Basic dXNlcjE6cHdkMQ==")
			body, _ := io.ReadAll(req.Body)
			actualRequests = append(actualRequests, string(body))
			return httpmock.NewStringResponse(200, "ok"), nil
		})

		authenticator := &basicAuth{Username: "user1", Password: "pwd1"}

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
			Authenticator: &extensions.ExtensionConfig{
				Type: "ext_basicauth",
			},
		}

		err := flusher.Init(mockContext{basicAuth: authenticator})
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
}

func TestHttpFlusherExport(t *testing.T) {
	Convey("Given a http flusher with Convert.Protocol: Raw, Convert.Encoding: Custom, Query: '%{metadata.db}'", t, func() {
		var actualRequests []string
		httpmock.Activate()
		defer httpmock.DeactivateAndReset()

		httpmock.RegisterResponder("POST", "http://test.com/write?db=mydb", func(req *http.Request) (*http.Response, error) {
			body, _ := io.ReadAll(req.Body)
			actualRequests = append(actualRequests, string(body))
			return httpmock.NewStringResponse(200, "ok"), nil
		})

		flusher := &FlusherHTTP{
			RemoteURL: "http://test.com/write",
			Convert: helper.ConvertConfig{
				Protocol: converter.ProtocolRaw,
				Encoding: converter.EncodingCustom,
			},
			Timeout:     defaultTimeout,
			Concurrency: 1,
			Query: map[string]string{
				"db": "%{metadata.db}",
			},
		}

		err := flusher.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		mockMetric1 := "cpu.load.short,host=server01,region=cn value=0.6 1672321328000000000"
		mockMetric2 := "cpu.load.short,host=server01,region=cn value=0.2 1672321358000000000"
		mockMetadata := models.NewMetadataWithKeyValues("db", "mydb")

		Convey("Export a single byte events each GroupEvents with Metadata {db: mydb}", func() {
			groupEventsArray := []*models.PipelineGroupEvents{
				{
					Group:  models.NewGroup(mockMetadata, nil),
					Events: []models.PipelineEvent{models.ByteArray(mockMetric1)},
				},
				{
					Group:  models.NewGroup(mockMetadata, nil),
					Events: []models.PipelineEvent{models.ByteArray(mockMetric2)},
				},
			}
			httpmock.ZeroCallCounters()
			err := flusher.Export(groupEventsArray, nil)
			So(err, ShouldBeNil)
			flusher.Stop()

			Convey("each GroupEvents should send in a single request", func() {
				So(httpmock.GetTotalCallCount(), ShouldEqual, 2)
			})
			Convey("request body should by valid", func() {
				So(actualRequests, ShouldResemble, []string{
					mockMetric1, mockMetric2,
				})
			})
		})

		Convey("Export multiple byte events in one GroupEvents with Metadata {db: mydb}, and ", func() {
			groupEvents := models.PipelineGroupEvents{
				Group: models.NewGroup(mockMetadata, nil),
				Events: []models.PipelineEvent{models.ByteArray(mockMetric1),
					models.ByteArray(mockMetric2)},
			}
			httpmock.ZeroCallCounters()
			err := flusher.Export([]*models.PipelineGroupEvents{&groupEvents}, nil)
			So(err, ShouldBeNil)
			flusher.Stop()

			Convey("events in the same groupEvents should be send in individual request, when Convert.Separator is not set", func() {
				reqCount := httpmock.GetTotalCallCount()
				So(reqCount, ShouldEqual, 2)
			})

			Convey("request body should be valid", func() {
				So(actualRequests, ShouldResemble, []string{
					mockMetric1, mockMetric2,
				})
			})
		})
	})

	Convey("Given a http flusher with Convert.Protocol: Raw, Convert.Encoding: Custom, Convert.Separator: '\n' ,Query: '%{metadata.db}'", t, func() {
		var actualRequests []string
		httpmock.Activate()
		defer httpmock.DeactivateAndReset()

		httpmock.RegisterResponder("POST", "http://test.com/write?db=mydb", func(req *http.Request) (*http.Response, error) {
			body, _ := io.ReadAll(req.Body)
			actualRequests = append(actualRequests, string(body))
			return httpmock.NewStringResponse(200, "ok"), nil
		})

		flusher := &FlusherHTTP{
			RemoteURL: "http://test.com/write",
			Convert: helper.ConvertConfig{
				Protocol:  converter.ProtocolRaw,
				Encoding:  converter.EncodingCustom,
				Separator: "\n",
			},
			Timeout:     defaultTimeout,
			Concurrency: 1,
			Query: map[string]string{
				"db": "%{metadata.db}",
			},
		}

		err := flusher.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		mockMetric1 := "cpu.load.short,host=server01,region=cn value=0.6 1672321328000000000"
		mockMetric2 := "cpu.load.short,host=server01,region=cn value=0.2 1672321358000000000"
		mockMetadata := models.NewMetadataWithKeyValues("db", "mydb")

		Convey("Export a single byte events each GroupEvents with Metadata {db: mydb}", func() {
			groupEventsArray := []*models.PipelineGroupEvents{
				{
					Group:  models.NewGroup(mockMetadata, nil),
					Events: []models.PipelineEvent{models.ByteArray(mockMetric1)},
				},
				{
					Group:  models.NewGroup(mockMetadata, nil),
					Events: []models.PipelineEvent{models.ByteArray(mockMetric2)},
				},
			}
			httpmock.ZeroCallCounters()
			err := flusher.Export(groupEventsArray, nil)
			So(err, ShouldBeNil)
			flusher.Stop()

			Convey("each GroupEvents should send in a single request", func() {
				So(httpmock.GetTotalCallCount(), ShouldEqual, 2)
			})
			Convey("request body should by valid", func() {
				So(actualRequests, ShouldResemble, []string{
					mockMetric1, mockMetric2,
				})
			})
		})

		Convey("Export multiple byte events in one GroupEvents with Metadata {db: mydb}, and ", func() {
			groupEvents := models.PipelineGroupEvents{
				Group: models.NewGroup(mockMetadata, nil),
				Events: []models.PipelineEvent{models.ByteArray(mockMetric1),
					models.ByteArray(mockMetric2)},
			}
			httpmock.ZeroCallCounters()
			err := flusher.Export([]*models.PipelineGroupEvents{&groupEvents}, nil)
			So(err, ShouldBeNil)
			flusher.Stop()

			Convey("events in the same groupEvents should be send in one request, when Convert.Separator is set", func() {
				reqCount := httpmock.GetTotalCallCount()
				So(reqCount, ShouldEqual, 1)
			})

			Convey("request body should be valid", func() {
				So(actualRequests, ShouldResemble, []string{
					mockMetric1 + "\n" + mockMetric2,
				})
			})
		})
	})

}

func TestHttpFlusherExportUnsupportedEventType(t *testing.T) {
	Convey("Given a http flusher with protocol: raw, encoding: custom", t, func() {
		flusher := &FlusherHTTP{
			RemoteURL: "http://test.com/write",
			Convert: helper.ConvertConfig{
				Protocol: converter.ProtocolRaw,
				Encoding: converter.EncodingCustom,
			},
			Timeout:     defaultTimeout,
			Concurrency: 1,
		}

		err := flusher.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("Export non byteArray type events, then", func() {
			groupEvents := models.PipelineGroupEvents{
				Events: []models.PipelineEvent{&models.Metric{
					Name:      "cpu.load.short",
					Timestamp: 1672321328000000000,
					Tags:      models.NewTagsWithKeyValues("host", "server01", "region", "cn"),
					Value:     &models.MetricSingleValue{Value: 0.64},
				}},
			}
			logger.ClearMemoryLog()
			err = flusher.Export([]*models.PipelineGroupEvents{&groupEvents}, nil)
			So(err, ShouldBeNil)
			flusher.Stop()
			Convey("logger should output Error Message when runFlushTask", func() {
				memoryLog, ok := logger.ReadMemoryLog(1)
				So(ok, ShouldBeTrue)
				So(memoryLog, ShouldContainSubstring, "unsupported event type 1")
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

func TestHttpFlusherFlushWithInterceptor(t *testing.T) {
	Convey("Given a http flusher with sync intercepter", t, func() {
		mockIntercepter := &mockInterceptor{}
		flusher := &FlusherHTTP{
			RemoteURL: "http://test.com/write",
			Convert: helper.ConvertConfig{
				Protocol: converter.ProtocolInfluxdb,
				Encoding: converter.EncodingCustom,
			},
			interceptor:    mockIntercepter,
			AsyncIntercept: false,
			Timeout:        defaultTimeout,
			Concurrency:    1,
			queue:          make(chan interface{}, 10),
		}

		Convey("should discard all events", func() {
			groupEvents := models.PipelineGroupEvents{
				Events: []models.PipelineEvent{&models.Metric{
					Name:      "cpu.load.short",
					Timestamp: 1672321328000000000,
					Tags:      models.NewTagsWithKeyValues("host", "server01", "region", "cn"),
					Value:     &models.MetricSingleValue{Value: 0.64},
				}},
			}
			err := flusher.Export([]*models.PipelineGroupEvents{&groupEvents}, nil)
			So(err, ShouldBeNil)
			So(flusher.queue, ShouldBeEmpty)
		})
	})

	Convey("Given a http flusher with async intercepter", t, func() {
		mockIntercepter := &mockInterceptor{}
		flusher := &FlusherHTTP{
			RemoteURL: "http://test.com/write",
			Convert: helper.ConvertConfig{
				Protocol: converter.ProtocolInfluxdb,
				Encoding: converter.EncodingCustom,
			},
			interceptor:    mockIntercepter,
			AsyncIntercept: true,
			Timeout:        defaultTimeout,
			Concurrency:    1,
			queue:          make(chan interface{}, 10),
		}

		Convey("should discard all events", func() {
			groupEvents := models.PipelineGroupEvents{
				Events: []models.PipelineEvent{&models.Metric{
					Name:      "cpu.load.short",
					Timestamp: 1672321328000000000,
					Tags:      models.NewTagsWithKeyValues("host", "server01", "region", "cn"),
					Value:     &models.MetricSingleValue{Value: 0.64},
				}},
			}
			err := flusher.Export([]*models.PipelineGroupEvents{&groupEvents}, nil)
			So(err, ShouldBeNil)
			So(len(flusher.queue), ShouldEqual, 1)
			err = flusher.convertAndFlush(<-flusher.queue)
			So(err, ShouldBeNil)
		})

	})
}

type mockContext struct {
	pipeline.Context
	basicAuth   *basicAuth
	interceptor *mockInterceptor
}

func (c mockContext) GetExtension(name string, cfg any) (pipeline.Extension, error) {
	if c.basicAuth != nil {
		return c.basicAuth, nil
	}

	if c.interceptor != nil {
		return c.interceptor, nil
	}
	return nil, fmt.Errorf("basicAuth not set")
}

func (c mockContext) GetConfigName() string {
	return "ctx"
}

func (c mockContext) GetRuntimeContext() context.Context {
	return context.Background()
}

func (c mockContext) GetPipelineScopeConfig() *config.GlobalConfig {
	return &config.GlobalConfig{}
}

func init() {
	logger.InitTestLogger(logger.OptionOpenMemoryReceiver)
}

type basicAuth struct {
	Username string
	Password string
}

func (b *basicAuth) Description() string {
	return "basic auth extension to add auth info to client request"
}

func (b *basicAuth) Init(context pipeline.Context) error {
	return nil
}

func (b *basicAuth) Stop() error {
	return nil
}

func (b *basicAuth) RoundTripper(base http.RoundTripper) (http.RoundTripper, error) {
	return &basicAuthRoundTripper{base: base, auth: b}, nil
}

type basicAuthRoundTripper struct {
	base http.RoundTripper
	auth *basicAuth
}

func (b *basicAuthRoundTripper) RoundTrip(request *http.Request) (*http.Response, error) {
	request.SetBasicAuth(b.auth.Username, b.auth.Password)
	return b.base.RoundTrip(request)
}

type mockInterceptor struct {
}

func (mi *mockInterceptor) Description() string {
	return "a filter that discard all events"
}

func (mi *mockInterceptor) Init(context pipeline.Context) error {
	return nil
}

func (mi *mockInterceptor) Stop() error {
	return nil
}

func (mi *mockInterceptor) Intercept(group *models.PipelineGroupEvents) *models.PipelineGroupEvents {
	if group == nil {
		return nil
	}
	group.Events = group.Events[:0]
	return group
}
