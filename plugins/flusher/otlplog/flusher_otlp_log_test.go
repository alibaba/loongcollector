package otlplog

import (
	"context"
	"net"
	"strconv"
	"testing"
	"time"

	"github.com/smartystreets/goconvey/convey"
	otlpv1 "go.opentelemetry.io/proto/otlp/collector/logs/v1"
	"google.golang.org/grpc"

	"github.com/alibaba/ilogtail/pkg/protocol"

	"github.com/alibaba/ilogtail/helper"
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

		convey.Convey("When FlusherOTLPLog init", func() {
			f := &FlusherOTLPLog{Version: v1, GrpcConfig: &helper.GrpcClientConfig{}}
			err := f.Init(logCtx)
			convey.So(err, convey.ShouldBeNil)
		})
	})
}

func Test_Flusher_Flush(t *testing.T) {
	convey.Convey("When init grpc service", t, func() {
		service, server := newTestGrpcService(t, ":8176", time.Nanosecond*0)
		defer func() {
			server.Stop()
		}()
		logCtx := mock.NewEmptyContext("p", "l", "c")

		convey.Convey("When FlusherOTLPLog init", func() {
			f := &FlusherOTLPLog{Version: v1, GrpcConfig: &helper.GrpcClientConfig{Endpoint: ":8176", WaitForReady: true}}
			err := f.Init(logCtx)
			convey.So(err, convey.ShouldBeNil)

			convey.Convey("When FlusherOTLPLog flush", func() {
				groupList := makeTestLogGroupList().GetLogGroupList()
				err := f.Flush("p", "l", "c", groupList)
				convey.So(err, convey.ShouldBeNil)
				r := <-service.ch
				r2 := f.convertLogGroupToRequest(groupList)
				convey.So(len(r.ResourceLogs), convey.ShouldEqual, len(r2.ResourceLogs))
				for i := range r.ResourceLogs {
					convey.So(len(r.ResourceLogs[i].ScopeLogs), convey.ShouldEqual, len(r2.ResourceLogs[i].ScopeLogs))
				}
			})
		})
	})
}

func Test_Flusher_Flush_Timeout(t *testing.T) {
	convey.Convey("When init grpc service", t, func() {
		_, server := newTestGrpcService(t, ":8176", time.Second*2)
		defer func() {
			server.Stop()
		}()
		logCtx := mock.NewEmptyContext("p", "l", "c")

		convey.Convey("When FlusherOTLPLog init", func() {
			f := &FlusherOTLPLog{Version: v1, GrpcConfig: &helper.GrpcClientConfig{Endpoint: ":8176", WaitForReady: true, Timeout: 1000}}
			err := f.Init(logCtx)
			convey.So(err, convey.ShouldBeNil)

			convey.Convey("When FlusherOTLPLog flush timeout", func() {
				groupList := makeTestLogGroupList().GetLogGroupList()
				err := f.Flush("p", "l", "c", groupList)
				convey.So(err, convey.ShouldBeError)
			})
		})
	})
}

func Test_Flusher_GetConverter(t *testing.T) {
	convey.Convey("When get Converter", t, func() {

		convey.Convey("When get Converter", func() {
			f := &FlusherOTLPLog{Version: v1}
			c, err := f.getConverter()
			convey.So(c, convey.ShouldNotBeNil)
			convey.So(err, convey.ShouldBeNil)
		})

		convey.Convey("When get Converter fail", func() {
			f := &FlusherOTLPLog{}
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
