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

	channel     chan *models.PipelineGroupEvents
	mu          sync.Mutex
	batches     []*Batch
	eventGroups []*models.PipelineGroupEvents
	server      *grpc.Server
}

func (g *GrpcSubscriber) Description() string {
	return "gRPC subscriber for the default LoongCollector mock backend"
}

type eventGRPCService struct {
	ch chan *models.PipelineGroupEvents
	protocol.UnimplementedPipelineEventGroupReportServiceServer
}

// GetData projects received events into the same observations as other backends.
func (g *GrpcSubscriber) GetData(string, int32) ([]*Batch, error) {
	g.mu.Lock()
	defer g.mu.Unlock()
	if err := g.drain(); err != nil {
		return nil, err
	}
	return g.batches, nil
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
		case events := <-g.channel:
			batches, err := projectEventReport(events)
			if err != nil {
				return err
			}
			g.eventGroups = append(g.eventGroups, events)
			g.batches = append(g.batches, batches...)
		default:
			return nil
		}
	}
}

func projectEventReport(input *models.PipelineGroupEvents) ([]*Batch, error) {
	var batches []*Batch
	err := helper.ForEachLogTagGroup(input, func(group *models.PipelineGroupEvents) error {
		rows, err := helper.EncodeEventRecords(group)
		if err != nil {
			return err
		}
		batch := &Batch{
			Tags:    make(map[string]string, group.Group.GetTags().Len()),
			Records: make([]*Record, 0, len(rows)),
		}
		for key, value := range group.Group.GetTags().Iterator() {
			if key != pipeline.TopicTagKey {
				batch.Tags[key] = value
			}
		}
		for _, row := range rows {
			record := &Record{Timestamp: row.Timestamp, Fields: make([]Field, 0, len(row.Fields))}
			for key, value := range row.Fields {
				record.Fields = append(record.Fields, Field{Key: key, Value: value})
			}
			batch.Records = append(batch.Records, record)
		}
		batches = append(batches, batch)
		return nil
	})
	return batches, err
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
		case t.ch <- group:
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
		g.channel = make(chan *models.PipelineGroupEvents, 1000)
		err := g.Start()
		return g, err
	})

	doc.Register("subscriber", gRPCName, new(GrpcSubscriber))
}
