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
	"fmt"
	"sync"
	"time"

	"go.opentelemetry.io/collector/pdata/plog"
	"go.opentelemetry.io/collector/pdata/plog/plogotlp"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.opentelemetry.io/collector/pdata/pmetric/pmetricotlp"
	"go.opentelemetry.io/collector/pdata/ptrace"
	"go.opentelemetry.io/collector/pdata/ptrace/ptraceotlp"
	"google.golang.org/grpc"
	"google.golang.org/grpc/connectivity"
	"google.golang.org/grpc/metadata"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
)

type Version string

var v1 Version = "v1"

type FlusherOTLP struct {
	Version Version                  `json:"Version"`
	Logs    *helper.GrpcClientConfig `json:"Logs"`
	Metrics *helper.GrpcClientConfig `json:"Metrics"`
	Traces  *helper.GrpcClientConfig `json:"Traces"`

	converter    *converter.Converter
	metadata     metadata.MD
	context      pipeline.Context
	logClient    *grpcClient[plogotlp.GRPCClient]
	metricClient *grpcClient[pmetricotlp.GRPCClient]
	traceClient  *grpcClient[ptraceotlp.GRPCClient]
}

func (f *FlusherOTLP) Description() string {
	return "Open Telemetry flusher for ilogtail"
}

func (f *FlusherOTLP) Init(ctx pipeline.Context) error {
	f.context = ctx
	logger.Info(f.context.GetRuntimeContext(), "otlp flusher init", "initializing")
	convert, err := f.getConverter()
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init otlp converter fail, error", err)
		return err
	}
	f.converter = convert

	if f.Logs != nil {
		grpcConn, err := buildGrpcClientConn(f.Logs)
		if err != nil {
			logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init otlp logs gRPC conn fail, error", err)
		} else {
			logger.Info(f.context.GetRuntimeContext(), "otlp logs flusher endpoint", f.Logs.Endpoint)
			logMeta := metadata.New(f.Logs.Headers)
			f.logClient = newGrpcClient(plogotlp.NewGRPCClient(grpcConn), grpcConn, f.Logs, logMeta)
		}
	}
	if f.Metrics != nil {
		grpcConn, err := buildGrpcClientConn(f.Metrics)
		if err != nil {
			logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init otlp metrics gRPC conn fail, error", err)
		} else {
			logger.Info(f.context.GetRuntimeContext(), "otlp metrics flusher endpoint", f.Metrics.Endpoint)
			metricMeta := metadata.New(f.Metrics.Headers)
			f.metricClient = newGrpcClient(pmetricotlp.NewGRPCClient(grpcConn), grpcConn, f.Metrics, metricMeta)
		}
	}

	if f.Traces != nil {
		grpcConn, err := buildGrpcClientConn(f.Traces)
		if err != nil {
			logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init otlp traces gRPC conn fail, error", err)
		} else {
			logger.Info(f.context.GetRuntimeContext(), "otlp traces flusher endpoint", f.Traces.Endpoint)
			traceMeta := metadata.New(f.Traces.Headers)
			f.traceClient = newGrpcClient(ptraceotlp.NewGRPCClient(grpcConn), grpcConn, f.Traces, traceMeta)
		}

	}

	if f.logClient == nil && f.metricClient == nil && f.traceClient == nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init otlp flusher fail, error", "invalid gRPC configs")
		return fmt.Errorf("invalid_grpc_configs")
	}

	logger.Info(f.context.GetRuntimeContext(), "otlp flusher init", "initialized")
	return nil
}

func (f *FlusherOTLP) getConverter() (*converter.Converter, error) {
	switch f.Version {
	case v1:
		return converter.NewConverter(converter.ProtocolOtlpV1, converter.EncodingNone, nil, nil, f.context.GetPipelineScopeConfig())
	default:
		return nil, fmt.Errorf("unsupported otlp log protocol version : %s", f.Version)
	}
}

func (f *FlusherOTLP) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	if f.logClient == nil {
		return nil
	}
	request := f.convertLogGroupToRequest(logGroupList)
	if !f.logClient.grpcConfig.Retry.Enable {
		return timeoutFlush[plogotlp.ExportRequest, plogotlp.ExportResponse](f.logClient.client, request, f.metadata, f.logClient.grpcConfig)
	}
	return flushWithRetry[plogotlp.ExportRequest, plogotlp.ExportResponse](f.logClient.client, request, f.metadata, f.logClient.grpcConfig)
}

// Export data to destination, such as gRPC, console, file, etc.
// It is expected to return no error at most time because IsReady will be called
// before it to make sure there is space for next data.
func (f *FlusherOTLP) Export(pipelinegroupeEventSlice []*models.PipelineGroupEvents, ctx pipeline.PipelineContext) error {
	logReq, metricReq, traceReq := f.convertPipelinesGroupeEventsToRequest(pipelinegroupeEventSlice)
	return f.flushRequests(logReq, metricReq, traceReq)
}

