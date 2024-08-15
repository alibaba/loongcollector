// Copyright 2023 iLogtail Authors
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

package opentelemetry

import (
	"context"
	"net"
	"strconv"
	"testing"
	"time"

	"github.com/smartystreets/goconvey/convey"
	"go.opentelemetry.io/collector/pdata/plog/plogotlp"
	"go.opentelemetry.io/collector/pdata/pmetric/pmetricotlp"
	"go.opentelemetry.io/collector/pdata/ptrace/ptraceotlp"
	otlpv1 "go.opentelemetry.io/proto/otlp/collector/logs/v1"
	"google.golang.org/grpc"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

type TestOtlpLogService struct {
	otlpv1.UnimplementedLogsServiceServer
	ch    chan *otlpv1.ExportLogsServiceRequest
	pause time.Duration
}

func (t *TestOtlpLogService) Export(ctx context.Context, request *otlpv1.ExportLogsServiceRequest) (*otlpv1.ExportLogsServiceResponse, error) {
	t.ch <- request
	if t.pause > 0 {
		time.Sleep(t.pause)
	}
	return &otlpv1.ExportLogsServiceResponse{}, nil
}

func Test_Flusher_Init(t *testing.T) {
	convey.Convey("When init grpc service", t, func() {
		_, server := newTestGrpcService(t, ":8080", time.Nanosecond)
		defer func() {
			server.Stop()
		}()
		logCtx := mock.NewEmptyContext("p", "l", "c")

		convey.Convey("When FlusherOTLP init", func() {
			f := &FlusherOTLP{Version: v1, Logs: &helper.GrpcClientConfig{Endpoint: ":8080"}}
			err := f.Init(logCtx)
			convey.So(err, convey.ShouldBeNil)
		})
	})
}

func Test_Flusher_Init_Invalid(t *testing.T) {
	convey.Convey("When init grpc service", t, func() {
		_, server := newTestGrpcService(t, ":8080", time.Nanosecond)
		defer func() {
			server.Stop()
		}()
		logCtx := mock.NewEmptyContext("p", "l", "c")

		convey.Convey("When FlusherOTLP init", func() {
			f := &FlusherOTLP{Version: v1}
			err := f.Init(logCtx)
			convey.So(err, convey.ShouldNotBeNil)
		})
	})
}

func Test_Flusher_Flush(t *testing.T) {
	convey.Convey("When init grpc service", t, func() {
		addr := test.GetAvailableLocalAddress(t)
		service, server := newTestGrpcService(t, addr, time.Nanosecond*0)
		defer func() {
			server.Stop()
		}()
		logCtx := mock.NewEmptyContext("p", "l", "c")

		convey.Convey("When FlusherOTLP init", func() {
			f := &FlusherOTLP{Version: v1, Logs: &helper.GrpcClientConfig{Endpoint: addr, WaitForReady: true}}
			err := f.Init(logCtx)
			convey.So(err, convey.ShouldBeNil)

			convey.Convey("When FlusherOTLP flush", func() {
				groupList := makeTestLogGroupList().GetLogGroupList()
				err := f.Flush("p", "l", "c", groupList)
				convey.So(err, convey.ShouldBeNil)
				r := <-service.ch
				r2 := f.convertLogGroupToRequest(groupList)
				convey.So(len(r.ResourceLogs), convey.ShouldEqual, r2.Logs().ResourceLogs().Len())
				for i := range r.ResourceLogs {
					for j := range r.ResourceLogs[i].ScopeLogs {
						for m := range r.ResourceLogs[i].ScopeLogs[j].LogRecords {
							expected := r.ResourceLogs[i].ScopeLogs[j].LogRecords[m]
							actual := r2.Logs().ResourceLogs().At(i).ScopeLogs().At(j).LogRecords().At(m)
							convey.So(expected.Body.GetStringValue(), convey.ShouldEqual, actual.Body().AsString())

							for _, v := range expected.Attributes {
								value, ok := actual.Attributes().Get(v.Key)
								convey.So(ok, convey.ShouldBeTrue)
								convey.So(v.Value.GetStringValue(), convey.ShouldEqual, value.AsString())
							}

						}
					}
				}

			})
		})
	})
}

func Test_Flusher_Export_Logs_Empty(t *testing.T) {
	convey.Convey("When init grpc service", t, func() {
		addr := test.GetAvailableLocalAddress(t)
		service, _, _, server := newTestGrpcMetricServiceV2(t, addr, time.Nanosecond*1)
		defer func() {
			server.Stop()
		}()
		logCtx := mock.NewEmptyContext("p", "l", "c")

		convey.Convey("When FlusherOTLP init", func() {
			f := &FlusherOTLP{Version: v1, Logs: &helper.GrpcClientConfig{Endpoint: addr, WaitForReady: true}}
			err := f.Init(logCtx)
			convey.So(err, convey.ShouldBeNil)
			convey.So(0, convey.ShouldEqual, len(service.ch))
		})
	})
}

func Test_Flusher_Export_Logs(t *testing.T) {
	convey.Convey("When init grpc service", t, func() {
		addr := test.GetAvailableLocalAddress(t)
		service, _, _, server := newTestGrpcMetricServiceV2(t, addr, time.Nanosecond*0)
		defer func() {
			server.Stop()
		}()
		logCtx := mock.NewEmptyContext("p", "l", "c")

		convey.Convey("When FlusherOTLP init", func() {
			f := &FlusherOTLP{Version: v1, Logs: &helper.GrpcClientConfig{Endpoint: addr, WaitForReady: true}}
			err := f.Init(logCtx)
			convey.So(err, convey.ShouldBeNil)

			convey.Convey("When FlusherOTLP flush", func() {
				PipelineGroupEventsSlice := makeTestPipelineGroupEventsLogSlice()
				err := f.Export(PipelineGroupEventsSlice, helper.NewNoopPipelineConext())
				convey.So(err, convey.ShouldBeNil)
				r := <-service.ch
				r2, _, _ := f.convertPipelinesGroupeEventsToRequest(PipelineGroupEventsSlice)
				convey.So(r.Logs().ResourceLogs().Len(), convey.ShouldEqual, r2.Logs().ResourceLogs().Len())

				for i := 0; i < r.Logs().ResourceLogs().Len(); i++ {
					resourceSpan := r.Logs().ResourceLogs().At(i)
					for j := 0; j < resourceSpan.ScopeLogs().Len(); j++ {
						scopemetric := resourceSpan.ScopeLogs().At(j)

						for m := 0; m < scopemetric.LogRecords().Len(); m++ {
							span := scopemetric.LogRecords().At(m)
							expected := span
							actual := r2.Logs().ResourceLogs().At(i).ScopeLogs().At(j).LogRecords().At(m)
							convey.So(3, convey.ShouldEqual, actual.Attributes().Len())
							convey.So(expected.Attributes().Len(), convey.ShouldEqual, actual.Attributes().Len())
							convey.So(expected.Timestamp(), convey.ShouldEqual, actual.Timestamp())
							convey.So(expected.Body().Bytes().AsRaw(), convey.ShouldResemble, actual.Body().Bytes().AsRaw())
						}
					}
				}

			})

		})
	})
}

func Test_Flusher_Export_Metrics(t *testing.T) {
	convey.Convey("When init grpc service", t, func() {
		addr := test.GetAvailableLocalAddress(t)
		_, service, _, server := newTestGrpcMetricServiceV2(t, addr, time.Nanosecond*0)
		defer func() {
			server.Stop()
		}()
		logCtx := mock.NewEmptyContext("p", "l", "c")

		convey.Convey("When FlusherOTLP init", func() {
			f := &FlusherOTLP{Version: v1, Metrics: &helper.GrpcClientConfig{Endpoint: addr, WaitForReady: true}}
			err := f.Init(logCtx)
			convey.So(err, convey.ShouldBeNil)

			convey.Convey("When FlusherOTLP flush", func() {
				PipelineGroupEventsSlice := makeTestPipelineGroupEventsMetricSlice()
				err := f.Export(PipelineGroupEventsSlice, helper.NewNoopPipelineConext())
				convey.So(err, convey.ShouldBeNil)

				r := <-service.ch
				_, r2, _ := f.convertPipelinesGroupeEventsToRequest(PipelineGroupEventsSlice)
				convey.So(r.Metrics().ResourceMetrics().Len(), convey.ShouldEqual, r2.Metrics().ResourceMetrics().Len())
				for i := 0; i < r.Metrics().ResourceMetrics().Len(); i++ {
					resourceMetric := r.Metrics().ResourceMetrics().At(i)
					for j := 0; j < resourceMetric.ScopeMetrics().Len(); j++ {
						scopemetric := resourceMetric.ScopeMetrics().At(j)

						for m := 0; m < scopemetric.Metrics().Len(); m++ {
							metric := scopemetric.Metrics().At(m)

							expected := metric
							actual := r2.Metrics().ResourceMetrics().At(i).ScopeMetrics().At(j).Metrics().At(m)
							convey.So(expected.Description(), convey.ShouldEqual, actual.Description())
							convey.So(expected.Sum().DataPoints().Len(), convey.ShouldEqual, actual.Sum().DataPoints().Len())
						}
					}
				}

			})
		})
	})
}

func Test_Flusher_Export_Traces(t *testing.T) {
	convey.Convey("When init grpc service", t, func() {
		addr := test.GetAvailableLocalAddress(t)
		_, _, service, server := newTestGrpcMetricServiceV2(t, addr, time.Nanosecond*0)
		defer func() {
			server.Stop()
		}()
		logCtx := mock.NewEmptyContext("p", "l", "c")

		convey.Convey("When FlusherOTLP init", func() {
			f := &FlusherOTLP{Version: v1, Traces: &helper.GrpcClientConfig{Endpoint: addr, WaitForReady: true}}
			err := f.Init(logCtx)
			convey.So(err, convey.ShouldBeNil)

			convey.Convey("When FlusherOTLP flush", func() {
				PipelineGroupEventsSlice := makeTestPipelineGroupEventsTraceSlice()
				err := f.Export(PipelineGroupEventsSlice, helper.NewNoopPipelineConext())
				convey.So(err, convey.ShouldBeNil)

				r := <-service.ch
				_, _, r2 := f.convertPipelinesGroupeEventsToRequest(PipelineGroupEventsSlice)
				convey.So(r.Traces().ResourceSpans().Len(), convey.ShouldEqual, r2.Traces().ResourceSpans().Len())

				for i := 0; i < r.Traces().ResourceSpans().Len(); i++ {
					resourceSpan := r.Traces().ResourceSpans().At(i)
					for j := 0; j < resourceSpan.ScopeSpans().Len(); j++ {
						scopemetric := resourceSpan.ScopeSpans().At(j)

						for m := 0; m < scopemetric.Spans().Len(); m++ {
							span := scopemetric.Spans().At(m)
							expected := span
							actual := r2.Traces().ResourceSpans().At(i).ScopeSpans().At(j).Spans().At(m)
							convey.So(3, convey.ShouldEqual, actual.Attributes().Len())
							convey.So(expected.Attributes().Len(), convey.ShouldEqual, actual.Attributes().Len())
							convey.So(expected.StartTimestamp(), convey.ShouldEqual, actual.StartTimestamp())
						}
					}
				}

			})

		})
	})
}

func Test_Flusher_Export_All(t *testing.T) {
	convey.Convey("When init grpc service", t, func() {
		defaultAddr := test.GetAvailableLocalAddress(t)
		_, _, traceService, traceServer := newTestGrpcMetricServiceV2(t, defaultAddr, time.Nanosecond*0)
		defer func() {
			traceServer.Stop()
		}()

		metricAddr := test.GetAvailableLocalAddress(t)
		_, metricService, _, metricServer := newTestGrpcMetricServiceV2(t, metricAddr, time.Nanosecond*0)
		defer func() {
			metricServer.Stop()
		}()

		logCtx := mock.NewEmptyContext("p", "l", "c")

		convey.Convey("When FlusherOTLP init", func() {
			f := &FlusherOTLP{
				Version: v1,
				Logs:    &helper.GrpcClientConfig{Endpoint: defaultAddr, WaitForReady: true},
				Metrics: &helper.GrpcClientConfig{Endpoint: metricAddr, WaitForReady: true},
				Traces:  &helper.GrpcClientConfig{Endpoint: defaultAddr, WaitForReady: true},
			}
			err := f.Init(logCtx)
			convey.So(err, convey.ShouldBeNil)

			convey.Convey("When FlusherOTLP flush traces", func() {
				PipelineGroupEventsSlice := makeTestPipelineGroupEventsTraceSlice()
				err := f.Export(PipelineGroupEventsSlice, helper.NewNoopPipelineConext())
				convey.So(err, convey.ShouldBeNil)

				r := <-traceService.ch
				_, _, r2 := f.convertPipelinesGroupeEventsToRequest(PipelineGroupEventsSlice)
				convey.So(r.Traces().ResourceSpans().Len(), convey.ShouldEqual, r2.Traces().ResourceSpans().Len())

				for i := 0; i < r.Traces().ResourceSpans().Len(); i++ {
					resourceSpan := r.Traces().ResourceSpans().At(i)
					for j := 0; j < resourceSpan.ScopeSpans().Len(); j++ {
						scopemetric := resourceSpan.ScopeSpans().At(j)

						for m := 0; m < scopemetric.Spans().Len(); m++ {
							span := scopemetric.Spans().At(m)
							expected := span
							actual := r2.Traces().ResourceSpans().At(i).ScopeSpans().At(j).Spans().At(m)
							convey.So(3, convey.ShouldEqual, actual.Attributes().Len())
							convey.So(expected.Attributes().Len(), convey.ShouldEqual, actual.Attributes().Len())
							convey.So(expected.StartTimestamp(), convey.ShouldEqual, actual.StartTimestamp())
						}
					}
				}
			})

			convey.Convey("When FlusherOTLP flush metrics", func() {
				PipelineGroupEventsSlice := makeTestPipelineGroupEventsMetricSlice()
				err := f.Export(PipelineGroupEventsSlice, helper.NewNoopPipelineConext())
				convey.So(err, convey.ShouldBeNil)

				r := <-metricService.ch
				_, r2, _ := f.convertPipelinesGroupeEventsToRequest(PipelineGroupEventsSlice)
				convey.So(r.Metrics().ResourceMetrics().Len(), convey.ShouldEqual, r2.Metrics().ResourceMetrics().Len())
				for i := 0; i < r.Metrics().ResourceMetrics().Len(); i++ {
					resourceMetric := r.Metrics().ResourceMetrics().At(i)
					for j := 0; j < resourceMetric.ScopeMetrics().Len(); j++ {
						scopemetric := resourceMetric.ScopeMetrics().At(j)

						for m := 0; m < scopemetric.Metrics().Len(); m++ {
							metric := scopemetric.Metrics().At(m)

							expected := metric
							actual := r2.Metrics().ResourceMetrics().At(i).ScopeMetrics().At(j).Metrics().At(m)
							convey.So(expected.Description(), convey.ShouldEqual, actual.Description())
							convey.So(expected.Sum().DataPoints().Len(), convey.ShouldEqual, actual.Sum().DataPoints().Len())
						}
					}
				}

			})
		})

	})
}

func Test_Flusher_Export_All_Disable_Trace(t *testing.T) {
	convey.Convey("When init grpc service", t, func() {
		defaultAddr := test.GetAvailableLocalAddress(t)
		_, _, traceService, traceServer := newTestGrpcMetricServiceV2(t, defaultAddr, time.Nanosecond*0)
		defer func() {
			traceServer.Stop()
		}()

		metricAddr := test.GetAvailableLocalAddress(t)
		_, metricService, _, metricServer := newTestGrpcMetricServiceV2(t, metricAddr, time.Nanosecond*0)
		defer func() {
			metricServer.Stop()
		}()

		logCtx := mock.NewEmptyContext("p", "l", "c")

		convey.Convey("When FlusherOTLP init", func() {
			f := &FlusherOTLP{
				Version: v1,
				Logs:    &helper.GrpcClientConfig{Endpoint: defaultAddr, WaitForReady: true},
				Metrics: &helper.GrpcClientConfig{
					Endpoint: metricAddr, WaitForReady: true},
			}

			err := f.Init(logCtx)
			convey.So(err, convey.ShouldBeNil)

			convey.Convey("When FlusherOTLP flush traces", func() {
				PipelineGroupEventsSlice := makeTestPipelineGroupEventsTraceSlice()
				err := f.Export(PipelineGroupEventsSlice, helper.NewNoopPipelineConext())
				convey.So(err, convey.ShouldBeNil)

				time.Sleep(1 * time.Second)
				convey.So(len(traceService.ch), convey.ShouldEqual, 0)
			})

			convey.Convey("When FlusherOTLP flush metrics", func() {
				PipelineGroupEventsSlice := makeTestPipelineGroupEventsMetricSlice()
				err := f.Export(PipelineGroupEventsSlice, helper.NewNoopPipelineConext())
				convey.So(err, convey.ShouldBeNil)

				r := <-metricService.ch
				_, r2, _ := f.convertPipelinesGroupeEventsToRequest(PipelineGroupEventsSlice)
				convey.So(r.Metrics().ResourceMetrics().Len(), convey.ShouldEqual, r2.Metrics().ResourceMetrics().Len())
				for i := 0; i < r.Metrics().ResourceMetrics().Len(); i++ {
					resourceMetric := r.Metrics().ResourceMetrics().At(i)
					for j := 0; j < resourceMetric.ScopeMetrics().Len(); j++ {
						scopemetric := resourceMetric.ScopeMetrics().At(j)

						for m := 0; m < scopemetric.Metrics().Len(); m++ {
							metric := scopemetric.Metrics().At(m)

							expected := metric
							actual := r2.Metrics().ResourceMetrics().At(i).ScopeMetrics().At(j).Metrics().At(m)
							convey.So(expected.Description(), convey.ShouldEqual, actual.Description())
							convey.So(expected.Sum().DataPoints().Len(), convey.ShouldEqual, actual.Sum().DataPoints().Len())
						}
					}
				}

			})
		})

	})
}

func Test_Flusher_Flush_Timeout(t *testing.T) {
	convey.Convey("When init grpc service", t, func() {
		Addr := test.GetAvailableLocalAddress(t)
		_, server := newTestGrpcService(t, Addr, time.Second*2)
		defer func() {
			server.Stop()
		}()
		logCtx := mock.NewEmptyContext("p", "l", "c")

		convey.Convey("When FlusherOTLP init", func() {
			f := &FlusherOTLP{Version: v1, Logs: &helper.GrpcClientConfig{Endpoint: Addr, WaitForReady: true, Timeout: 1000}}
			err := f.Init(logCtx)
			convey.So(err, convey.ShouldBeNil)

			convey.Convey("When FlusherOTLP flush timeout", func() {
				groupList := makeTestLogGroupList().GetLogGroupList()
				err := f.Flush("p", "l", "c", groupList)
				convey.So(err, convey.ShouldBeError)
			})
		})
	})
}

func Test_Flusher_GetConverter(t *testing.T) {
	convey.Convey("When get Converter", t, func() {

		logCtx := mock.NewEmptyContext("p", "l", "c")
		convey.Convey("When get Converter", func() {
			f := &FlusherOTLP{Version: v1, context: logCtx}

			c, err := f.getConverter()
			convey.So(c, convey.ShouldNotBeNil)
			convey.So(err, convey.ShouldBeNil)
		})

		convey.Convey("When get Converter fail", func() {
			f := &FlusherOTLP{context: logCtx}

			_, err := f.getConverter()
			convey.So(err, convey.ShouldBeError)
		})
	})
}

func newTestGrpcService(t *testing.T, address string, pause time.Duration) (*TestOtlpLogService, *grpc.Server) {
	server := grpc.NewServer()
	listener, err := net.Listen("tcp", address)
	convey.So(err, convey.ShouldBeNil)
	ch := make(chan *otlpv1.ExportLogsServiceRequest, 1000)
	service := &TestOtlpLogService{ch: ch, pause: pause}
	otlpv1.RegisterLogsServiceServer(server, service)
	go func(t *testing.T) {
		convey.Convey("When gRPC service start", t, func() {
			err = server.Serve(listener)
			convey.So(err, convey.ShouldBeNil)
		})
	}(t)
	time.Sleep(time.Second)
	return service, server
}

func makeTestLogGroupList() *protocol.LogGroupList {
	f := map[string]string{}
	lgl := &protocol.LogGroupList{
		LogGroupList: make([]*protocol.LogGroup, 0, 10),
	}
	for i := 1; i <= 10; i++ {
		lg := &protocol.LogGroup{
			Logs: make([]*protocol.Log, 0, 10),
		}
		for j := 1; j <= 10; j++ {
			f["group"] = strconv.Itoa(i)
			f["content"] = "The message: " + strconv.Itoa(j)
			l := test.CreateLogByFields(f)
			lg.Logs = append(lg.Logs, l)
		}
		lgl.LogGroupList = append(lgl.LogGroupList, lg)
	}
	return lgl
}

type TestOtlpLogServiceV2 struct {
	ch    chan plogotlp.ExportRequest
	pause time.Duration
}

func (t *TestOtlpLogServiceV2) Export(ctx context.Context, request plogotlp.ExportRequest) (plogotlp.ExportResponse, error) {
	t.ch <- request
	if t.pause > 0 {
		time.Sleep(t.pause)
	}
	return plogotlp.NewExportResponse(), nil
}

type TestOtlpMetricServiceV2 struct {
	ch    chan pmetricotlp.ExportRequest
	pause time.Duration
}

func (t *TestOtlpMetricServiceV2) Export(ctx context.Context, request pmetricotlp.ExportRequest) (pmetricotlp.ExportResponse, error) {
	t.ch <- request
	if t.pause > 0 {
		time.Sleep(t.pause)
	}
	return pmetricotlp.NewExportResponse(), nil
}

func newTestGrpcMetricServiceV2(t *testing.T, address string, pause time.Duration) (
	*TestOtlpLogServiceV2, *TestOtlpMetricServiceV2, *TestOtlpTraceServiceV2, *grpc.Server) {
	server := grpc.NewServer()
	listener, err := net.Listen("tcp", address)
	convey.So(err, convey.ShouldBeNil)

	logCh := make(chan plogotlp.ExportRequest, 1000)
	metricCh := make(chan pmetricotlp.ExportRequest, 1000)
	traceCh := make(chan ptraceotlp.ExportRequest, 1000)

	metricService := &TestOtlpMetricServiceV2{ch: metricCh, pause: pause}
	logService := &TestOtlpLogServiceV2{ch: logCh, pause: pause}
	traceService := &TestOtlpTraceServiceV2{ch: traceCh, pause: pause}

	pmetricotlp.RegisterGRPCServer(server, metricService)
	plogotlp.RegisterGRPCServer(server, logService)
	ptraceotlp.RegisterGRPCServer(server, traceService)

	go func(t *testing.T) {
		convey.Convey("When gRPC service start", t, func() {
			err = server.Serve(listener)
			convey.So(err, convey.ShouldBeNil)
		})
	}(t)
	time.Sleep(time.Second)
	return logService, metricService, traceService, server
}

type TestOtlpTraceServiceV2 struct {
	ch    chan ptraceotlp.ExportRequest
	pause time.Duration
}

func (t *TestOtlpTraceServiceV2) Export(ctx context.Context, request ptraceotlp.ExportRequest) (ptraceotlp.ExportResponse, error) {
	t.ch <- request
	if t.pause > 0 {
		time.Sleep(t.pause)
	}
	return ptraceotlp.NewExportResponse(), nil
}

func makeTestPipelineGroupEventsMetricSlice() []*models.PipelineGroupEvents {
	slice := make([]*models.PipelineGroupEvents, 0, 10)

	for i := 1; i <= 10; i++ {
		pipelineGroupEvent := &models.PipelineGroupEvents{
			Group: &models.GroupInfo{
				Metadata: models.NewMetadata(),
				Tags:     models.NewTags(),
			},
		}

		for j := 0; j < 5; j++ {
			pipelineGroupEvent.Group.Metadata.Add("meta_key_"+strconv.Itoa(j), "meta_value_"+strconv.Itoa(j))
		}

		for j := 0; j < 5; j++ {
			pipelineGroupEvent.Group.Tags.Add("common_tags_"+strconv.Itoa(j), "common_value_"+strconv.Itoa(j))
		}

		for j := 0; j < 10; j++ {
			tags := models.NewTagsWithMap(
				map[string]string{
					"__tag__:__path__": "/root/test/origin/example.log",
					"__log_topic__":    "file",
					"content":          "test log content" + strconv.Itoa(j),
				},
			)
			event := models.NewSingleValueMetric(
				"metric_name_"+strconv.Itoa(i),
				models.MetricTypeCounter,
				tags,
				time.Now().Unix(),
				i+1,
			)
			pipelineGroupEvent.Events = append(pipelineGroupEvent.Events, event)
		}

		slice = append(slice, pipelineGroupEvent)

	}
	return slice
}

func makeTestPipelineGroupEventsTraceSlice() []*models.PipelineGroupEvents {
	slice := make([]*models.PipelineGroupEvents, 0, 10)

	for i := 1; i <= 10; i++ {
		pipelineGroupEvent := &models.PipelineGroupEvents{
			Group: &models.GroupInfo{
				Metadata: models.NewMetadata(),
				Tags:     models.NewTags(),
			},
		}

		for j := 0; j < 5; j++ {
			pipelineGroupEvent.Group.Metadata.Add("meta_key_"+strconv.Itoa(j), "meta_value_"+strconv.Itoa(j))
		}

		for j := 0; j < 5; j++ {
			pipelineGroupEvent.Group.Tags.Add("common_tags_"+strconv.Itoa(j), "common_value_"+strconv.Itoa(j))
		}

		for j := 0; j < 10; j++ {
			tags := models.NewTagsWithMap(
				map[string]string{
					"__tag__:__path__": "/root/test/origin/example.log",
					"__log_topic__":    "file",
					"content":          "test log content" + strconv.Itoa(j),
				},
			)

			event := models.NewSpan(
				"span_name_"+strconv.Itoa(i),
				"",
				"",
				models.SpanKindClient,
				uint64(time.Now().Add(-time.Second).UnixNano()),
				uint64(time.Now().UnixNano()),
				tags,
				nil,
				nil,
			)
			pipelineGroupEvent.Events = append(pipelineGroupEvent.Events, event)
		}

		slice = append(slice, pipelineGroupEvent)

	}
	return slice
}

func makeTestPipelineGroupEventsLogSlice() []*models.PipelineGroupEvents {
	slice := make([]*models.PipelineGroupEvents, 0, 10)

	for i := 1; i <= 10; i++ {
		pipelineGroupEvent := &models.PipelineGroupEvents{
			Group: &models.GroupInfo{
				Metadata: models.NewMetadata(),
				Tags:     models.NewTags(),
			},
		}

		for j := 0; j < 5; j++ {
			pipelineGroupEvent.Group.Metadata.Add("meta_key_"+strconv.Itoa(j), "meta_value_"+strconv.Itoa(j))
		}

		for j := 0; j < 5; j++ {
			pipelineGroupEvent.Group.Tags.Add("common_tags_"+strconv.Itoa(j), "common_value_"+strconv.Itoa(j))
		}

		for j := 0; j < 10; j++ {
			tags := models.NewTagsWithMap(
				map[string]string{
					"__tag__:__path__": "/root/test/origin/example.log",
					"__log_topic__":    "file",
					"content":          "test log content" + strconv.Itoa(j),
				},
			)

			event := models.NewLog(
				"log_name_"+strconv.Itoa(i),
				[]byte("message"),
				"INFO",
				"",
				"",
				tags,
				uint64(time.Now().UnixNano()),
			)
			event.ObservedTimestamp = uint64(time.Now().Add(-time.Second).UnixNano())
			pipelineGroupEvent.Events = append(pipelineGroupEvent.Events, event)
		}

		slice = append(slice, pipelineGroupEvent)

	}
	return slice
}
