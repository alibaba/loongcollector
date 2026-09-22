// Copyright 2026 iLogtail Authors
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
	"bytes"
	"compress/gzip"
	"crypto/rand"
	"fmt"
	"io"
	"math/big"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"time"

	"go.opentelemetry.io/collector/pdata/plog/plogotlp"
	"go.opentelemetry.io/collector/pdata/pmetric/pmetricotlp"
	"go.opentelemetry.io/collector/pdata/ptrace/ptraceotlp"
	spb "google.golang.org/genproto/googleapis/rpc/status"
	"google.golang.org/protobuf/proto"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/pipeline/extensions"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

const (
	otlpHTTPEncodingProto = "proto"
	otlpHTTPEncodingJSON  = "json"

	otlpHTTPCompressionGzip = "gzip"
	otlpHTTPCompressionNone = "none"

	otlpHTTPLogsPath    = "/v1/logs"
	otlpHTTPMetricsPath = "/v1/metrics"
	otlpHTTPTracesPath  = "/v1/traces"

	// Signal names, only used in log messages.
	otlpHTTPSignalLogs    = "logs"
	otlpHTTPSignalMetrics = "metrics"
	otlpHTTPSignalTraces  = "traces"

	otlpHTTPProtobufContentType = "application/x-protobuf"
	otlpHTTPJSONContentType     = "application/json"

	// Upper bound of response body bytes read for error message / partial success decoding.
	// Aligned with the OpenTelemetry Collector otlphttpexporter.
	otlpHTTPMaxResponseReadBytes = 64 * 1024

	otlpHTTPDefaultTimeout      = 30 * time.Second
	otlpHTTPDefaultMaxRetry     = 3
	otlpHTTPDefaultInitialDelay = time.Second
	otlpHTTPDefaultMaxDelay     = 30 * time.Second
)

var (
	_ pipeline.FlusherV1 = (*FlusherOTLPHTTP)(nil)
	_ pipeline.FlusherV2 = (*FlusherOTLPHTTP)(nil)
)

// httpDoer is the minimal http.Client surface used by the flusher, so tests can inject a stub.
type httpDoer interface {
	Do(req *http.Request) (*http.Response, error)
}

// otlpHTTPSignalConfig overrides the destination of a single OTLP signal.
type otlpHTTPSignalConfig struct {
	// Endpoint is a complete URL. When set it replaces the base Endpoint entirely,
	// i.e. no signal path such as /v1/logs is appended to it.
	Endpoint string `json:"Endpoint"`
	// Headers are merged on top of the flusher level Headers, this signal wins on conflict.
	Headers map[string]string `json:"Headers"`
}

type otlpHTTPRetryConfig struct {
	Enable        bool          `json:"Enable"`
	MaxRetryTimes int           `json:"MaxRetryTimes"`
	InitialDelay  time.Duration `json:"InitialDelay"`
	MaxDelay      time.Duration `json:"MaxDelay"`
}