func (f *FlusherOTLP) SetUrgent(flag bool) {
}

// IsReady is ready to flush
func (f *FlusherOTLP) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	// logs/metrics/traces clients cannot be all nil, otherwise initialization would fail.
	ready := (f.logClient == nil || f.logClient.isReady()) &&
		(f.metricClient == nil || f.metricClient.isReady()) &&
		(f.traceClient == nil || f.traceClient.isReady())

	if !ready {
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_READY_ALARM", "otlp flusher is not ready, all gRPC conn is nil")
	}
	return ready
}

// Stop ...
func (f *FlusherOTLP) Stop() error {
	var err error

	if f.logClient.grpcConn != nil {
		err = f.logClient.grpcConn.Close()
		if err != nil {
			logger.Error(f.context.GetRuntimeContext(), "FLUSHER_STOP_ALARM", "stop otlp logs flusher fail, error", err)
		}
	}

	if f.metricClient.grpcConn != nil {
		err = f.metricClient.grpcConn.Close()
		if err != nil {
			logger.Error(f.context.GetRuntimeContext(), "FLUSHER_STOP_ALARM", "stop otlp metrics flusher fail, error", err)
		}
	}

	if f.traceClient.grpcConn != nil {
		err = f.traceClient.grpcConn.Close()
		if err != nil {
			logger.Error(f.context.GetRuntimeContext(), "FLUSHER_STOP_ALARM", "stop otlp traces flusher fail, error", err)
		}
	}

	return err
}

func (f *FlusherOTLP) convertPipelinesGroupeEventsToRequest(pipelinegroupeEventSlice []*models.PipelineGroupEvents) (plogotlp.ExportRequest, pmetricotlp.ExportRequest, ptraceotlp.ExportRequest) {
	logs := plog.NewLogs()
	metrics := pmetric.NewMetrics()
	traces := ptrace.NewTraces()

	for _, ps := range pipelinegroupeEventSlice {
		resourceLog, resourceMetric, resourceTrace, err := converter.ConvertPipelineEventToOtlpEvent(f.converter, ps)
		if err != nil {
			logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init gRPC client options fail, error", err)
		}
		if resourceLog.ScopeLogs().Len() > 0 {
			newLog := logs.ResourceLogs().AppendEmpty()
			resourceLog.MoveTo(newLog)
		}

		if resourceMetric.ScopeMetrics().Len() > 0 {
			newMetric := metrics.ResourceMetrics().AppendEmpty()
			resourceMetric.MoveTo(newMetric)
		}

		if resourceTrace.ScopeSpans().Len() > 0 {
			newTrace := traces.ResourceSpans().AppendEmpty()
			resourceTrace.MoveTo(newTrace)
		}
	}

	return plogotlp.NewExportRequestFromLogs(logs),
		pmetricotlp.NewExportRequestFromMetrics(metrics),
		ptraceotlp.NewExportRequestFromTraces(traces)
}

func (f *FlusherOTLP) convertLogGroupToRequest(logGroupList []*protocol.LogGroup) plogotlp.ExportRequest {
	logs := plog.NewLogs()
	for _, logGroup := range logGroupList {
		c, _ := f.converter.Do(logGroup)
		if log, ok := c.(plog.ResourceLogs); ok {
			if log.ScopeLogs().Len() > 0 {
				newLog := logs.ResourceLogs().AppendEmpty()
				log.MoveTo(newLog)
			}
		}
	}

	return plogotlp.NewExportRequestFromLogs(logs)
}

func (f *FlusherOTLP) flushRequests(log plogotlp.ExportRequest, metric pmetricotlp.ExportRequest, trace ptraceotlp.ExportRequest) error {
	wg := sync.WaitGroup{}
	errChan := make(chan error, 3)

	if f.logClient != nil {
		wg.Add(1)
		go func() {
			defer wg.Done()
			var err error
			if f.logClient.grpcConfig.Retry.Enable {
				err = flushWithRetry[plogotlp.ExportRequest, plogotlp.ExportResponse](f.logClient.client, log, f.metadata, f.logClient.grpcConfig)
			} else {
				err = timeoutFlush[plogotlp.ExportRequest, plogotlp.ExportResponse](f.logClient.client, log, f.metadata, f.logClient.grpcConfig)
			}

			if err != nil {
				logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "send data to otlp server fail, error", err)
			}
			errChan <- err
		}()
	}

	if f.metricClient != nil {
		wg.Add(1)
		go func() {
			defer wg.Done()
			var err error
			if f.metricClient.grpcConfig.Retry.Enable {
				err = flushWithRetry[pmetricotlp.ExportRequest, pmetricotlp.ExportResponse](f.metricClient.client, metric, f.metadata, f.metricClient.grpcConfig)
			} else {
				err = timeoutFlush[pmetricotlp.ExportRequest, pmetricotlp.ExportResponse](f.metricClient.client, metric, f.metadata, f.metricClient.grpcConfig)
			}

			if err != nil {
				logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "send metric data to otlp server fail, error", err)
			}
			errChan <- err
		}()
	}

	if f.traceClient != nil {
		wg.Add(1)
		go func() {
			defer wg.Done()
			var err error
			if f.traceClient.grpcConfig.Retry.Enable {
				err = flushWithRetry[ptraceotlp.ExportRequest, ptraceotlp.ExportResponse](f.traceClient.client, trace, f.metadata, f.traceClient.grpcConfig)
			} else {
				err = timeoutFlush[ptraceotlp.ExportRequest, ptraceotlp.ExportResponse](f.traceClient.client, trace, f.metadata, f.traceClient.grpcConfig)
			}

			if err != nil {
				logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "send trace data to otlp server fail, error", err)
			}
			errChan <- err
		}()

	}

	wg.Wait()
	return <-errChan
}

