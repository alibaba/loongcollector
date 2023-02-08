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

package httpserver

import (
	"context"
	"net"
	"net/http"
	"net/url"
	"strings"
	"sync"
	"syscall"
	"time"

	"github.com/alibaba/ilogtail/helper/decoder"
	"github.com/alibaba/ilogtail/helper/decoder/common"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

const (
	v1 = iota
	v2
)

// ServiceHTTP ...
type ServiceHTTP struct {
	context     pipeline.Context
	collector   pipeline.Collector
	decoder     decoder.Decoder
	server      *http.Server
	listener    net.Listener
	wg          sync.WaitGroup
	collectorV2 pipeline.PipelineCollector
	version     int8
	paramCount  int

	Format             string
	Address            string
	ReadTimeoutSec     int
	ShutdownTimeoutSec int
	MaxBodySize        int64
	UnlinkUnixSock     bool
	FieldsExtend       bool
	DisableUncompress  bool

	// params below works only for version v2
	QueryParams       []string
	HeaderParams      []string
	QueryParamPrefix  string
	HeaderParamPrefix string
}

// Init ...
func (s *ServiceHTTP) Init(context pipeline.Context) (int, error) {
	s.context = context
	var err error
	if s.decoder, err = decoder.GetDecoderWithOptions(s.Format, decoder.Option{FieldsExtend: s.FieldsExtend, DisableUncompress: s.DisableUncompress}); err != nil {
		return 0, err
	}

	switch s.Format {
	case common.ProtocolOTLPLogV1:
		s.Address += "/v1/logs"
	case common.ProtocolOTLPMetricV1:
		s.Address += "/v1/metrics"
	case common.ProtocolOTLPTraceV1:
		s.Address += "/v1/traces"
	}

	s.paramCount = len(s.QueryParams) + len(s.HeaderParams)

	return 0, nil
}

// Description ...
func (s *ServiceHTTP) Description() string {
	return "HTTP service input plugin for logtail"
}

// Collect takes in an accumulator and adds the metrics that the Input
// gathers. This is called every "interval"
func (s *ServiceHTTP) Collect(pipeline.Collector) error {
	return nil
}

func (s *ServiceHTTP) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	if r.ContentLength > s.MaxBodySize {
		TooLarge(w)
		return
	}
	data, statusCode, err := s.decoder.ParseRequest(w, r, s.MaxBodySize)
	logger.Debugf(s.context.GetRuntimeContext(), "request [method] %v; [header] %v; [url] %v; [body] %v", r.Method, r.Header, r.URL, string(data))

	switch statusCode {
	case http.StatusBadRequest:
		BadRequest(w)
	case http.StatusRequestEntityTooLarge:
		TooLarge(w)
	case http.StatusInternalServerError:
		InternalServerError(w)
	case http.StatusMethodNotAllowed:
		MethodNotAllowed(w)

	}
	if err != nil {
		logger.Warning(s.context.GetRuntimeContext(), "READ_BODY_FAIL_ALARM", "read body failed", err, "request", r.URL.String())
		return
	}

	switch s.version {
	case v1:
		logs, err := s.decoder.Decode(data, r)
		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), "DECODE_BODY_FAIL_ALARM", "decode body failed", err, "request", r.URL.String())
			BadRequest(w)
			return
		}
		for _, log := range logs {
			s.collector.AddRawLog(log)
		}
	case v2:
		groups, err := s.decoder.DecodeV2(data, r)
		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), "DECODE_BODY_FAIL_ALARM", "decode body failed", err, "request", r.URL.String())
			BadRequest(w)
			return
		}
		if reqParams := s.extractRequestParams(r); len(reqParams) != 0 {
			for _, g := range groups {
				g.Group.Metadata.Merge(models.NewMetadataWithMap(reqParams))
			}
		}
		s.collectorV2.CollectList(groups...)
	}

	if s.Format == "sls" {
		w.Header().Set("x-log-requestid", "1234567890abcde")
		w.WriteHeader(http.StatusOK)
	} else {
		w.WriteHeader(http.StatusNoContent)
	}

}

func TooLarge(res http.ResponseWriter) {
	res.Header().Set("Content-Type", "application/json")
	res.WriteHeader(http.StatusRequestEntityTooLarge)
	_, _ = res.Write([]byte(`{"error":"http: request body too large"}`))
}

