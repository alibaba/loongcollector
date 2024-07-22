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
	"time"

	"github.com/mitchellh/mapstructure"
	"google.golang.org/grpc"
	"google.golang.org/grpc/encoding"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const gRPCName = "grpc"

type GrpcSubscriber struct {
	Address    string `mapstructure:"address" comment:"the gRPC server address, default value is :9000"`
	Network    string `mapstructure:"network" comment:"the gRPC server network, default value is tcp"`
	DelayStart string `mapstructure:"delay_start" comment:"the delay start time duration for fault injection, such as 5s"`

	channel chan *protocol.LogGroup
	cache   []*protocol.LogGroup
	server  *grpc.Server
}

func (g *GrpcSubscriber) Description() string {
	return "this a gRPC subscriber, which is the default mock backend for Ilogtail."
}

type GRPCService struct {
	ch chan *protocol.LogGroup
	protocol.UnimplementedLogReportServiceServer
}

func (g *GrpcSubscriber) GetData(int32) ([]*protocol.LogGroup, error) {
	for {
		select {
		case logGroup, ok := <-g.channel:
			if !ok {
				return g.cache, nil
			}
			g.cache = append(g.cache, logGroup)
		default:
			return g.cache, nil
		}
	}
}

func (g *GrpcSubscriber) Start() error {
	delayDuration, err := time.ParseDuration(g.DelayStart)
	if err != nil {
		return err
	}
	encoding.RegisterCodec(new(protocol.Codec))
	g.server = grpc.NewServer()
	listener, err := net.Listen(g.Network, g.Address)
	if err != nil {
		return err
	}
	serv := &GRPCService{ch: g.channel}
	protocol.RegisterLogReportServiceServer(g.server, serv)
	go func() {
		logger.Infof(context.Background(), "the grpc server would start in %s", g.DelayStart)
		time.Sleep(delayDuration)
		err = g.server.Serve(listener)
		if err != nil {
			logger.Error(context.Background(), "GRPC_SERVER_ALARM", "err", err)
		}
	}()
	return nil
}

func (g *GrpcSubscriber) Stop() error {
	if g.server != nil {
		g.server.Stop()
	}
	close(g.channel)
	return nil
}

func (g *GrpcSubscriber) SubscribeChan() <-chan *protocol.LogGroup {
	return g.channel
}

func (t *GRPCService) Collect(stream protocol.LogReportService_CollectServer) error {
	for {
		logGroup, err := stream.Recv()
		if err == io.EOF {
			return stream.SendAndClose(&protocol.Response{
				Code:    protocol.ResponseCode_Success,
				Message: "success",
			})
		}
		if err != nil {
			return err
		}
		t.ch <- logGroup
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
		g.channel = make(chan *protocol.LogGroup, 1000)
		err := g.Start()
		return g, err
	})

	doc.Register("subscriber", gRPCName, new(GrpcSubscriber))
}
