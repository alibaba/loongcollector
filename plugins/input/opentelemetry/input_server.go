// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      grpc://www.apache.org/licenses/LICENSE-2.0
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
	"net"
	"net/http"
	"net/url"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/helper/decoder"
	"github.com/alibaba/ilogtail/helper/decoder/common"
	"github.com/alibaba/ilogtail/helper/decoder/opentelemetry"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/plugins/input/httpserver"
	"go.opentelemetry.io/collector/pdata/plog/plogotlp"
	"go.opentelemetry.io/collector/pdata/pmetric/pmetricotlp"
	"go.opentelemetry.io/collector/pdata/ptrace/ptraceotlp"
	"google.golang.org/grpc"
)

const (
	defaultGRPCEndpoint = "0.0.0.0:4317"
	defaultHTTPEndpoint = "0.0.0.0:4318"
)

const (
	pbContentType   = "application/x-protobuf"
	jsonContentType = "application/json"
)

// Server implements ServiceInputV2
// It can only work in v2 pipelines.
type Server struct {
	context         ilogtail.Context
	piplineContext  ilogtail.PipelineContext
	serverGPRC      *grpc.Server
	serverHTTP      *http.Server
	grpcListener    net.Listener
	httpListener    net.Listener
	logsReceiver    plogotlp.GRPCServer // currently logs are not supported
	tracesReceiver  ptraceotlp.GRPCServer
	metricsReceiver pmetricotlp.GRPCServer
	wg              sync.WaitGroup

	Protocals Protocals
}

// Init ...
func (s *Server) Init(context ilogtail.Context) (int, error) {
	s.context = context
	logger.Info(s.context.GetRuntimeContext(), "otlp server init", "initializing")

	if s.Protocals.Grpc != nil {
		if s.Protocals.Grpc.Endpoint == "" {
			s.Protocals.Grpc.Endpoint = defaultGRPCEndpoint
		}

	}

	if s.Protocals.Http != nil {
		if s.Protocals.Http.Endpoint == "" {
			s.Protocals.Http.Endpoint = defaultGRPCEndpoint
		}
		if s.Protocals.Http.ReadTimeoutSec == 0 {
			s.Protocals.Http.ReadTimeoutSec = 10
		}

		if s.Protocals.Http.ShutdownTimeoutSec == 0 {
			s.Protocals.Http.ShutdownTimeoutSec = 5
		}

		if s.Protocals.Http.MaxRequestBodySizeMiB == 0 {
			s.Protocals.Http.MaxRequestBodySizeMiB = 64
		}

	}

	logger.Info(s.context.GetRuntimeContext(), "otlp server init", "initialized", "gRPC settings", s.Protocals.Grpc, "HTTP setting", s.Protocals.Http)
	return 0, nil
}

// Description ...
func (s *Server) Description() string {
	return "Open-Telemetry service input plugin for logtail"
}

// Start ...
func (s *Server) Start(c ilogtail.Collector) error {
	return nil
}

// StartService(PipelineContext) error
func (s *Server) StartService(ctx ilogtail.PipelineContext) error {
	s.piplineContext = ctx
	s.tracesReceiver = newTracesReceiver(ctx)
	s.metricsReceiver = newMetricsReceiver(ctx)
	s.logsReceiver = newLogsReceiver(ctx)

	if s.Protocals.Grpc != nil {
		grpcServer := grpc.NewServer(
			serverGRPCOptions(s.Protocals.Grpc)...,
		)
		s.serverGPRC = grpcServer
		listener, err := getNetListener(s.Protocals.Grpc.Endpoint)
		if err != nil {
			return err
		}
		s.grpcListener = listener

		ptraceotlp.RegisterGRPCServer(s.serverGPRC, s.tracesReceiver)
		pmetricotlp.RegisterGRPCServer(s.serverGPRC, s.metricsReceiver)
		plogotlp.RegisterGRPCServer(s.serverGPRC, s.logsReceiver)
		logger.Info(s.context.GetRuntimeContext(), "otlp grpc receiver for logs/metrics/traces", "initialized")

		s.wg.Add(1)
		go func() {
			logger.Info(s.context.GetRuntimeContext(), "otlp grpc server start", s.Protocals.Grpc.Endpoint)
			_ = s.serverGPRC.Serve(listener)
			s.serverGPRC.GracefulStop()
			logger.Info(s.context.GetRuntimeContext(), "otlp grpc server shutdown", s.Protocals.Grpc.Endpoint)
			s.wg.Done()
		}()
	}

	if s.Protocals.Http != nil {
		httpMux := http.NewServeMux()
		maxBodySize := int64(s.Protocals.Http.MaxRequestBodySizeMiB) * 1024 * 1024

		s.registerHTTPLogsComsumer(httpMux, &opentelemetry.Decoder{Format: common.ProtocolOTLPLogV1}, maxBodySize, "/v1/logs")
		s.registerHTTPMetricsComsumer(httpMux, &opentelemetry.Decoder{Format: common.ProtocolOTLPMetricV1}, maxBodySize, "/v1/metrics")
		s.registerHTTPTracesComsumer(httpMux, &opentelemetry.Decoder{Format: common.ProtocolOTLPTraceV1}, maxBodySize, "/v1/traces")
		logger.Info(s.context.GetRuntimeContext(), "otlp http receiver for logs/metrics/traces", "initialized")

		httpServer := &http.Server{
			Addr:        s.Protocals.Http.Endpoint,
			Handler:     httpMux,
			ReadTimeout: time.Duration(s.Protocals.Http.ReadTimeoutSec) * time.Second,
		}

		s.serverHTTP = httpServer
		listener, err := getNetListener(s.Protocals.Http.Endpoint)
		if err != nil {
			return err
		}
		s.httpListener = listener
		logger.Info(s.context.GetRuntimeContext(), "otlp http server init", "initialized")

		s.wg.Add(1)
		go func() {
			logger.Info(s.context.GetRuntimeContext(), "otlp http server start", s.Protocals.Http.Endpoint)
			_ = s.serverHTTP.Serve(s.httpListener)
			ctx, cancel := context.WithTimeout(context.Background(), time.Duration(s.Protocals.Http.ShutdownTimeoutSec)*time.Second)
			defer cancel()
			_ = s.serverHTTP.Shutdown(ctx)
			logger.Info(s.context.GetRuntimeContext(), "otlp http server shutdown", s.Protocals.Http.Endpoint)
			s.wg.Done()
		}()

	}
	return nil
}