// FlusherOTLPHTTP exports data over the OTLP/HTTP protocol, i.e. an HTTP POST carrying a
// protobuf or JSON encoded OTLP ExportRequest. Protocol behavior follows the OpenTelemetry
// Collector otlphttpexporter.
//
// All three signals are supported through the v2 Export entry point. The v1 Flush entry point
// only carries logs, since protocol.LogGroup has no notion of metrics or traces.
type FlusherOTLPHTTP struct {
	Version Version `json:"Version"`
	// Endpoint is the base URL, e.g. http://collector:4318. The signal path (/v1/logs,
	// /v1/metrics, /v1/traces) is appended to it, so a single Endpoint enables every signal.
	// It may be omitted when every used signal sets its own Endpoint.
	Endpoint string `json:"Endpoint"`
	// Logs, Metrics and Traces optionally override the URL and headers of a single signal.
	// A signal with neither a base Endpoint nor its own Endpoint is disabled, its data is dropped.
	Logs    *otlpHTTPSignalConfig `json:"Logs"`
	Metrics *otlpHTTPSignalConfig `json:"Metrics"`
	Traces  *otlpHTTPSignalConfig `json:"Traces"`

	// Encoding is the OTLP payload encoding, either proto (default) or json.
	Encoding string `json:"Encoding"`
	// Compression is the request body compression, either gzip (default) or none.
	Compression string `json:"Compression"`
	// Headers are appended to every request.
	Headers map[string]string `json:"Headers"`
	// Timeout of a single request, default is 30s.
	Timeout time.Duration       `json:"Timeout"`
	Retry   otlpHTTPRetryConfig `json:"Retry"`

	// http.Transport tuning, semantics follow flusher_http.
	MaxConnsPerHost     int           `json:"MaxConnsPerHost"`
	MaxIdleConnsPerHost int           `json:"MaxIdleConnsPerHost"`
	IdleConnTimeout     time.Duration `json:"IdleConnTimeout"`
	WriteBufferSize     int           `json:"WriteBufferSize"`

	// Authenticator is the name and options of the extensions.ClientAuthenticator extension to use.
	Authenticator *extensions.ExtensionConfig `json:"Authenticator"`
	// RequestInterceptors is a chain of extensions.RequestInterceptor extensions to use.
	RequestInterceptors []extensions.ExtensionConfig `json:"RequestInterceptors"`

	converter    *converter.Converter
	context      pipeline.Context
	client       httpDoer
	logClient    *otlpHTTPSignalClient
	metricClient *otlpHTTPSignalClient
	traceClient  *otlpHTTPSignalClient

	// stopCh is closed by Stop, it aborts a retry backoff that is currently waiting so that
	// shutdown is not held hostage by a server suggested Retry-After.
	stopCh   chan struct{}
	stopOnce sync.Once
}

// otlpHTTPSignalClient is the resolved destination of one signal, immutable after Init.
type otlpHTTPSignalClient struct {
	// signal is logs, metrics or traces, it is only used in log messages.
	signal  string
	url     string
	headers map[string]string
	// partialSuccess decodes an accepted response body into the number of rejected records and
	// the server message. The rejected counter is signal specific: log records, data points or spans.
	partialSuccess func(body []byte, jsonEncoding bool) (rejected int64, message string, err error)
}

// otlpRequest is the marshaling surface shared by the logs, metrics and traces export requests.
type otlpRequest interface {
	MarshalProto() ([]byte, error)
	MarshalJSON() ([]byte, error)
}

// otlpUnmarshaler is the unmarshaling surface shared by the OTLP export request and response
// wrappers, it lets the encoding be picked once for every signal.
type otlpUnmarshaler interface {
	UnmarshalProto(data []byte) error
	UnmarshalJSON(data []byte) error
}

func NewFlusherOTLPHTTP() *FlusherOTLPHTTP {
	return &FlusherOTLPHTTP{
		Version:     v1,
		Encoding:    otlpHTTPEncodingProto,
		Compression: otlpHTTPCompressionGzip,
		Timeout:     otlpHTTPDefaultTimeout,
		Retry: otlpHTTPRetryConfig{
			Enable:        true,
			MaxRetryTimes: otlpHTTPDefaultMaxRetry,
			InitialDelay:  otlpHTTPDefaultInitialDelay,
			MaxDelay:      otlpHTTPDefaultMaxDelay,
		},
		stopCh: make(chan struct{}),
	}
}

func (f *FlusherOTLPHTTP) Description() string {
	return "Open Telemetry HTTP flusher for ilogtail"
}

