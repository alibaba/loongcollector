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

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/helper/decoder"
	"github.com/alibaba/ilogtail/pkg/logger"
)

// ServiceHTTP ...
type ServiceHTTP struct {
	context   ilogtail.Context
	collector ilogtail.Collector
	decoder   decoder.Decoder
	server    *http.Server
	listener  net.Listener
	wg        sync.WaitGroup

	Format             string
	Address            string
	ReadTimeoutSec     int
	ShutdownTimeoutSec int
	MaxBodySize        int64
	UnlinkUnixSock     bool
}

// Init ...
func (s *ServiceHTTP) Init(context ilogtail.Context) (int, error) {
	s.context = context
	var err error
	if s.decoder, err = decoder.GetDecoder(s.Format); err != nil {
		return 0, err
	}

	if s.Format == "otlp_logv1" {
		s.Address += "/v1/logs"
	}

	return 0, nil
}

// Description ...
func (s *ServiceHTTP) Description() string {
	return "HTTP service input plugin for logtail"
}

// Collect takes in an accumulator and adds the metrics that the Input
// gathers. This is called every "interval"
func (s *ServiceHTTP) Collect(ilogtail.Collector) error {
	return nil
}

func (s *ServiceHTTP) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	if r.ContentLength > s.MaxBodySize {
		tooLarge(w)
		return
	}

	data, statusCode, err := s.decoder.ParseRequest(w, r, s.MaxBodySize)

	switch statusCode {
	case http.StatusBadRequest:
		badRequest(w)
	case http.StatusRequestEntityTooLarge:
		tooLarge(w)
	case http.StatusInternalServerError:
		internalServerError(w)
	case http.StatusMethodNotAllowed:
		methodNotAllowed(w)

	}
	if err != nil {
		logger.Warning(s.context.GetRuntimeContext(), "READ_BODY_FAIL_ALARM", "read body failed", err, "request", r.URL.String())
		return
	}
	logs, err := s.decoder.Decode(data, r)
	if err != nil {
		logger.Warning(s.context.GetRuntimeContext(), "DECODE_BODY_FAIL_ALARM", "decode body failed", err, "request", r.URL.String())
		badRequest(w)
		return
	}
	for _, log := range logs {
		s.collector.AddRawLog(log)
	}
	if s.Format == "sls" {
		w.Header().Set("x-log-requestid", "1234567890abcde")
		w.WriteHeader(http.StatusOK)
	} else {
		w.WriteHeader(http.StatusNoContent)
	}

}

func tooLarge(res http.ResponseWriter) {
	res.Header().Set("Content-Type", "application/json")
	res.WriteHeader(http.StatusRequestEntityTooLarge)
	_, _ = res.Write([]byte(`{"error":"http: request body too large"}`))
}

func methodNotAllowed(res http.ResponseWriter) {
	res.Header().Set("Content-Type", "application/json")
	res.WriteHeader(http.StatusMethodNotAllowed)
	_, _ = res.Write([]byte(`{"error":"http: method not allowed"}`))
}

func internalServerError(res http.ResponseWriter) {
	res.Header().Set("Content-Type", "application/json")
	res.WriteHeader(http.StatusInternalServerError)
}

func badRequest(res http.ResponseWriter) {
	res.Header().Set("Content-Type", "application/json")
	res.WriteHeader(http.StatusBadRequest)
	_, _ = res.Write([]byte(`{"error":"http: bad request"}`))
}

// Start starts the ServiceInput's service, whatever that may be
func (s *ServiceHTTP) Start(c ilogtail.Collector) error {
	s.collector = c
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
	ilogtail.ServiceInputs["service_http_server"] = func() ilogtail.ServiceInput {
		return &ServiceHTTP{
			ReadTimeoutSec:     10,
			ShutdownTimeoutSec: 5,
			MaxBodySize:        64 * 1024 * 1024,
			UnlinkUnixSock:     true,
		}
	}
}
