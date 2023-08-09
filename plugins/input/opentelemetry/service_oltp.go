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
	"errors"
	"fmt"
	"net"
	"net/http"
	"net/url"
	"strings"
	"sync"
	"time"

	"go.opentelemetry.io/collector/pdata/plog/plogotlp"
	"go.opentelemetry.io/collector/pdata/pmetric/pmetricotlp"
	"go.opentelemetry.io/collector/pdata/ptrace/ptraceotlp"
	"google.golang.org/grpc"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/pipeline/extensions"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/common"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/opentelemetry"
	"github.com/alibaba/ilogtail/plugins/input/httpserver"
)

const (
	defaultGRPCEndpoint = "0.0.0.0:4317"
	defaultHTTPEndpoint = "0.0.0.0:4318"
)

const (
	pbContentType   = "application/x-protobuf"
	jsonContentType = "application/json"
)

var (
	errTooLargeRequest = errors.New("request_too_large")
	errInvalidMethod   = errors.New("method_invalid")
)

// Server implements ServiceInputV1 and ServiceInputV2
type Server struct {
	context         pipeline.Context
	collector       pipeline.Collector
	piplineContext  pipeline.PipelineContext
	serverGPRC      *grpc.Server
	serverHTTP      *http.Server
	grpcListener    net.Listener
	httpListener    net.Listener
	logsReceiver    plogotlp.GRPCServer
	tracesReceiver  ptraceotlp.GRPCServer // currently traces are not supported when using the v1 pipeline
	metricsReceiver pmetricotlp.GRPCServer
	wg              sync.WaitGroup

	Protocals Protocals
}

// Init ...
func (s *Server) Init(context pipeline.Context) (int, error) {
	s.context = context
	logger.Info(s.context.GetRuntimeContext(), "otlp server init", "initializing")

	if s.Protocals.GRPC != nil {
		if s.Protocals.GRPC.Endpoint == "" {
			s.Protocals.GRPC.Endpoint = defaultGRPCEndpoint
		}
	}

	if s.Protocals.HTTP != nil {
		if s.Protocals.HTTP.Endpoint == "" {
			s.Protocals.HTTP.Endpoint = defaultGRPCEndpoint
		}
		if s.Protocals.HTTP.ReadTimeoutSec == 0 {
			s.Protocals.HTTP.ReadTimeoutSec = 10
		}

		if s.Protocals.HTTP.ShutdownTimeoutSec == 0 {
			s.Protocals.HTTP.ShutdownTimeoutSec = 5
		}

		if s.Protocals.HTTP.MaxRequestBodySizeMiB == 0 {
			s.Protocals.HTTP.MaxRequestBodySizeMiB = 64
		}

	}

	logger.Info(s.context.GetRuntimeContext(), "otlp server init", "initialized", "gRPC settings", s.Protocals.GRPC, "HTTP setting", s.Protocals.HTTP)
	return 0, nil
}

// Description ...
func (s *Server) Description() string {
	return "Open-Telemetry service input plugin for logtail"
}

// Start ...
func (s *Server) Start(c pipeline.Collector) error {
	s.collector = c
	s.tracesReceiver = newTracesReceiverV1(c)
	s.metricsReceiver = newMetricsReceiverV1(c)
	s.logsReceiver = newLogsReceiverV1(c)
	return s.start()
}

// StartService(PipelineContext) error
func (s *Server) StartService(ctx pipeline.PipelineContext) error {
	s.piplineContext = ctx
	s.tracesReceiver = newTracesReceiver(ctx)
	s.metricsReceiver = newMetricsReceiver(ctx)
	s.logsReceiver = newLogsReceiver(ctx)
	return s.start()
}