func (f *FlusherOTLPHTTP) Init(ctx pipeline.Context) error {
	f.context = ctx
	logger.Info(f.context.GetRuntimeContext(), "otlp http flusher init", "initializing")

	if f.stopCh == nil {
		// The flusher may have been built by the config loader rather than NewFlusherOTLPHTTP.
		f.stopCh = make(chan struct{})
	}

	f.fillDefaults()

	if err := f.validate(); err != nil {
		logger.Warning(f.context.GetRuntimeContext(), selfmonitor.FlusherInitAlarm, "otlp http flusher check config fail, error", err)
		return err
	}

	convert, err := f.getConverter()
	if err != nil {
		logger.Warning(f.context.GetRuntimeContext(), selfmonitor.FlusherInitAlarm, "init otlp http converter fail, error", err)
		return err
	}
	f.converter = convert

	if f.client, err = f.buildHTTPClient(); err != nil {
		logger.Warning(f.context.GetRuntimeContext(), selfmonitor.FlusherInitAlarm, "init otlp http client fail, error", err)
		return err
	}

	f.logClient = f.buildSignalClient(otlpHTTPSignalLogs, f.Logs, otlpHTTPLogsPath, decodeLogsPartialSuccess)
	f.metricClient = f.buildSignalClient(otlpHTTPSignalMetrics, f.Metrics, otlpHTTPMetricsPath, decodeMetricsPartialSuccess)
	f.traceClient = f.buildSignalClient(otlpHTTPSignalTraces, f.Traces, otlpHTTPTracesPath, decodeTracesPartialSuccess)

	if f.logClient == nil && f.metricClient == nil && f.traceClient == nil {
		err = fmt.Errorf("invalid_otlp_http_configs")
		logger.Warning(f.context.GetRuntimeContext(), selfmonitor.FlusherInitAlarm, "init otlp http flusher fail, error", "no endpoint configured")
		return err
	}

	logger.Info(f.context.GetRuntimeContext(), "otlp http flusher init", "initialized",
		"encoding", f.Encoding, "compression", f.Compression, "timeout", f.Timeout)
	return nil
}

func (f *FlusherOTLPHTTP) fillDefaults() {
	if f.Version == "" {
		f.Version = v1
	}
	if f.Encoding == "" {
		f.Encoding = otlpHTTPEncodingProto
	}
	if f.Compression == "" {
		f.Compression = otlpHTTPCompressionGzip
	}
	if f.Timeout <= 0 {
		f.Timeout = otlpHTTPDefaultTimeout
	}
	if f.Retry.MaxRetryTimes < 0 {
		f.Retry.MaxRetryTimes = 0
	}
	if f.Retry.InitialDelay <= 0 {
		f.Retry.InitialDelay = otlpHTTPDefaultInitialDelay
	}
	if f.Retry.MaxDelay <= 0 {
		f.Retry.MaxDelay = otlpHTTPDefaultMaxDelay
	}
	if f.Retry.MaxDelay < f.Retry.InitialDelay {
		f.Retry.MaxDelay = f.Retry.InitialDelay
	}
}

func (f *FlusherOTLPHTTP) validate() error {
	switch f.Encoding {
	case otlpHTTPEncodingProto, otlpHTTPEncodingJSON:
	default:
		return fmt.Errorf("unsupported otlp http encoding: %s, only proto and json are supported", f.Encoding)
	}

	switch f.Compression {
	case otlpHTTPCompressionNone, otlpHTTPCompressionGzip:
	default:
		return fmt.Errorf("unsupported otlp http compression: %s, only gzip and none are supported", f.Compression)
	}
	return nil
}

func (f *FlusherOTLPHTTP) getConverter() (*converter.Converter, error) {
	switch f.Version {
	case v1:
		return converter.NewConverter(converter.ProtocolOtlpV1, converter.EncodingNone, nil, nil, f.context.GetPipelineScopeConfig())
	default:
		return nil, fmt.Errorf("unsupported otlp log protocol version : %s", f.Version)
	}
}

// buildSignalClient resolves one signal into a client, or nil when the signal has no destination
// and is therefore disabled.
func (f *FlusherOTLPHTTP) buildSignalClient(signal string, signalCfg *otlpHTTPSignalConfig, signalPath string,
	partialSuccess func(body []byte, jsonEncoding bool) (int64, string, error)) *otlpHTTPSignalClient {
	url := f.buildSignalURL(signalCfg, signalPath)
	if url == "" {
		return nil
	}

	logger.Info(f.context.GetRuntimeContext(), "otlp http flusher endpoint", url, "signal", signal)
	return &otlpHTTPSignalClient{
		signal:         signal,
		url:            url,
		headers:        f.buildSignalHeaders(signalCfg),
		partialSuccess: partialSuccess,
	}
}

// buildSignalURL resolves the destination URL of a signal. A signal level Endpoint is used
// verbatim, otherwise the signal path is appended to the base Endpoint. An empty result means
// the signal is not configured. Behavior matches otlphttpexporter's composeSignalURL.
func (f *FlusherOTLPHTTP) buildSignalURL(signalCfg *otlpHTTPSignalConfig, signalPath string) string {
	if signalCfg != nil && signalCfg.Endpoint != "" {
		return signalCfg.Endpoint
	}
	if f.Endpoint != "" {
		return strings.TrimRight(f.Endpoint, "/") + signalPath
	}
	return ""
}

