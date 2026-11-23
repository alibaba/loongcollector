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

package grpc

import (
	"context"
	"io"
	"net"
	"testing"

	"github.com/stretchr/testify/require"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/status"
	"google.golang.org/grpc/test/bufconn"

	"github.com/alibaba/ilogtail/pkg/helper"
	_ "github.com/alibaba/ilogtail/pkg/logger/test"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type reportReceiver struct {
	protocol.UnimplementedPipelineEventGroupReportServiceServer
	reports chan *protocol.PipelineEventGroupReport
	ack     error
}

func (receiver *reportReceiver) Collect(stream protocol.PipelineEventGroupReportService_CollectServer) error {
	for {
		report, err := stream.Recv()
		switch err {
		case io.EOF:
			if receiver.ack != nil {
				return receiver.ack
			}
			return stream.SendAndClose(&protocol.PipelineEventGroupReportResponse{})
		case nil:
			receiver.reports <- report
		default:
			return err
		}
	}
}

type legacyReceiver struct {
	protocol.UnimplementedLogReportServiceServer
	calls chan struct{}
}

func (receiver *legacyReceiver) Collect(stream protocol.LogReportService_CollectServer) error {
	receiver.calls <- struct{}{}
	return stream.SendAndClose(&protocol.Response{Code: protocol.ResponseCode_Success})
}

func testFlusher(t *testing.T, register func(*grpc.Server)) *Flusher {
	t.Helper()
	listener := bufconn.Listen(1024 * 1024)
	server := grpc.NewServer(grpc.ForceServerCodec(protocol.Codec{}))
	register(server)
	go func() { _ = server.Serve(listener) }()
	t.Cleanup(server.Stop)
	conn, err := grpc.Dial("passthrough:///bufnet",
		grpc.WithTransportCredentials(insecure.NewCredentials()),
		grpc.WithDefaultCallOptions(grpc.ForceCodec(protocol.Codec{})),
		grpc.WithContextDialer(func(ctx context.Context, _ string) (net.Conn, error) {
			return listener.DialContext(ctx)
		}),
	)
	require.NoError(t, err)
	flusher := &Flusher{conn: conn, client: protocol.NewPipelineEventGroupReportServiceClient(conn)}
	t.Cleanup(func() { require.NoError(t, flusher.Stop()) })
	return flusher
}

func TestFlusherExportPreservesEventGroups(t *testing.T) {
	receiver := &reportReceiver{reports: make(chan *protocol.PipelineEventGroupReport, 8)}
	flusher := testFlusher(t, func(server *grpc.Server) {
		protocol.RegisterPipelineEventGroupReportServiceServer(server, receiver)
	})
	const timestamp = 1700000000123456789
	log := models.NewLog("access", nil, "info", "span-id", "trace-id", models.NewTagsWithKeyValues("shared", "event"), timestamp)
	log.SetIndices(models.NewOrderedLogContents(3))
	log.Contents.Add("z", []byte{0xff, 0x00})
	log.Contents.Add("y", true)
	log.Contents.Add("content", "first")
	log.SetObservedTimestamp(timestamp + 2)
	log.SetOffset(21)
	log.SetRawSize(68)
	second := models.NewSimpleLog([]byte("second"), models.NewTagsWithKeyValues("container", "other"), timestamp+1)
	metric := models.NewSingleValueMetric("requests", models.MetricTypeGauge, models.NewTagsWithKeyValues("zone", "A"), timestamp+2, 42.5)
	metric.Unit = "requests"
	metric.Description = "Observed requests"
	metric.TypedValue = models.NewMetricTypedValues()
	metric.TypedValue.Add("source", &models.TypedValue{Type: models.ValueTypeString, Value: "probe"})
	metric.TypedValue.Add("ready", &models.TypedValue{Type: models.ValueTypeBoolean, Value: true})
	typedOnly := &models.Metric{Name: "typed_only", MetricType: models.MetricTypeUntyped, Timestamp: timestamp + 3, TypedValue: models.NewMetricTypedValues()}
	typedOnly.TypedValue.Add("version", &models.TypedValue{Type: models.ValueTypeString, Value: "v1"})
	span := models.NewSpan("request", "trace", "span", models.SpanKindServer, timestamp+4, timestamp+6, nil,
		[]*models.SpanEvent{{Timestamp: timestamp + 5, Name: "mark"}}, []*models.SpanLink{{TraceID: "linked"}})
	span.SetObservedTimestamp(timestamp + 7)
	last := models.NewSimpleLog([]byte("last"), nil, timestamp+8)
	group := &models.PipelineGroupEvents{
		Group: &models.GroupInfo{
			Tags:                models.NewTagsWithKeyValues("shared", "group", "__topic__", "access"),
			Metadata:            models.NewMetadataWithKeyValues("source_id", "probe"),
			DestinationLogstore: "routed",
		},
		Events: []models.PipelineEvent{log, second, metric, typedOnly, span, models.ByteArray{0x01, 0x00}, models.ByteArray{}, last},
	}
	require.NoError(t, flusher.Export([]*models.PipelineGroupEvents{nil, {}, group}, nil))
	require.Len(t, receiver.reports, 5)
	var counts []int
	var received []models.PipelineEvent
	for len(receiver.reports) > 0 {
		report := <-receiver.reports
		decoded, err := helper.DecodePipelineEventReport(report)
		require.NoError(t, err)
		require.Equal(t, "routed", decoded.Group.GetDestinationLogstore())
		require.Equal(t, group.Group.GetTags().Iterator(), decoded.Group.GetTags().Iterator())
		require.Equal(t, group.Group.GetMetadata().Iterator(), decoded.Group.GetMetadata().Iterator())
		received = append(received, decoded.Events...)
		counts = append(counts, len(decoded.Events))
		if len(counts) == 1 {
			require.Equal(t, []uint32{0}, report.LogAttributes[0].ByteFieldIndices)
			fields := report.EventGroup.GetLogs().Events[0].Contents
			require.Equal(t, []string{"z", "y", "content"}, []string{string(fields[0].Key), string(fields[1].Key), string(fields[2].Key)})
		}
	}
	require.Equal(t, []int{2, 2, 1, 2, 1}, counts)

	gotLog, ok := received[0].(*models.Log)
	require.True(t, ok)
	require.Equal(t, log.GetTags().Iterator(), gotLog.GetTags().Iterator())
	require.Equal(t, log.Name, gotLog.Name)
	require.Equal(t, log.Level, gotLog.Level)
	require.Equal(t, log.TraceID, gotLog.TraceID)
	require.Equal(t, log.SpanID, gotLog.SpanID)
	require.Equal(t, log.Offset, gotLog.Offset)
	require.Equal(t, log.RawSize, gotLog.RawSize)
	require.Equal(t, uint64(timestamp), gotLog.Timestamp)
	require.Equal(t, log.ObservedTimestamp, gotLog.ObservedTimestamp)
	require.Equal(t, []byte{0xff, 0x00}, gotLog.GetIndices().Get("z"))
	require.Equal(t, "true", gotLog.GetIndices().Get("y"))
	require.Equal(t, "first", gotLog.GetIndices().Get("content"))
	require.False(t, gotLog.GetIndices().Contains("shared"))
	gotSecond, ok := received[1].(*models.Log)
	require.True(t, ok)
	require.Equal(t, second.GetTags().Iterator(), gotSecond.GetTags().Iterator())
	require.Equal(t, []byte("second"), gotSecond.GetIndices().Get("content"))
	require.Zero(t, gotSecond.ObservedTimestamp)

	gotMetric, ok := received[2].(*models.Metric)
	require.True(t, ok)
	require.Equal(t, metric.GetTags().Iterator(), gotMetric.GetTags().Iterator())
	require.Equal(t, metric.MetricType, gotMetric.MetricType)
	require.Equal(t, metric.GetTimestamp(), gotMetric.GetTimestamp())
	require.Equal(t, metric.Unit, gotMetric.Unit)
	require.Equal(t, metric.Description, gotMetric.Description)
	require.Equal(t, 42.5, gotMetric.GetValue().GetSingleValue())
	require.Equal(t, metric.TypedValue.Get("source"), gotMetric.TypedValue.Get("source"))
	require.Equal(t, metric.TypedValue.Get("ready"), gotMetric.TypedValue.Get("ready"))
	gotTypedOnly, ok := received[3].(*models.Metric)
	require.True(t, ok)
	require.False(t, gotTypedOnly.GetValue().IsSingleValue())
	require.False(t, gotTypedOnly.GetValue().IsMultiValues())
	require.Equal(t, typedOnly.TypedValue.Get("version"), gotTypedOnly.TypedValue.Get("version"))

	gotSpan, ok := received[4].(*models.Span)
	require.True(t, ok)
	require.Equal(t, span.TraceID, gotSpan.TraceID)
	require.Equal(t, span.GetStartTime(), gotSpan.GetStartTime())
	require.Equal(t, span.GetEndTime(), gotSpan.GetEndTime())
	require.Equal(t, span.GetObservedTimestamp(), gotSpan.GetObservedTimestamp())
	require.Equal(t, "mark", gotSpan.GetEvents()[0].Name)
	require.Equal(t, "linked", gotSpan.GetLinks()[0].TraceID)
	require.Equal(t, models.ByteArray{0x01, 0x00}, received[5])
	require.Empty(t, received[6].(models.ByteArray))
	require.Equal(t, []byte("last"), received[7].(*models.Log).GetBody())
}

func TestFlusherExportReturnsServerStatus(t *testing.T) {
	group := &models.PipelineGroupEvents{Events: []models.PipelineEvent{models.NewSimpleLog([]byte("body"), nil, 0)}}
	t.Run("negative acknowledgement", func(t *testing.T) {
		receiver := &reportReceiver{reports: make(chan *protocol.PipelineEventGroupReport, 1), ack: status.Error(codes.PermissionDenied, "reject test batch")}
		flusher := testFlusher(t, func(server *grpc.Server) {
			protocol.RegisterPipelineEventGroupReportServiceServer(server, receiver)
		})
		err := flusher.Export([]*models.PipelineGroupEvents{group}, nil)
		require.Equal(t, codes.PermissionDenied, status.Code(err), "%v", err)
		require.Len(t, receiver.reports, 1)
	})
	t.Run("legacy service only", func(t *testing.T) {
		receiver := &legacyReceiver{calls: make(chan struct{}, 1)}
		flusher := testFlusher(t, func(server *grpc.Server) {
			protocol.RegisterLogReportServiceServer(server, receiver)
		})
		err := flusher.Export([]*models.PipelineGroupEvents{group}, nil)
		require.Equal(t, codes.Unimplemented, status.Code(err), "%v", err)
		require.Empty(t, receiver.calls)
	})
}