func (s *Server) start() error {
	if s.Protocals.GRPC != nil {
		ops, err := s.Protocals.GRPC.GetServerOption()
		if err != nil {
			logger.Warningf(s.context.GetRuntimeContext(), "SERVICE_OTLP_INVALID_GRPC_SERVER_CONFIG", "inavlid grpc server config: %v, err: %v", s.Protocals.GRPC, err)
		}
		grpcServer := grpc.NewServer(
			ops...,
		)
		s.serverGPRC = grpcServer
		listener, err := getNetListener(s.Protocals.GRPC.Endpoint)
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
			logger.Info(s.context.GetRuntimeContext(), "otlp grpc server start", s.Protocals.GRPC.Endpoint)
			_ = s.serverGPRC.Serve(listener)
			s.serverGPRC.GracefulStop()
			logger.Info(s.context.GetRuntimeContext(), "otlp grpc server shutdown", s.Protocals.GRPC.Endpoint)
			s.wg.Done()
		}()
	}

	if s.Protocals.HTTP != nil {
		httpMux := http.NewServeMux()
		maxBodySize := int64(s.Protocals.HTTP.MaxRequestBodySizeMiB) * 1024 * 1024

		s.registerHTTPLogsComsumer(httpMux, &opentelemetry.Decoder{Format: common.ProtocolOTLPLogV1}, maxBodySize, "/v1/logs")
		s.registerHTTPMetricsComsumer(httpMux, &opentelemetry.Decoder{Format: common.ProtocolOTLPMetricV1}, maxBodySize, "/v1/metrics")
		s.registerHTTPTracesComsumer(httpMux, &opentelemetry.Decoder{Format: common.ProtocolOTLPTraceV1}, maxBodySize, "/v1/traces")
		logger.Info(s.context.GetRuntimeContext(), "otlp http receiver for logs/metrics/traces", "initialized")

		httpServer := &http.Server{
			Addr:        s.Protocals.HTTP.Endpoint,
			Handler:     httpMux,
			ReadTimeout: time.Duration(s.Protocals.HTTP.ReadTimeoutSec) * time.Second,
		}

		s.serverHTTP = httpServer
		listener, err := getNetListener(s.Protocals.HTTP.Endpoint)
		if err != nil {
			return err
		}
		s.httpListener = listener
		logger.Info(s.context.GetRuntimeContext(), "otlp http server init", "initialized")

		s.wg.Add(1)
		go func() {
			logger.Info(s.context.GetRuntimeContext(), "otlp http server start", s.Protocals.HTTP.Endpoint)
			_ = s.serverHTTP.Serve(s.httpListener)
			ctx, cancel := context.WithTimeout(context.Background(), time.Duration(s.Protocals.HTTP.ShutdownTimeoutSec)*time.Second)
			defer cancel()
			_ = s.serverHTTP.Shutdown(ctx)
			logger.Info(s.context.GetRuntimeContext(), "otlp http server shutdown", s.Protocals.HTTP.Endpoint)
			s.wg.Done()
		}()
	}
	return nil
}

// Stop stops the services and closes any necessary channels and connections
func (s *Server) Stop() error {
	if s.grpcListener != nil {
		_ = s.grpcListener.Close()
		logger.Info(s.context.GetRuntimeContext(), "otlp grpc server stop", s.Protocals.GRPC.Endpoint)
	}

	if s.httpListener != nil {
		_ = s.httpListener.Close()
		logger.Info(s.context.GetRuntimeContext(), "otlp http server stop", s.Protocals.HTTP.Endpoint)
	}
	s.wg.Wait()
	return nil
}

func (s *Server) registerHTTPLogsComsumer(serveMux *http.ServeMux, decoder extensions.Decoder, maxBodySize int64, routing string) {
	serveMux.HandleFunc(routing, func(w http.ResponseWriter, r *http.Request) {
		data, err := handleInvalidRequest(w, r, maxBodySize, decoder)
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
}

func (s *Server) registerHTTPMetricsComsumer(serveMux *http.ServeMux, decoder extensions.Decoder, maxBodySize int64, routing string) {
	serveMux.HandleFunc(routing, func(w http.ResponseWriter, r *http.Request) {
		data, err := handleInvalidRequest(w, r, maxBodySize, decoder)
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
}

func (s *Server) registerHTTPTracesComsumer(serveMux *http.ServeMux, decoder extensions.Decoder, maxBodySize int64, routing string) {
	serveMux.HandleFunc(routing, func(w http.ResponseWriter, r *http.Request) {
		data, err := handleInvalidRequest(w, r, maxBodySize, decoder)
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

func handleInvalidRequest(w http.ResponseWriter, r *http.Request, maxBodySize int64, decoder extensions.Decoder) (data []byte, err error) {
	if r.Method != http.MethodPost {
		handleUnmatchedMethod(w)
		err = errInvalidMethod
		return
	}

	if r.ContentLength > maxBodySize {
		httpserver.TooLarge(w)
		err = errTooLargeRequest
		return
	}

	var statusCode int
	data, statusCode, err = decoder.ParseRequest(w, r, maxBodySize)

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

	return data, err
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
	GRPC *helper.GRPCServerSettings
	HTTP *HTTPServerSettings
}

type HTTPServerSettings struct {
	Endpoint              string
	MaxRequestBodySizeMiB int
	ReadTimeoutSec        int
	ShutdownTimeoutSec    int
}

func init() {
	pipeline.ServiceInputs["service_otlp"] = func() pipeline.ServiceInput {
		return &Server{}
	}
}