// buildSignalHeaders merges the flusher level headers with the signal level ones, the latter wins.
func (f *FlusherOTLPHTTP) buildSignalHeaders(signalCfg *otlpHTTPSignalConfig) map[string]string {
	headers := make(map[string]string, len(f.Headers)+1)
	for k, v := range f.Headers {
		headers[k] = v
	}
	if signalCfg != nil {
		for k, v := range signalCfg.Headers {
			headers[k] = v
		}
	}
	return headers
}

func (f *FlusherOTLPHTTP) buildHTTPClient() (httpDoer, error) {
	transport := http.DefaultTransport
	if dt, ok := transport.(*http.Transport); ok {
		dt = dt.Clone()
		if f.MaxConnsPerHost > dt.MaxConnsPerHost {
			dt.MaxConnsPerHost = f.MaxConnsPerHost
		}
		if f.MaxIdleConnsPerHost > dt.MaxIdleConnsPerHost {
			dt.MaxIdleConnsPerHost = f.MaxIdleConnsPerHost
		}
		if f.IdleConnTimeout > dt.IdleConnTimeout {
			dt.IdleConnTimeout = f.IdleConnTimeout
		}
		if f.WriteBufferSize > 0 {
			dt.WriteBufferSize = f.WriteBufferSize
		}
		transport = dt
	}

	transport, err := f.initRequestInterceptors(transport)
	if err != nil {
		return nil, err
	}

	if f.Authenticator != nil {
		var auth pipeline.Extension
		auth, err = f.context.GetExtension(f.Authenticator.Type, f.Authenticator.Options)
		if err != nil {
			return nil, fmt.Errorf("get authenticator extension failed: %w", err)
		}
		ca, ok := auth.(extensions.ClientAuthenticator)
		if !ok {
			return nil, fmt.Errorf("authenticator(%s) not implement interface extensions.ClientAuthenticator", f.Authenticator.Type)
		}
		if transport, err = ca.RoundTripper(transport); err != nil {
			return nil, fmt.Errorf("build authenticator round tripper failed: %w", err)
		}
	}

	return &http.Client{Timeout: f.Timeout, Transport: transport}, nil
}

func (f *FlusherOTLPHTTP) initRequestInterceptors(transport http.RoundTripper) (http.RoundTripper, error) {
	for i := len(f.RequestInterceptors) - 1; i >= 0; i-- {
		setting := f.RequestInterceptors[i]
		ext, err := f.context.GetExtension(setting.Type, setting.Options)
		if err != nil {
			return nil, fmt.Errorf("get request interceptor extension(%s) failed: %w", setting.Type, err)
		}
		interceptor, ok := ext.(extensions.RequestInterceptor)
		if !ok {
			return nil, fmt.Errorf("interceptor(%s) with type %T not implement interface extensions.RequestInterceptor", setting.Type, ext)
		}
		if transport, err = interceptor.RoundTripper(transport); err != nil {
			return nil, fmt.Errorf("build request interceptor(%s) round tripper failed: %w", setting.Type, err)
		}
	}
	return transport, nil
}

// SetHTTPDoer replaces the underlying HTTP client, it is only meant for tests.
func (f *FlusherOTLPHTTP) SetHTTPDoer(doer httpDoer) {
	f.client = doer
}

// IsReady is ready to flush
func (f *FlusherOTLPHTTP) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	ready := f.client != nil && (f.logClient != nil || f.metricClient != nil || f.traceClient != nil)
	if !ready {
		logger.Warning(f.context.GetRuntimeContext(), selfmonitor.FlusherReadyAlarm, "otlp http flusher is not ready", "no available endpoint")
	}
	return ready
}

func (f *FlusherOTLPHTTP) SetUrgent(flag bool) {
}