func flushWithRetry[
	T plogotlp.ExportRequest | pmetricotlp.ExportRequest | ptraceotlp.ExportRequest,
	P plogotlp.ExportResponse | pmetricotlp.ExportResponse | ptraceotlp.ExportResponse,
	Q interface {
		Export(ctx context.Context, request T, opts ...grpc.CallOption) (P, error)
	},
](q Q, request T, metadata metadata.MD, grpcConfig *helper.GrpcClientConfig) error {
	var retryNum = 0

	for {
		err := timeoutFlush[T, P](q, request, metadata, grpcConfig)
		if err == nil {
			return nil
		}
		retry := helper.GetRetryInfo(err)
		if retry == nil || retryNum >= grpcConfig.Retry.MaxCount {
			return err
		}

		retryNum++
		<-time.After(retry.ShouldDelay(grpcConfig.Retry.DefaultDelay))
	}
}

func timeoutFlush[
	T plogotlp.ExportRequest | pmetricotlp.ExportRequest | ptraceotlp.ExportRequest,
	P plogotlp.ExportResponse | pmetricotlp.ExportResponse | ptraceotlp.ExportResponse,
	Q interface {
		Export(ctx context.Context, request T, opts ...grpc.CallOption) (P, error)
	},
](
	q Q, req T, metadata metadata.MD, grpcConfig *helper.GrpcClientConfig) error {
	timeout, canal := context.WithTimeout(withMetadata(metadata), grpcConfig.GetTimeout())
	defer canal()
	_, err := q.Export(timeout, req)
	return err

}

// append withMetadata to context. refer to https://github.com/open-telemetry/opentelemetry-collector/blob/main/exporter/otlpexporter/otlp.go#L121
func withMetadata(md metadata.MD) context.Context {
	if md.Len() > 0 {
		return metadata.NewOutgoingContext(context.Background(), md)
	}
	return context.Background()
}

type grpcClient[T any] struct {
	client     T
	grpcConn   *grpc.ClientConn
	grpcConfig *helper.GrpcClientConfig
	metadata   metadata.MD
}

func newGrpcClient[T any](t T, conn *grpc.ClientConn, config *helper.GrpcClientConfig, metadata metadata.MD) *grpcClient[T] {
	return &grpcClient[T]{
		client:     t,
		grpcConn:   conn,
		grpcConfig: config,
		metadata:   metadata,
	}
}

func (c *grpcClient[T]) reConnect() (connectivity.State, error) {
	var state connectivity.State
	if c == nil {
		return state, fmt.Errorf("nilt grpc client")
	}
	c.grpcConn.Connect()
	return c.grpcConn.GetState(), nil
}

func (c *grpcClient[T]) isReady() bool {
	if c == nil {
		return false
	}
	state := c.grpcConn.GetState()
	var ready bool
	switch c.grpcConn.GetState() {
	case connectivity.Ready:
		ready = true
	case connectivity.Connecting:
		if !c.grpcConfig.WaitForReady {
			ready = false
			break
		}
		timeout, canal := context.WithTimeout(context.Background(), c.grpcConfig.GetTimeout())
		defer canal()
		_ = c.grpcConn.WaitForStateChange(timeout, state)
		state = c.grpcConn.GetState()
		ready = state == connectivity.Ready
	default:
		var err error
		state, err = c.reConnect()
		if err != nil {
			return false
		}
		ready = state == connectivity.Ready
	}
	return ready
}

func buildGrpcClientConn(grpcConfig *helper.GrpcClientConfig) (*grpc.ClientConn, error) {
	dialOpts, err := grpcConfig.GetDialOptions()
	if err != nil {
		return nil, err
	}

	return grpc.Dial(grpcConfig.GetEndpoint(), dialOpts...)
}

func init() {
	pipeline.Flushers["flusher_otlp"] = func() pipeline.Flusher {
		return &FlusherOTLP{Version: v1}
	}
}
