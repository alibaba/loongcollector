// Copyright 2022 iLogtail Authors
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

package helper

import (
	"crypto/tls"
	"fmt"
	"strings"
	"time"

	tls_helper "github.com/influxdata/telegraf/plugins/common/tls"
	"google.golang.org/genproto/googleapis/rpc/errdetails"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/status"
)

var supportedCompressionType = map[string]interface{}{"gzip": nil, "snappy": nil, "zstd": nil}

type GrpcClientConfig struct {
	Endpoint string `json:"Endpoint"`

	// The compression key for supported compression types within collector.
	Compression string `json:"Compression"`

	// The headers associated with gRPC requests.
	Headers map[string]string `json:"Headers"`

	// Sets the balancer in grpclb_policy to discover the servers. Default is pick_first.
	// https://github.com/grpc/grpc-go/blob/master/examples/features/load_balancing/README.md
	BalancerName string `json:"BalancerName"`

	// WaitForReady parameter configures client to wait for ready state before sending data.
	// (https://github.com/grpc/grpc/blob/master/doc/wait-for-ready.md)
	WaitForReady bool `json:"WaitForReady"`

	// ReadBufferSize for gRPC client. See grpchelper.WithReadBufferSize.
	// (https://godoc.org/google.golang.org/grpc#WithReadBufferSize).
	ReadBufferSize int `json:"ReadBufferSize"`

	// WriteBufferSize for gRPC gRPC. See grpchelper.WithWriteBufferSize.
	// (https://godoc.org/google.golang.org/grpc#WithWriteBufferSize).
	WriteBufferSize int `json:"WriteBufferSize"`

	// Send retry setting
	Retry RetryConfig `json:"Retry"`

	Timeout int `json:"Timeout"`
}

type RetryConfig struct {
	Enable       bool
	MaxCount     int           `json:"MaxCount"`
	DefaultDelay time.Duration `json:"DefaultDelay"`
}

// GetDialOptions maps GrpcClientConfig to a slice of dial options for gRPC.
func (cfg *GrpcClientConfig) GetDialOptions() ([]grpc.DialOption, error) {
	var opts []grpc.DialOption
	if cfg.Compression != "" && cfg.Compression != "none" {
		if _, ok := supportedCompressionType[cfg.Compression]; !ok {
			return nil, fmt.Errorf("unsupported compression type %q", cfg.Compression)
		}
		opts = append(opts, grpc.WithDefaultCallOptions(grpc.UseCompressor(cfg.Compression)))
	}

	cred := insecure.NewCredentials()
	if strings.HasPrefix(cfg.Endpoint, "https://") {
		/* #nosec G402 - it is a false positive since tls.VersionTLS13 is the latest version */
		cred = credentials.NewTLS(&tls.Config{MinVersion: tls.VersionTLS13})
	}
	opts = append(opts, grpc.WithTransportCredentials(cred))

	if cfg.ReadBufferSize > 0 {
		opts = append(opts, grpc.WithReadBufferSize(cfg.ReadBufferSize))
	}

	if cfg.WriteBufferSize > 0 {
		opts = append(opts, grpc.WithWriteBufferSize(cfg.WriteBufferSize))
	}

	opts = append(opts, grpc.WithTimeout(cfg.GetTimeout()))
	opts = append(opts, grpc.WithDefaultCallOptions(grpc.WaitForReady(cfg.WaitForReady)))

	if cfg.WaitForReady {
		opts = append(opts, grpc.WithBlock())
	}

	return opts, nil
}

func (cfg *GrpcClientConfig) GetEndpoint() string {
	if strings.HasPrefix(cfg.Endpoint, "http://") {
		return strings.TrimPrefix(cfg.Endpoint, "http://")
	}
	if strings.HasPrefix(cfg.Endpoint, "https://") {
		return strings.TrimPrefix(cfg.Endpoint, "https://")
	}
	return cfg.Endpoint
}

func (cfg *GrpcClientConfig) GetTimeout() time.Duration {
	if cfg.Timeout <= 0 {
		return 5000 * time.Millisecond
	}
	return time.Duration(cfg.Timeout) * time.Millisecond
}

// RetryInfo Handle retry for grpc. Refer to https://github.com/open-telemetry/opentelemetry-collector/blob/main/exporter/otlpexporter/otlp.go#L121
type RetryInfo struct {
	delay time.Duration
	err   error
}