// Stop ...
func (f *FlusherOTLPHTTP) Stop() error {
	// Wake up a retry backoff that is currently waiting, so that shutdown is not blocked by it.
	f.stopOnce.Do(func() {
		if f.stopCh != nil {
			close(f.stopCh)
		}
	})
	if c, ok := f.client.(*http.Client); ok {
		c.CloseIdleConnections()
	}
	return nil
}

func (f *FlusherOTLPHTTP) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	if f.logClient == nil {
		// The logs signal is disabled, so there is nothing to convert the log groups into.
		logger.Warning(f.context.GetRuntimeContext(), selfmonitor.FlusherFlushAlarm, "otlp http flusher dropped data",
			"no endpoint configured for this signal", "signal", otlpHTTPSignalLogs)
		return nil
	}
	// v1 log groups only carry logs.
	req := otlpConvertLogGroupToRequest(f.converter, logGroupList)
	return f.exportSignal(f.logClient, req, otlpHTTPSignalLogs, req.Logs().LogRecordCount())
}

// Export data to destination, such as gRPC, console, file, etc.
// It is expected to return no error at most time because IsReady will be called
// before it to make sure there is space for next data.
func (f *FlusherOTLPHTTP) Export(groupEventsSlice []*models.PipelineGroupEvents, ctx pipeline.PipelineContext) error {
	logReq, metricReq, traceReq := otlpConvertPipelineEventsToRequests(f.context.GetRuntimeContext(), f.converter, groupEventsSlice)

	// One request per non empty signal, sent one after another. A batch normally carries a single
	// event type, so in practice this is a single request. The first failure is reported, the
	// remaining signals are still attempted so that one broken signal cannot block the others.
	err := f.exportSignal(f.logClient, logReq, otlpHTTPSignalLogs, logReq.Logs().LogRecordCount())
	if e := f.exportSignal(f.metricClient, metricReq, otlpHTTPSignalMetrics, metricReq.Metrics().DataPointCount()); e != nil && err == nil {
		err = e
	}
	if e := f.exportSignal(f.traceClient, traceReq, otlpHTTPSignalTraces, traceReq.Traces().SpanCount()); e != nil && err == nil {
		err = e
	}
	return err
}

// exportSignal sends one signal, unless it carries no data or has no configured destination.
func (f *FlusherOTLPHTTP) exportSignal(client *otlpHTTPSignalClient, req otlpRequest, signal string, count int) error {
	if count == 0 {
		// Nothing to send, an empty OTLP request would only waste a round trip.
		return nil
	}
	if client == nil {
		logger.Warning(f.context.GetRuntimeContext(), selfmonitor.FlusherFlushAlarm, "otlp http flusher dropped data",
			"no endpoint configured for this signal", "signal", signal, "count", count)
		return nil
	}

	data, contentType, err := f.marshalRequest(req)
	if err != nil {
		// A marshal failure is permanent, retrying cannot help.
		logger.Warning(f.context.GetRuntimeContext(), selfmonitor.FlusherFlushAlarm, "otlp http flusher marshal request fail, data dropped, error", err,
			"signal", client.signal)
		return err
	}

	if err = f.sendWithRetry(client, data, contentType); err != nil {
		logger.Warning(f.context.GetRuntimeContext(), selfmonitor.FlusherFlushAlarm, "send data to otlp http server fail, error", err,
			"signal", client.signal, "url", client.url)
	}
	return err
}

func (f *FlusherOTLPHTTP) marshalRequest(req otlpRequest) (data []byte, contentType string, err error) {
	if f.Encoding == otlpHTTPEncodingJSON {
		data, err = req.MarshalJSON()
		return data, otlpHTTPJSONContentType, err
	}
	data, err = req.MarshalProto()
	return data, otlpHTTPProtobufContentType, err
}

func (f *FlusherOTLPHTTP) compressData(data []byte) (body io.Reader, contentEncoding string, err error) {
	if f.Compression != otlpHTTPCompressionGzip {
		return bytes.NewReader(data), "", nil
	}

	var buf bytes.Buffer
	gw := gzip.NewWriter(&buf)
	if _, err = gw.Write(data); err != nil {
		return nil, "", err
	}
	if err = gw.Close(); err != nil {
		return nil, "", err
	}
	return &buf, otlpHTTPCompressionGzip, nil
}