// Stop stops the services and closes any necessary channels and connections
func (s *Server) Stop() error {
	if s.grpcListener != nil {
		_ = s.grpcListener.Close()
		logger.Info(s.context.GetRuntimeContext(), "otlp grpc server stop", s.Protocals.Grpc.Endpoint)
		s.wg.Wait()
	}

	if s.httpListener != nil {
		_ = s.httpListener.Close()
		logger.Info(s.context.GetRuntimeContext(), "otlp http server stop", s.Protocals.Http.Endpoint)
		s.wg.Wait()
	}
	return nil
}

func (s *Server) registerHTTPLogsComsumer(serveMux *http.ServeMux, decoder decoder.Decoder, MaxBodySize int64, routing string) error {
	serveMux.HandleFunc(routing, func(w http.ResponseWriter, r *http.Request) {
		data, _, err := handleInvalidRequest(w, r, MaxBodySize, decoder)
		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), "READ_BODY_FAIL_ALARM", "read body failed", err, "request", r.URL.String())
			return
		}

		otlpLogReq := plogotlp.NewExportRequest()
		otlpLogReq, err = opentelemetry.DecodeOtlpRequest(otlpLogReq, data, r)
		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), "DECODE_BODY_FAIL_ALARM", "decode body failed", err, "request", r.URL.String())
			httpserver.BadRequest(w)
			return
		}

		otlpResp, err := s.logsReceiver.Export(r.Context(), otlpLogReq)
		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), "EXPORT_REQ_FAIL_ALARM", "export logs failed", err, "request", r.URL.String())
			httpserver.BadRequest(w)
			return
		}

		msg, contentType, err := marshalResp(otlpResp, r)

		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), "MARSHAL_RESP_FAIL_ALARM", "marshal resp failed", err, "request", r.URL.String())
			httpserver.BadRequest(w)
			return
		}

		writeResponse(w, contentType, http.StatusOK, msg)
	})
	return nil
}

func (s *Server) registerHTTPMetricsComsumer(serveMux *http.ServeMux, decoder decoder.Decoder, MaxBodySize int64, routing string) error {
	serveMux.HandleFunc(routing, func(w http.ResponseWriter, r *http.Request) {
		data, _, err := handleInvalidRequest(w, r, MaxBodySize, decoder)
		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), "READ_BODY_FAIL_ALARM", "read body failed", err, "request", r.URL.String())
			return
		}

		otlpMetricReq := pmetricotlp.NewExportRequest()
		otlpMetricReq, err = opentelemetry.DecodeOtlpRequest(otlpMetricReq, data, r)
		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), "DECODE_BODY_FAIL_ALARM", "decode body failed", err, "request", r.URL.String())
			httpserver.BadRequest(w)
			return
		}

		otlpResp, err := s.metricsReceiver.Export(r.Context(), otlpMetricReq)
		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), "EXPORT_REQ_FAIL_ALARM", "export metrics failed", err, "request", r.URL.String())
			httpserver.BadRequest(w)
			return
		}

		msg, contentType, err := marshalResp(otlpResp, r)
		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), "MARSHAL_RESP_FAIL_ALARM", "marshal resp failed", err, "request", r.URL.String())
			httpserver.BadRequest(w)
			return
		}

		writeResponse(w, contentType, http.StatusOK, msg)
	})
	return nil
}

