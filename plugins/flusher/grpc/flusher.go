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
	"errors"
	"fmt"
	"io"

	"google.golang.org/grpc"
	"google.golang.org/grpc/connectivity"
	"google.golang.org/grpc/credentials"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
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
	conn        *grpc.ClientConn
	client      protocol.PipelineEventGroupReportServiceClient
	ctx         pipeline.Context
}

func (f *Flusher) Init(ctx pipeline.Context) error {
	f.ctx = ctx
	options := []grpc.DialOption{grpc.WithDefaultCallOptions(grpc.ForceCodec(protocol.Codec{}))}
	if f.EnableTLS {
		cfg, err := util.GetTLSConfig(f.CertFile, f.KeyFile, f.CAFile, f.InsecureSkipVerify)
		if err != nil {
			logger.Warningf(f.ctx.GetRuntimeContext(), selfmonitor.GRPCFlusherAlarm, "error in creating TLS config,: %v", err)
			return err
		}
		options = append(options, grpc.WithTransportCredentials(credentials.NewTLS(cfg)))
	} else {
		options = append(options, grpc.WithInsecure())
	}
	f.dialOptions = options

	c, err := grpc.Dial(f.Address, options...)
	if err != nil {
		logger.Warningf(f.ctx.GetRuntimeContext(), selfmonitor.GRPCFlusherAlarm, "error in dialing with gRPC server %s, would try again later", f.Address)
	} else {
		f.conn = c
		f.client = protocol.NewPipelineEventGroupReportServiceClient(c)
	}
	return nil
}

func (f *Flusher) Description() string {
	return "flush pipeline events with gRPC"
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
		logger.Warningf(f.ctx.GetRuntimeContext(), selfmonitor.GRPCFlusherAlarm, "error in dialing with gRPC server %s, would try again later", f.Address)
		return false
	}
	f.conn = c
	f.client = protocol.NewPipelineEventGroupReportServiceClient(c)
	return f.conn.GetState() == connectivity.Ready
}

// Export sends event reports over the event RPC and waits for acknowledgment.
func (f *Flusher) Export(groups []*models.PipelineGroupEvents, _ pipeline.PipelineContext) error {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	var stream protocol.PipelineEventGroupReportService_CollectClient
	for _, group := range groups {
		err := helper.ForEachPipelineEventReport(group, func(report *protocol.PipelineEventGroupReport) error {
			if stream == nil {
				if f.client == nil {
					return fmt.Errorf("gRPC flusher is not connected")
				}
				var err error
				stream, err = f.client.Collect(ctx)
				if err != nil {
					return fmt.Errorf("open gRPC event stream: %w", err)
				}
			}
			if err := stream.Send(report); err != nil {
				if errors.Is(err, io.EOF) {
					// Send reports EOF when the server closes the stream.
					// Read the final status, including unimplemented RPCs.
					if _, statusErr := stream.CloseAndRecv(); statusErr != nil {
						err = statusErr
					}
				}
				return fmt.Errorf("send gRPC event report: %w", err)
			}
			return nil
		})
		if err != nil {
			return err
		}
	}
	if stream != nil {
		if _, err := stream.CloseAndRecv(); err != nil {
			return fmt.Errorf("acknowledge gRPC event stream: %w", err)
		}
	}
	return nil
}

func (f *Flusher) SetUrgent(flag bool) {
}

func (f *Flusher) Stop() error {
	if f.conn == nil {
		return nil
	}
	return f.conn.Close()
}

func init() {
	pipeline.Flushers["flusher_grpc"] = func() pipeline.Flusher {
		return &Flusher{
			Address: ":8000",
		}
	}
}