func (f *FlusherOTLPHTTP) sendWithRetry(client *otlpHTTPSignalClient, data []byte, contentType string) error {
	var err error
	for attempt := 0; attempt <= f.Retry.MaxRetryTimes; attempt++ {
		retryable, retryAfter, e := f.sendOnce(client, data, contentType)
		if e == nil {
			return nil
		}
		err = e
		if !retryable || !f.Retry.Enable || attempt == f.Retry.MaxRetryTimes {
			break
		}
		if !f.waitBeforeRetry(f.nextRetryDelay(attempt, retryAfter)) {
			// Stopping, give up instead of holding the flush goroutine hostage.
			break
		}
	}
	return err
}

// waitBeforeRetry sleeps for delay, unless Stop is called first. It reports whether the wait
// completed, i.e. whether another attempt should be made.
func (f *FlusherOTLPHTTP) waitBeforeRetry(delay time.Duration) bool {
	timer := time.NewTimer(delay)
	defer timer.Stop()
	select {
	case <-timer.C:
		return true
	case <-f.stopCh:
		// A nil stopCh never fires, which is exactly the pre-Init behavior.
		return false
	}
}

// sendOnce performs a single OTLP/HTTP POST. It reports whether the failure is worth retrying
// and, for throttled responses, the server suggested delay.
func (f *FlusherOTLPHTTP) sendOnce(client *otlpHTTPSignalClient, data []byte,
	contentType string) (retryable bool, retryAfter time.Duration, err error) {
	body, contentEncoding, err := f.compressData(data)
	if err != nil {
		return false, 0, fmt.Errorf("compress otlp payload failed: %w", err)
	}

	req, err := http.NewRequest(http.MethodPost, client.url, body)
	if err != nil {
		return false, 0, fmt.Errorf("create otlp http request failed: %w", err)
	}

	// User headers first, then the protocol headers we know are correct for the body we built,
	// so a stale Content-Type/Content-Encoding in the config cannot corrupt the request.
	for k, v := range client.headers {
		req.Header.Set(k, v)
	}
	if req.Header.Get("User-Agent") == "" {
		req.Header.Set("User-Agent", config.UserAgent)
	}
	req.Header.Set("Content-Type", contentType)
	if contentEncoding != "" {
		req.Header.Set("Content-Encoding", contentEncoding)
	} else {
		// The body is not compressed, so a Content-Encoding coming from the config must go, it
		// would make the server decode a body that was never encoded.
		req.Header.Del("Content-Encoding")
	}

	resp, err := f.client.Do(req)
	if err != nil {
		// Network level failures (connection refused, timeout, reset) are worth retrying.
		return true, 0, err
	}
	defer resp.Body.Close() //nolint:errcheck

	respBody, readErr := io.ReadAll(io.LimitReader(resp.Body, otlpHTTPMaxResponseReadBytes))
	if readErr != nil {
		logger.Warning(f.context.GetRuntimeContext(), selfmonitor.FlusherFlushAlarm, "otlp http flusher read response fail, error", readErr)
	}
	// Drain the rest so the connection can be reused.
	_, _ = io.Copy(io.Discard, resp.Body)

	if resp.StatusCode >= http.StatusOK && resp.StatusCode <= 299 {
		f.checkPartialSuccess(client, respBody)
		return false, 0, nil
	}

	err = fmt.Errorf("otlp http server returned %s: %s", resp.Status, f.decodeErrorStatus(respBody))

	switch resp.StatusCode {
	case http.StatusTooManyRequests, http.StatusServiceUnavailable:
		// Throttled, the server may tell us how long to wait.
		return true, parseRetryAfterDuration(resp.Header.Get("Retry-After")), err
	case http.StatusBadGateway, http.StatusGatewayTimeout:
		return true, 0, err
	default:
		// Everything else, including 500, is permanent per the OTLP/HTTP specification.
		return false, 0, err
	}
}