func (s *Server) registerHTTPTracesComsumer(serveMux *http.ServeMux, decoder decoder.Decoder, MaxBodySize int64, routing string) error {
	serveMux.HandleFunc(routing, func(w http.ResponseWriter, r *http.Request) {
		data, _, err := handleInvalidRequest(w, r, MaxBodySize, decoder)
		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), "READ_BODY_FAIL_ALARM", "read body failed", err, "request", r.URL.String())
			return
		}

		otlpTraceReq := ptraceotlp.NewExportRequest()
		otlpTraceReq, err = opentelemetry.DecodeOtlpRequest(otlpTraceReq, data, r)
		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), "DECODE_BODY_FAIL_ALARM", "decode body failed", err, "request", r.URL.String())
			httpserver.BadRequest(w)
			return
		}

		otlpResp, err := s.tracesReceiver.Export(r.Context(), otlpTraceReq)
		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), "EXPORT_REQ_FAIL_ALARM", "export traces failed", err, "request", r.URL.String())
			httpserver.BadRequest(w)
			return
		}

		msg, contentType, err := marshalResp(otlpResp, r)

		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), "MARSHAL_RESP_FAIL_ALARM", "marshal resp failed", err, "request", r.URL.String())
			httpserver.BadRequest(w)
			return
		}

		writeResponse(w, contentType, http.StatusOK, msg)
	})
	return nil
}

func serverGRPCOptions(grpcConfig *GRPCServerSettings) []grpc.ServerOption {
	var opts []grpc.ServerOption
	if grpcConfig != nil {
		if grpcConfig.MaxRecvMsgSizeMiB > 0 {
			opts = append(opts, grpc.MaxRecvMsgSize(int(grpcConfig.MaxRecvMsgSizeMiB*1024*1024)))
		}
		if grpcConfig.MaxConcurrentStreams > 0 {
			opts = append(opts, grpc.MaxConcurrentStreams(uint32(grpcConfig.MaxConcurrentStreams)))
		}

		if grpcConfig.ReadBufferSize > 0 {
			opts = append(opts, grpc.ReadBufferSize(grpcConfig.ReadBufferSize))
		}

		if grpcConfig.WriteBufferSize > 0 {
			opts = append(opts, grpc.WriteBufferSize(grpcConfig.WriteBufferSize))
		}
	}

	return opts
}

func getNetListener(endpoint string) (net.Listener, error) {
	var listener net.Listener
	var err error
	switch {
	case strings.HasPrefix(endpoint, "http") ||
		strings.HasPrefix(endpoint, "https") ||
		strings.HasPrefix(endpoint, "tcp"):
		configURL, errAddr := url.Parse(endpoint)
		if errAddr != nil {
			return nil, errAddr
		}
		listener, err = net.Listen("tcp", configURL.Host)
	default:
		listener, err = net.Listen("tcp", endpoint)
	}
	return listener, err
}

func marshalResp[
	P interface {
		MarshalProto() ([]byte, error)
		MarshalJSON() ([]byte, error)
	}](resp P, r *http.Request) ([]byte, string, error) {
	var msg []byte
	var err error
	contentType := r.Header.Get("Content-Type")
	switch contentType {
	case pbContentType:
		msg, err = resp.MarshalProto()
	case jsonContentType:
		msg, err = resp.MarshalJSON()
	}
	return msg, contentType, err
}

func handleInvalidRequest(w http.ResponseWriter, r *http.Request, MaxBodySize int64, decoder decoder.Decoder) (data []byte, statusCode int, err error) {
	if r.Method != http.MethodPost {
		handleUnmatchedMethod(w)
		return
	}

	if r.ContentLength > MaxBodySize {
		httpserver.TooLarge(w)
		return
	}

	data, statusCode, err = decoder.ParseRequest(w, r, MaxBodySize)

	switch statusCode {
	case http.StatusBadRequest:
		httpserver.BadRequest(w)
	case http.StatusRequestEntityTooLarge:
		httpserver.TooLarge(w)
	case http.StatusInternalServerError:
		httpserver.InternalServerError(w)
	case http.StatusMethodNotAllowed:
		httpserver.MethodNotAllowed(w)
	}

	return data, statusCode, err
}

func handleUnmatchedMethod(resp http.ResponseWriter) {
	status := http.StatusMethodNotAllowed
	writeResponse(resp, "text/plain", status, []byte(fmt.Sprintf("%v method not allowed, supported: [POST]", status)))
}

func writeResponse(w http.ResponseWriter, contentType string, statusCode int, msg []byte) {
	w.Header().Set("Content-Type", contentType)
	w.WriteHeader(statusCode)
	// Nothing we can do with the error if we cannot write to the response.
	_, _ = w.Write(msg)
}

type Protocals struct {
	Grpc *GRPCServerSettings
	Http *HTTPServerSettings
}

type GRPCServerSettings struct {
	Endpoint             string
	MaxRecvMsgSizeMiB    int
	MaxConcurrentStreams int
	ReadBufferSize       int
	WriteBufferSize      int
}

type HTTPServerSettings struct {
	Endpoint              string
	MaxRequestBodySizeMiB int
	ReadTimeoutSec        int
	ShutdownTimeoutSec    int
}

func init() {
	ilogtail.ServiceInputs["service_otlp"] = func() ilogtail.ServiceInput {
		return &Server{}
	}
}