func MethodNotAllowed(res http.ResponseWriter) {
	res.Header().Set("Content-Type", "application/json")
	res.WriteHeader(http.StatusMethodNotAllowed)
	_, _ = res.Write([]byte(`{"error":"http: method not allowed"}`))
}

func InternalServerError(res http.ResponseWriter) {
	res.Header().Set("Content-Type", "application/json")
	res.WriteHeader(http.StatusInternalServerError)
}

func BadRequest(res http.ResponseWriter) {
	res.Header().Set("Content-Type", "application/json")
	res.WriteHeader(http.StatusBadRequest)
	_, _ = res.Write([]byte(`{"error":"http: bad request"}`))
}

// Start starts the ServiceInput's service, whatever that may be
func (s *ServiceHTTP) Start(c pipeline.Collector) error {
	s.collector = c
	s.version = v1
	return s.start()
}

// StartService start the ServiceInput's service by plugin runner v2
func (s *ServiceHTTP) StartService(context pipeline.PipelineContext) error {
	s.collectorV2 = context.Collector()
	s.version = v2

	return s.start()
}

func (s *ServiceHTTP) start() error {
	s.wg.Add(1)

	server := &http.Server{
		Addr:        s.Address,
		Handler:     s,
		ReadTimeout: time.Duration(s.ReadTimeoutSec) * time.Second,
	}
	var listener net.Listener
	var err error
	switch {
	case strings.HasPrefix(s.Address, "unix"):
		sockPath := strings.Replace(s.Address, "unix://", "", 1)
		if s.UnlinkUnixSock {
			_ = syscall.Unlink(sockPath)
		}
		listener, err = net.Listen("unix", sockPath)
	case strings.HasPrefix(s.Address, "http") ||
		strings.HasPrefix(s.Address, "https") ||
		strings.HasPrefix(s.Address, "tcp"):
		configURL, errAddr := url.Parse(s.Address)
		if errAddr != nil {
			return errAddr
		}
		listener, err = net.Listen("tcp", configURL.Host)
	default:
		listener, err = net.Listen("tcp", s.Address)
	}
	if err != nil {
		return err
	}
	s.listener = listener
	s.server = server
	go func() {
		logger.Info(s.context.GetRuntimeContext(), "http server start", s.Address)
		_ = server.Serve(listener)
		ctx, cancel := context.WithTimeout(context.Background(), time.Duration(s.ShutdownTimeoutSec)*time.Second)
		defer cancel()
		_ = server.Shutdown(ctx)
		logger.Info(s.context.GetRuntimeContext(), "http server shutdown", s.Address)
		s.wg.Done()
	}()
	return nil
}

func (s *ServiceHTTP) extractRequestParams(req *http.Request) map[string]string {
	keyValues := make(map[string]string, s.paramCount)
	for _, key := range s.QueryParams {
		value := req.FormValue(key)
		if len(value) == 0 {
			continue
		}
		if len(s.QueryParamPrefix) > 0 {
			builder := strings.Builder{}
			builder.WriteString(s.QueryParamPrefix)
			builder.WriteString(key)
			key = builder.String()
		}
		keyValues[key] = value
	}
	for _, key := range s.HeaderParams {
		value := req.Header.Get(key)
		if len(value) == 0 {
			continue
		}
		if len(s.HeaderParamPrefix) > 0 {
			builder := strings.Builder{}
			builder.WriteString(s.HeaderParamPrefix)
			builder.WriteString(key)
			key = builder.String()
		}
		keyValues[key] = value
	}
	return keyValues
}

// Stop stops the services and closes any necessary channels and connections
func (s *ServiceHTTP) Stop() error {
	if s.listener != nil {
		_ = s.listener.Close()
		logger.Info(s.context.GetRuntimeContext(), "http server stop", s.Address)
		s.wg.Wait()
	}
	return nil
}

func init() {
	pipeline.ServiceInputs["service_http_server"] = func() pipeline.ServiceInput {
		return &ServiceHTTP{
			ReadTimeoutSec:     10,
			ShutdownTimeoutSec: 5,
			MaxBodySize:        64 * 1024 * 1024,
			UnlinkUnixSock:     true,
		}
	}
}