func (f *FlusherOTLPHTTP) nextRetryDelay(attempt int, retryAfter time.Duration) time.Duration {
	delay := f.Retry.MaxDelay
	if attempt < 32 {
		if d := f.Retry.InitialDelay << time.Duration(attempt); d > 0 && d < f.Retry.MaxDelay {
			delay = d
		}
	}

	// Apply an about equally distributed jitter in the second half of the interval, such that
	// the wait time falls into [delay/2, delay].
	half := int64(delay / 2)
	if jitter, err := rand.Int(rand.Reader, big.NewInt(half+1)); err == nil {
		delay = time.Duration(half + jitter.Int64())
	}

	// A server suggested Retry-After raises the delay, but never above MaxDelay: it is external
	// input, and honoring an arbitrarily large value would park the flush goroutine for that long.
	if retryAfter > delay {
		if retryAfter > f.Retry.MaxDelay {
			return f.Retry.MaxDelay
		}
		return retryAfter
	}
	return delay
}

// decodeErrorStatus renders the error body, which is a protobuf encoded google.rpc.Status for
// protobuf requests, into a log friendly message.
func (f *FlusherOTLPHTTP) decodeErrorStatus(body []byte) string {
	if len(body) == 0 {
		return "<empty body>"
	}

	var status spb.Status
	if err := proto.Unmarshal(body, &status); err == nil && status.GetMessage() != "" {
		return fmt.Sprintf("code=%d message=%s", status.GetCode(), status.GetMessage())
	}
	return string(body)
}

// checkPartialSuccess warns about records the server rejected. A partial success is not an
// error for the retry machinery, resending the whole batch would duplicate the accepted records.
func (f *FlusherOTLPHTTP) checkPartialSuccess(client *otlpHTTPSignalClient, body []byte) {
	if len(body) == 0 {
		return
	}

	rejected, message, err := client.partialSuccess(body, f.Encoding == otlpHTTPEncodingJSON)
	if err != nil {
		logger.Debug(f.context.GetRuntimeContext(), "otlp http flusher cannot decode export response", err)
		return
	}

	if rejected != 0 || message != "" {
		logger.Warning(f.context.GetRuntimeContext(), selfmonitor.FlusherFlushAlarm, "otlp http server partially rejected data",
			"signal", client.signal, "rejected", rejected, "message", message)
	}
}

func decodeLogsPartialSuccess(body []byte, jsonEncoding bool) (int64, string, error) {
	resp := plogotlp.NewExportResponse()
	if err := unmarshalOTLP(resp, body, jsonEncoding); err != nil {
		return 0, "", err
	}
	partial := resp.PartialSuccess()
	return partial.RejectedLogRecords(), partial.ErrorMessage(), nil
}

func decodeMetricsPartialSuccess(body []byte, jsonEncoding bool) (int64, string, error) {
	resp := pmetricotlp.NewExportResponse()
	if err := unmarshalOTLP(resp, body, jsonEncoding); err != nil {
		return 0, "", err
	}
	partial := resp.PartialSuccess()
	return partial.RejectedDataPoints(), partial.ErrorMessage(), nil
}

func decodeTracesPartialSuccess(body []byte, jsonEncoding bool) (int64, string, error) {
	resp := ptraceotlp.NewExportResponse()
	if err := unmarshalOTLP(resp, body, jsonEncoding); err != nil {
		return 0, "", err
	}
	partial := resp.PartialSuccess()
	return partial.RejectedSpans(), partial.ErrorMessage(), nil
}

func unmarshalOTLP(msg otlpUnmarshaler, body []byte, jsonEncoding bool) error {
	if jsonEncoding {
		return msg.UnmarshalJSON(body)
	}
	return msg.UnmarshalProto(body)
}

// parseRetryAfterDuration reads a Retry-After header value, which is either delay seconds or an
// HTTP date. It returns 0 when the value is absent, unparsable or already in the past.
func parseRetryAfterDuration(header string) time.Duration {
	header = strings.TrimSpace(header)
	if header == "" {
		return 0
	}

	if seconds, err := strconv.ParseInt(header, 10, 64); err == nil {
		if seconds <= 0 {
			return 0
		}
		return time.Duration(seconds) * time.Second
	}

	if date, err := http.ParseTime(header); err == nil {
		if delay := time.Until(date); delay > 0 {
			return delay
		}
	}
	return 0
}

func init() {
	pipeline.Flushers["flusher_otlp_http"] = func() pipeline.Flusher {
		return NewFlusherOTLPHTTP()
	}
}
