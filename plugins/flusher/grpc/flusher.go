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

	"google.golang.org/grpc"
	"google.golang.org/grpc/connectivity"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/encoding"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

type Flusher struct {
	Address            string // The gRPC server address
	EnableTLS          bool   // Enable TLS connect to server
	CertFile           string // The client.pem path.
	KeyFile            string // The client.key path.
	CAFile             string // The ca.pem path.
	InsecureSkipVerify bool   // Controls whether a client verifies the server's certificate chain and host name.

	dialOptions []grpc.DialOption
	dialSuccess bool
	conn        *grpc.ClientConn
	client      protocol.LogReportServiceClient
	ctx         pipeline.Context
}

func (f *Flusher) Init(ctx pipeline.Context) error {
	encoding.RegisterCodec(new(protocol.Codec))
	f.ctx = ctx
	options := make([]grpc.DialOption, 0, 1)
	if f.EnableTLS {
		cfg, err := util.GetTLSConfig(f.CertFile, f.KeyFile, f.CAFile, f.InsecureSkipVerify)
		if err != nil {
			logger.Errorf(f.ctx.GetRuntimeContext(), "GRPC_FLUSHER_ALARM", "error in creating TLS config,: %v", err)
			return err
		}
		options = append(options, grpc.WithTransportCredentials(credentials.NewTLS(cfg)))
	} else {
		options = append(options, grpc.WithInsecure())
	}
	f.dialOptions = options

	c, err := grpc.Dial(f.Address, options...)
	if err != nil {
		logger.Errorf(f.ctx.GetRuntimeContext(), "GRPC_FLUSHER_ALARM", "error in dialing with gRPC server %s, would try again later", f.Address)
	} else {
		f.conn = c
		f.client = protocol.NewLogReportServiceClient(c)
		f.dialSuccess = true
	}
	return nil
}

func (f *Flusher) Description() string {
	return "flush log group with gRPC transfer channel"
}

func (f *Flusher) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	if f.conn != nil {
		if f.conn.GetState() == connectivity.Ready {
			return true
		}
		_ = f.conn.Close()
	}
	// force try to dial with server.
	c, err := grpc.Dial(f.Address, f.dialOptions...)
	if err != nil {
		logger.Errorf(f.ctx.GetRuntimeContext(), "GRPC_FLUSHER_ALARM", "error in dialing with gRPC server %s, would try again later", f.Address)
		return false
	}
	f.conn = c
	f.client = protocol.NewLogReportServiceClient(c)
	return f.conn.GetState() == connectivity.Ready
}

func (f *Flusher) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	stream, err := f.client.Collect(context.Background())
	defer f.closeStream(stream)
	if err != nil {
		logger.Error(f.ctx.GetRuntimeContext(), "GRPC_FLUSH_ALARM", "err", err)
		return err
	}
	for _, group := range logGroupList {
		if len(group.Logs) == 0 {
			continue
		}
		if err := stream.Send(group); err != nil {
			logger.Error(f.ctx.GetRuntimeContext(), "GRPC_FLUSH_ALARM", "err", err)
			return err
		}
	}
	return nil
}

func (f *Flusher) SetUrgent(flag bool) {
}

func (f *Flusher) Stop() error {
	return f.conn.Close()
}

func (f *Flusher) closeStream(stream protocol.LogReportService_CollectClient) {
	_, err := stream.CloseAndRecv()
	if err != nil && err != io.EOF {
		logger.Info(f.ctx.GetRuntimeContext(), "stage", "close stream", "err", err)
	}
}

func init() {
	pipeline.Flushers["flusher_grpc"] = func() pipeline.Flusher {
		return &Flusher{
			Address: ":8000",
		}
	}
}
