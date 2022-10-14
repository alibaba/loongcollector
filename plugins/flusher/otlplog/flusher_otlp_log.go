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

package otlplog

import (
	"context"
	"fmt"
	"time"

	otlpv1 "go.opentelemetry.io/proto/otlp/collector/logs/v1"
	logv1 "go.opentelemetry.io/proto/otlp/logs/v1"
	"google.golang.org/grpc"
	"google.golang.org/grpc/connectivity"
	"google.golang.org/grpc/metadata"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/helper"
)

type Version string

var v1 Version = "v1"

type FlusherOTLPLog struct {
	GrpcConfig *helper.GrpcClientConfig `json:"Grpc"`
	Version    Version                  `json:"Version"`

	converter *converter.Converter
	metadata  metadata.MD
	context   ilogtail.Context
	grpcConn  *grpc.ClientConn
	logClient otlpv1.LogsServiceClient
}

func (f *FlusherOTLPLog) Description() string {
	return "OTLP Log flusher for logtail"
}

func (f *FlusherOTLPLog) Init(ctx ilogtail.Context) error {
	f.context = ctx
	logger.Info(f.context.GetRuntimeContext(), "otlplog flusher init", "initializing")
	convert, err := f.getConverter()
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init otlp_log converter fail, error", err)
		return err
	}
	f.converter = convert

	if f.GrpcConfig == nil {
		err = fmt.Errorf("GrpcConfig cannit be nil")
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init otlp_log converter fail, error", err)
		return err
	}

	dialOpts, err := f.GrpcConfig.GetDialOptions()
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init gRPC client options fail, error", err)
		return err
	}
	logger.Info(f.context.GetRuntimeContext(), "otlplog flusher init", "initializing gRPC conn", "endpoint", f.GrpcConfig.GetEndpoint())

	if f.grpcConn, err = grpc.Dial(f.GrpcConfig.GetEndpoint(), dialOpts...); err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init otlplog gRPC conn fail, error", err)
		return err
	}

	logger.Info(f.context.GetRuntimeContext(), "init otlplog gRPC conn. status", f.grpcConn.GetState())
	f.metadata = metadata.New(f.GrpcConfig.Headers)
	f.logClient = otlpv1.NewLogsServiceClient(f.grpcConn)
	logger.Info(f.context.GetRuntimeContext(), "otlplog flusher init", "initialized")
	return nil
}

func (f *FlusherOTLPLog) getConverter() (*converter.Converter, error) {
	switch f.Version {
	case v1:
		return converter.NewConverter(converter.ProtocolOtlpLogV1, converter.EncodingNone, nil, nil)
	default:
		return nil, fmt.Errorf("unsupported otlp log protocol version : %s", f.Version)
	}
}

func (f *FlusherOTLPLog) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	request := f.convertLogGroupToRequest(logGroupList)
	if !f.GrpcConfig.Retry.Enable {
		return f.flush(request)
	}
	return f.flushWithRetry(request)
}

func (f *FlusherOTLPLog) convertLogGroupToRequest(logGroupList []*protocol.LogGroup) *otlpv1.ExportLogsServiceRequest {
	resourceLogs := make([]*logv1.ResourceLogs, 0)
	for _, logGroup := range logGroupList {
		c, _ := f.converter.Do(logGroup)
		if log, ok := c.(*logv1.ResourceLogs); ok {
			resourceLogs = append(resourceLogs, log)
		}
	}
	return &otlpv1.ExportLogsServiceRequest{
		ResourceLogs: resourceLogs,
	}
}

func (f *FlusherOTLPLog) flush(request *otlpv1.ExportLogsServiceRequest) error {
	if err := f.timeoutFlush(request); err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "send data to otlplog server fail, error", err)
		return err
	}
	return nil
}

func (f *FlusherOTLPLog) flushWithRetry(request *otlpv1.ExportLogsServiceRequest) error {
	var retryNum = 0

	for {
		err := f.timeoutFlush(request)
		if err == nil {
			return nil
		}
		retry := helper.GetRetryInfo(err)
		if retry == nil || retryNum >= f.GrpcConfig.Retry.MaxCount {
			logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "send data to otlplog server fail, error", err)
			return err
		}
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "send data to otlplog server fail, will retry...")
		retryNum++
		<-time.After(retry.ShouldDelay(f.GrpcConfig.Retry.DefaultDelay))
	}
}

func (f *FlusherOTLPLog) timeoutFlush(request *otlpv1.ExportLogsServiceRequest) error {
	timeout, canal := context.WithTimeout(f.withMetadata(), f.GrpcConfig.GetTimeout())
	defer canal()
	_, err := f.logClient.Export(timeout, request)
	return err
}

// append withMetadata to context. refer to https://github.com/open-telemetry/opentelemetry-collector/blob/main/exporter/otlpexporter/otlp.go#L121
func (f *FlusherOTLPLog) withMetadata() context.Context {
	if f.metadata.Len() > 0 {
		return metadata.NewOutgoingContext(context.Background(), f.metadata)
	}
	return context.Background()
}

func (f *FlusherOTLPLog) SetUrgent(flag bool) {
}

// IsReady is ready to flush
func (f *FlusherOTLPLog) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	if f.grpcConn == nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_READY_ALARM", "otlplog flusher is not ready, gRPC conn is nil")
		return false
	}
	state := f.grpcConn.GetState()
	var ready bool
	switch f.grpcConn.GetState() {
	case connectivity.Ready:
		ready = true
	case connectivity.Connecting:
		if !f.GrpcConfig.WaitForReady {
			ready = false
			break
		}
		timeout, canal := context.WithTimeout(context.Background(), f.GrpcConfig.GetTimeout())
		defer canal()
		_ = f.grpcConn.WaitForStateChange(timeout, state)
		state = f.grpcConn.GetState()
		ready = state == connectivity.Ready
	default:
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_READY_ALARM", "gRPC conn is not ready, will reconnect. current status", f.grpcConn.GetState())
		state = f.reConnect()
		ready = state == connectivity.Ready
	}
	if !ready {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_READY_ALARM", "otlplog flusher is not ready. current status", f.grpcConn.GetState())
	}
	return ready
}

func (f *FlusherOTLPLog) reConnect() connectivity.State {
	f.grpcConn.Connect()
	return f.grpcConn.GetState()
}

// Stop ...
func (f *FlusherOTLPLog) Stop() error {
	err := f.grpcConn.Close()
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_STOP_ALARM", "stop otlplog flusher fail, error", err)
	}
	return err
}

func init() {
	ilogtail.Flushers["flusher_otlp_log"] = func() ilogtail.Flusher {
		return &FlusherOTLPLog{Version: v1}
	}
}
