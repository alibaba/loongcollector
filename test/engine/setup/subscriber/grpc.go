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

package subscriber

import (
	"context"
	"fmt"
	"io"
	"net"
	"strings"
	"sync"
	"time"

	"github.com/mitchellh/mapstructure"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

const gRPCName = "grpc"

type GrpcSubscriber struct {
	Address    string `mapstructure:"address" comment:"the gRPC server address, default value is :9000"`
	Network    string `mapstructure:"network" comment:"the gRPC server network, default value is tcp"`
	DelayStart string `mapstructure:"delay_start" comment:"the delay start time duration for fault injection, such as 5s"`

	channel     chan grpcBatch
	mu          sync.Mutex
	logGroups   []*protocol.LogGroup
	eventGroups []*models.PipelineGroupEvents
	server      *grpc.Server
}

func (g *GrpcSubscriber) Description() string {
	return "gRPC subscriber for the default LoongCollector mock backend"
}

type grpcBatch struct {
	legacy *protocol.LogGroup
	events *models.PipelineGroupEvents
}

type legacyGRPCService struct {
	ch chan grpcBatch
	protocol.UnimplementedLogReportServiceServer
}

type eventGRPCService struct {
	ch chan grpcBatch
	protocol.UnimplementedPipelineEventGroupReportServiceServer
}

// GetData projects received events into the existing E2E view used by the
// other subscribers. This projection is not part of either gRPC wire path.
func (g *GrpcSubscriber) GetData(string, int32) ([]*protocol.LogGroup, error) {
	g.mu.Lock()
	defer g.mu.Unlock()
	if err := g.drain(); err != nil {
		return nil, err
	}
	return g.logGroups, nil
}

// GetEventData exposes the native view for assertions on contents and tags.
func (g *GrpcSubscriber) GetEventData() ([]*models.PipelineGroupEvents, error) {
	g.mu.Lock()
	defer g.mu.Unlock()
	if err := g.drain(); err != nil {
		return nil, err
	}
	return g.eventGroups, nil
}

func (g *GrpcSubscriber) drain() error {
	for {
		select {
		case batch := <-g.channel:
			if batch.legacy != nil {
				g.logGroups = append(g.logGroups, batch.legacy)
				continue
			}
			logGroups, err := projectEventReport(batch.events)
			if err != nil {
				return err
			}
			g.eventGroups = append(g.eventGroups, batch.events)
			g.logGroups = append(g.logGroups, logGroups...)
		default:
			return nil
		}
	}
}

func projectEventReport(input *models.PipelineGroupEvents) ([]*protocol.LogGroup, error) {
	var groups []*protocol.LogGroup
	err := helper.ForEachLogTagGroup(input, func(group *models.PipelineGroupEvents) error {
		rows, err := helper.EncodeEventRecords(group)
		if err != nil {
			return err
		}
		out := &protocol.LogGroup{
			Category: group.Group.GetDestinationLogstore(),
			Logs:     make([]*protocol.Log, 0, len(rows)),
		}
		for key, value := range group.Group.GetTags().Iterator() {
			if key == pipeline.TopicTagKey {
				out.Topic = value
			} else {
				out.LogTags = append(out.LogTags, &protocol.LogTag{Key: key, Value: value})
			}
		}
		for _, row := range rows {
			log := &protocol.Log{Time: uint32(row.Timestamp / 1e9), Contents: make([]*protocol.Log_Content, 0, len(row.Fields))}
			if ns := uint32(row.Timestamp % 1e9); ns != 0 {
				log.TimeNs = &ns
			}
			for key, value := range row.Fields {
				log.Contents = append(log.Contents, &protocol.Log_Content{Key: key, Value: value})
			}
			out.Logs = append(out.Logs, log)
		}
		groups = append(groups, out)
		return nil
	})
	return groups, err
}

func (g *GrpcSubscriber) Start() error {
	delayDuration, err := time.ParseDuration(g.DelayStart)
	if err != nil {
		return err
	}
	g.server = grpc.NewServer(grpc.ForceServerCodec(protocol.Codec{}))
	listener, err := net.Listen(g.Network, g.Address)
	if err != nil {
		return err
	}
	protocol.RegisterPipelineEventGroupReportServiceServer(g.server, &eventGRPCService{ch: g.channel})
	protocol.RegisterLogReportServiceServer(g.server, &legacyGRPCService{ch: g.channel})
	go func() {
		logger.Infof(context.Background(), "the grpc server would start in %s", g.DelayStart)
		time.Sleep(delayDuration)
		if err := g.server.Serve(listener); err != nil {
			logger.Error(context.Background(), selfmonitor.GRPCServerAlarm, "err", err)
		}
	}()
	return nil
}

func (g *GrpcSubscriber) Stop() error {
	if g.server != nil {
		g.server.Stop()
	}
	return nil
}

func (t *legacyGRPCService) Collect(stream protocol.LogReportService_CollectServer) error {
	for {
		group, err := stream.Recv()
		if err == io.EOF {
			return stream.SendAndClose(&protocol.Response{
				Code:    protocol.ResponseCode_Success,
				Message: "success",
			})
		}
		if err != nil {
			return err
		}
		select {
		case t.ch <- grpcBatch{legacy: group}:
		case <-stream.Context().Done():
			return stream.Context().Err()
		}
	}
}

func (t *eventGRPCService) Collect(stream protocol.PipelineEventGroupReportService_CollectServer) error {
	for {
		report, err := stream.Recv()
		if err == io.EOF {
			return stream.SendAndClose(&protocol.PipelineEventGroupReportResponse{})
		}
		if err != nil {
			return err
		}
		group, err := helper.DecodePipelineEventReport(report)
		if err != nil {
			return status.Errorf(codes.InvalidArgument, "decode event report: %v", err)
		}
		select {
		case t.ch <- grpcBatch{events: group}:
		case <-stream.Context().Done():
			return stream.Context().Err()
		}
	}
}

func (g *GrpcSubscriber) Name() string {
	return gRPCName
}

func (g *GrpcSubscriber) FlusherConfig() string {
	parts := strings.Split(g.Address, ":")
	return fmt.Sprintf(`
flushers:
  - Type: flusher_grpc
    Address: host.docker.internal:%s`, parts[len(parts)-1])
}

func init() {
	RegisterCreator(gRPCName, func(spec map[string]interface{}) (Subscriber, error) {
		g := new(GrpcSubscriber)
		if err := mapstructure.Decode(spec, g); err != nil {
			return nil, err
		}
		if g.Address == "" {
			g.Address = ":9000"
		}
		if g.Network == "" {
			g.Network = "tcp"
		}
		if g.DelayStart == "" {
			g.DelayStart = "0s"
		}
		g.channel = make(chan grpcBatch, 1000)
		err := g.Start()
		return g, err
	})

	doc.Register("subscriber", gRPCName, new(GrpcSubscriber))
}