func (r *RetryInfo) Error() error {
	return r.err
}

func (r *RetryInfo) ShouldDelay(delay time.Duration) time.Duration {
	if r.delay != 0 {
		return r.delay
	}
	return delay
}

func GetRetryInfo(err error) *RetryInfo {
	if err == nil {
		// Request is successful, we are done.
		return nil
	}
	// We have an error, check gRPC status code.

	st := status.Convert(err)
	if st.Code() == codes.OK {
		// Not really an error, still success.
		return nil
	}
	retryInfo := getRetryInfo(st)

	if !shouldRetry(st.Code(), retryInfo) {
		// It is not a retryable error, we should not retry.
		return nil
	}
	throttle := getThrottleDuration(retryInfo)
	return &RetryInfo{delay: throttle, err: err}
}

func shouldRetry(code codes.Code, retryInfo *errdetails.RetryInfo) bool {
	switch code {
	case codes.Canceled,
		codes.DeadlineExceeded,
		codes.Aborted,
		codes.OutOfRange,
		codes.Unavailable,
		codes.DataLoss:
		// These are retryable errors.
		return true
	case codes.ResourceExhausted:
		// Retry only if RetryInfo was supplied by the server.
		// This indicates that the server can still recover from resource exhaustion.
		return retryInfo != nil
	}
	// Don't retry on any other code.
	return false
}

func getRetryInfo(status *status.Status) *errdetails.RetryInfo {
	for _, detail := range status.Details() {
		if t, ok := detail.(*errdetails.RetryInfo); ok {
			return t
		}
	}
	return nil
}

func getThrottleDuration(t *errdetails.RetryInfo) time.Duration {
	if t == nil || t.RetryDelay == nil {
		return 0
	}
	if t.RetryDelay.Seconds > 0 || t.RetryDelay.Nanos > 0 {
		return time.Duration(t.RetryDelay.Seconds)*time.Second + time.Duration(t.RetryDelay.Nanos)*time.Nanosecond
	}
	return 0
}

type GRPCServerSettings struct {
	Endpoint string `json:"Endpoint"`

	MaxRecvMsgSizeMiB int `json:"MaxRecvMsgSizeMiB"`

	MaxConcurrentStreams int `json:"MaxConcurrentStreams"`

	ReadBufferSize int `json:"ReadBufferSize"`

	WriteBufferSize int `json:"WriteBufferSize"`

	Compression string `json:"Compression"`

	Decompression string `json:"Decompression"`

	TLSConfig tls_helper.ServerConfig `json:"TLSConfig"`
}

func (cfg *GRPCServerSettings) GetServerOption() ([]grpc.ServerOption, error) {
	var opts []grpc.ServerOption
	var err error
	if cfg != nil {
		if cfg.MaxRecvMsgSizeMiB > 0 {
			opts = append(opts, grpc.MaxRecvMsgSize(cfg.MaxRecvMsgSizeMiB*1024*1024))
		}
		if cfg.MaxConcurrentStreams > 0 {
			opts = append(opts, grpc.MaxConcurrentStreams(uint32(cfg.MaxConcurrentStreams)))
		}

		if cfg.ReadBufferSize > 0 {
			opts = append(opts, grpc.ReadBufferSize(cfg.ReadBufferSize))
		}

		if cfg.WriteBufferSize > 0 {
			opts = append(opts, grpc.WriteBufferSize(cfg.WriteBufferSize))
		}

		var tlsConfig *tls.Config
		tlsConfig, err = cfg.TLSConfig.TLSConfig()
		if err == nil && tlsConfig != nil {
			opts = append(opts, grpc.Creds(credentials.NewTLS(tlsConfig)))
		}

		dc := strings.ToLower(cfg.Decompression)
		if dc != "" && dc != "none" {
			dc := strings.ToLower(cfg.Decompression)
			switch dc {
			case "gzip":
				opts = append(opts, grpc.RPCDecompressor(grpc.NewGZIPDecompressor()))
			default:
				err = fmt.Errorf("invalid decompression: %s", cfg.Decompression)
			}
		}

		cp := strings.ToLower(cfg.Compression)
		if cp != "" && cp != "none" {
			switch cp {
			case "gzip":
				opts = append(opts, grpc.RPCCompressor(grpc.NewGZIPCompressor()))
			default:
				err = fmt.Errorf("invalid compression: %s", cfg.Compression)
			}
		}

	}

	return opts, err
}
