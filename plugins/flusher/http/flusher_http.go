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

package http

import (
	"bytes"
	"compress/gzip"
	"crypto/rand"
	"errors"
	"fmt"
	"io"
	"math/big"
	"net/http"
	"net/url"
	"sync"
	"time"

	"github.com/golang/snappy"

	"github.com/alibaba/ilogtail/pkg/fmtstr"
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/pipeline/extensions"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
)

const (
	defaultTimeout = time.Minute

	contentTypeHeader     = "Content-Type"
	defaultContentType    = "application/octet-stream"
	contentEncodingHeader = "Content-Encoding"
)

var contentTypeMaps = map[string]string{
	converter.EncodingJSON:     "application/json",
	converter.EncodingProtobuf: defaultContentType,
	converter.EncodingNone:     defaultContentType,
	converter.EncodingCustom:   defaultContentType,
}

var supportedCompressionType = map[string]any{
	"gzip":   nil,
	"snappy": nil,
}

type retryConfig struct {
	Enable        bool          // If enable retry, default is true
	MaxRetryTimes int           // Max retry times, default is 3
	InitialDelay  time.Duration // Delay time before the first retry, default is 1s
	MaxDelay      time.Duration // max delay time when retry, default is 30s
}

type Client interface {
	Do(req *http.Request) (*http.Response, error)
}

type FlusherHTTP struct {
	RemoteURL              string                       // RemoteURL to request
	Headers                map[string]string            // Headers to append to the http request
	Query                  map[string]string            // Query parameters to append to the http request
	Timeout                time.Duration                // Request timeout, default is 60s
	Retry                  retryConfig                  // Retry strategy, default is retry 3 times with delay time begin from 1second, max to 30 seconds
	Encoder                *extensions.ExtensionConfig  // Encoder defines which protocol and format to encode to
	Convert                helper.ConvertConfig         // Convert defines which protocol and format to convert to
	Concurrency            int                          // How many requests can be performed in concurrent
	MaxConnsPerHost        int                          // MaxConnsPerHost for http.Transport
	MaxIdleConnsPerHost    int                          // MaxIdleConnsPerHost for http.Transport
	IdleConnTimeout        time.Duration                // IdleConnTimeout for http.Transport
	WriteBufferSize        int                          // WriteBufferSize for http.Transport
	Authenticator          *extensions.ExtensionConfig  // name and options of the extensions.ClientAuthenticator extension to use
	FlushInterceptor       *extensions.ExtensionConfig  // name and options of the extensions.FlushInterceptor extension to use
	AsyncIntercept         bool                         // intercept the event asynchronously
	RequestInterceptors    []extensions.ExtensionConfig // custom request interceptor settings
	QueueCapacity          int                          // capacity of channel
	DropEventWhenQueueFull bool                         // If true, pipeline events will be dropped when the queue is full
	Compression            string                       // Compression type, support gzip and snappy at this moment.

	varKeys []string

	context     pipeline.Context
	encoder     extensions.Encoder
	converter   *converter.Converter
	client      Client
	interceptor extensions.FlushInterceptor

	queue   chan interface{}
	counter sync.WaitGroup
}

func NewHTTPFlusher() *FlusherHTTP {
	return &FlusherHTTP{
		QueueCapacity: 1024,
		Timeout:       defaultTimeout,
		Concurrency:   1,
		Convert: helper.ConvertConfig{
			Protocol:             converter.ProtocolCustomSingle,
			Encoding:             converter.EncodingJSON,
			IgnoreUnExpectedData: true,
		},
		Retry: retryConfig{
			Enable:        true,
			MaxRetryTimes: 3,
			InitialDelay:  time.Second,
			MaxDelay:      30 * time.Second,
		},
	}
}

func (f *FlusherHTTP) Description() string {
	return "http flusher for ilogtail"
}

func (f *FlusherHTTP) Init(context pipeline.Context) error {
	f.context = context
	logger.Info(f.context.GetRuntimeContext(), "http flusher init", "initializing")
	if f.RemoteURL == "" {
		err := errors.New("remoteURL is empty")
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "http flusher init fail, error", err)
		return err
	}

	if f.Concurrency < 1 {
		err := errors.New("concurrency must be greater than zero")
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "http flusher check concurrency fail, error", err)
		return err
	}

	var err error
	if err = f.initEncoder(); err != nil {
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "http flusher init encoder fail, error", err)
		return err
	}

	if err = f.initConverter(); err != nil {
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "http flusher init converter fail, error", err)
		return err
	}

	if f.FlushInterceptor != nil {
		var ext pipeline.Extension
		ext, err = f.context.GetExtension(f.FlushInterceptor.Type, f.FlushInterceptor.Options)
		if err != nil {
			logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "http flusher init filter fail, error", err)
			return err
		}
		interceptor, ok := ext.(extensions.FlushInterceptor)
		if !ok {
			err = fmt.Errorf("filter(%s) not implement interface extensions.FlushInterceptor", f.FlushInterceptor)
			logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "http flusher init filter fail, error", err)
			return err
		}
		f.interceptor = interceptor
	}

	err = f.initHTTPClient()
	if err != nil {
		return err
	}

	if f.QueueCapacity <= 0 {
		f.QueueCapacity = 1024
	}
	f.queue = make(chan interface{}, f.QueueCapacity)
	for i := 0; i < f.Concurrency; i++ {
		go f.runFlushTask()
	}

	f.buildVarKeys()
	f.fillRequestContentType()

	logger.Info(f.context.GetRuntimeContext(), "http flusher init", "initialized",
		"timeout", f.Timeout,
		"compression", f.Compression)
	return nil
}

func (f *FlusherHTTP) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	for _, logGroup := range logGroupList {
		f.addTask(logGroup)
	}
	return nil
}

func (f *FlusherHTTP) Export(groupEventsArray []*models.PipelineGroupEvents, ctx pipeline.PipelineContext) error {
	for _, groupEvents := range groupEventsArray {
		if !f.AsyncIntercept && f.interceptor != nil {
			groupEvents = f.interceptor.Intercept(groupEvents)
			// skip groupEvents that is nil or empty.
			if groupEvents == nil || len(groupEvents.Events) == 0 {
				continue
			}
		}

		f.addTask(groupEvents)
	}
	return nil
}

func (f *FlusherHTTP) SetUrgent(flag bool) {
}

func (f *FlusherHTTP) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return f.client != nil
}

func (f *FlusherHTTP) Stop() error {
	f.counter.Wait()
	close(f.queue)
	return nil
}

func (f *FlusherHTTP) SetHTTPClient(client Client) {
	f.client = client
}

func (f *FlusherHTTP) initEncoder() error {
	if f.Encoder == nil {
		return nil
	}

	ext, err := f.context.GetExtension(f.Encoder.Type, f.Encoder.Options)
	if err != nil {
		return fmt.Errorf("get extension failed, error: %w", err)
	}

	enc, ok := ext.(extensions.Encoder)
	if !ok {
		return fmt.Errorf("filter(%s) not implement interface extensions.Encoder", f.Encoder)
	}

	f.encoder = enc

	return nil
}

func (f *FlusherHTTP) initConverter() error {
	conv, err := f.getConverter()
	if err == nil {
		f.converter = conv
		return nil
	}

	if f.encoder == nil {
		// e.g.
		// Prometheus http flusher does not config helper.ConvertConfig,
		// but must config encoder config (i.e. prometheus encoder config).
		// If err != nil, meanwhile http flusher has no encoder,
		// flusher cannot work, so should return error.
		return err
	}

	return nil
}

func (f *FlusherHTTP) getConverter() (*converter.Converter, error) {
	return converter.NewConverterWithSep(f.Convert.Protocol, f.Convert.Encoding, f.Convert.Separator, f.Convert.IgnoreUnExpectedData, f.Convert.TagFieldsRename, f.Convert.ProtocolFieldsRename, f.context.GetPipelineScopeConfig())
}

func (f *FlusherHTTP) initHTTPClient() error {
	transport := http.DefaultTransport
	if dt, ok := transport.(*http.Transport); ok {
		dt = dt.Clone()
		if f.Concurrency > dt.MaxIdleConnsPerHost {
			dt.MaxIdleConnsPerHost = f.Concurrency + 1
		}
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

	var err error
	transport, err = f.initRequestInterceptors(transport)
	if err != nil {
		return err
	}

	if f.Authenticator != nil {
		var auth pipeline.Extension
		auth, err = f.context.GetExtension(f.Authenticator.Type, f.Authenticator.Options)
		if err != nil {
			logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "http flusher init authenticator fail, error", err)
			return err
		}
		ca, ok := auth.(extensions.ClientAuthenticator)
		if !ok {
			err = fmt.Errorf("authenticator(%s) not implement interface extensions.ClientAuthenticator", f.Authenticator)
			logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "http flusher init authenticator fail, error", err)
			return err
		}
		transport, err = ca.RoundTripper(transport)
		if err != nil {
			logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "http flusher init authenticator fail, error", err)
			return err
		}
	}

	f.client = &http.Client{
		Timeout:   f.Timeout,
		Transport: transport,
	}
	return nil
}

func (f *FlusherHTTP) initRequestInterceptors(transport http.RoundTripper) (http.RoundTripper, error) {
	for i := len(f.RequestInterceptors) - 1; i >= 0; i-- {
		setting := f.RequestInterceptors[i]
		ext, err := f.context.GetExtension(setting.Type, setting.Options)
		if err != nil {
			logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "http flusher init request interceptor fail, error", err)
			return nil, err
		}
		interceptor, ok := ext.(extensions.RequestInterceptor)
		if !ok {
			err = fmt.Errorf("interceptor(%s) with type %T not implement interface extensions.RequestInterceptor", setting.Type, ext)
			logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "http flusher init request interceptor fail, error", err)
			return nil, err
		}
		transport, err = interceptor.RoundTripper(transport)
		if err != nil {
			logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "http flusher init request interceptor fail, error", err)
			return nil, err
		}
	}
	return transport, nil
}

func (f *FlusherHTTP) addTask(log interface{}) {
	f.counter.Add(1)

	if f.DropEventWhenQueueFull {
		select {
		case f.queue <- log:
		default:
			f.counter.Done()
			logger.Warningf(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher dropped a group event since the queue is full")
		}
	} else {
		f.queue <- log
	}
}

func (f *FlusherHTTP) countDownTask() {
	f.counter.Done()
}

func (f *FlusherHTTP) runFlushTask() {
	flushTaskFn, action := f.convertAndFlush, "convert"
	if f.encoder != nil {
		flushTaskFn, action = f.encodeAndFlush, "encode"
	}

	for data := range f.queue {
		err := flushTaskFn(data)
		if err != nil {
			logger.Warningf(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM",
				"http flusher failed %s or flush data, data dropped, error: %s", action, err.Error())
		}
	}
}

func (f *FlusherHTTP) encodeAndFlush(event any) error {
	defer f.countDownTask()

	var data [][]byte
	var err error

	switch v := event.(type) {
	case *models.PipelineGroupEvents:
		data, err = f.encoder.EncodeV2(v)

	default:
		return errors.New("unsupported event type")
	}

	if err != nil {
		return fmt.Errorf("http flusher encode event data fail, error: %w", err)
	}

	for _, shard := range data {
		if err = f.flushWithRetry(shard, nil); err != nil {
			logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM",
				"http flusher failed flush data after retry, data dropped, error", err,
				"remote url", f.RemoteURL)
		}
	}

	return nil
}

func (f *FlusherHTTP) convertAndFlush(data interface{}) error {
	defer f.countDownTask()
	var logs interface{}
	var varValues []map[string]string
	var err error
	switch v := data.(type) {
	case *protocol.LogGroup:
		logs, varValues, err = f.converter.ToByteStreamWithSelectedFields(v, f.varKeys)
	case *models.PipelineGroupEvents:
		if f.AsyncIntercept && f.interceptor != nil {
			v = f.interceptor.Intercept(v)
			if v == nil || len(v.Events) == 0 {
				return nil
			}
		}
		logs, varValues, err = f.converter.ToByteStreamWithSelectedFieldsV2(v, f.varKeys)
	default:
		return fmt.Errorf("unsupport data type")
	}

	if err != nil {
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher converter log fail, error", err)
		return err
	}
	switch rows := logs.(type) {
	case [][]byte:
		for idx, data := range rows {
			body, values := data, varValues[idx]
			err = f.flushWithRetry(body, values)
			if err != nil {
				logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher failed flush data after retry, data dropped, error", err)
			}
		}
		return nil
	case []byte:
		err = f.flushWithRetry(rows, nil)
		if err != nil {
			logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher failed flush data after retry, error", err)
		}
		return err
	default:
		err = fmt.Errorf("not supported logs type [%T]", logs)
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher failed flush data, error", err)
		return err
	}
}

func (f *FlusherHTTP) flushWithRetry(data []byte, varValues map[string]string) error {
	var err error
	for i := 0; i <= f.Retry.MaxRetryTimes; i++ {
		ok, retryable, e := f.flush(data, varValues)
		if ok || !retryable || !f.Retry.Enable {
			err = e
			break
		}
		err = e
		<-time.After(f.getNextRetryDelay(i))
	}
	converter.PutPooledByteBuf(&data)
	return err
}

func (f *FlusherHTTP) getNextRetryDelay(retryTime int) time.Duration {
	delay := f.Retry.InitialDelay * 1 << time.Duration(retryTime)
	if delay > f.Retry.MaxDelay {
		delay = f.Retry.MaxDelay
	}

	// apply about equaly distributed jitter in second half of the interval, such that the wait
	// time falls into the interval [dur/2, dur]
	harf := int64(delay / 2)
	jitter, err := rand.Int(rand.Reader, big.NewInt(harf+1))
	if err != nil {
		return delay
	}
	return time.Duration(harf + jitter.Int64())
}

func (f *FlusherHTTP) compressData(data []byte) (io.Reader, error) {
	var reader io.Reader = bytes.NewReader(data)
	if compressionType, ok := f.Headers[contentEncodingHeader]; ok {
		switch compressionType {
		case "gzip":
			var buf bytes.Buffer
			gw := gzip.NewWriter(&buf)
			if _, err := gw.Write(data); err != nil {
				return nil, err
			}
			if err := gw.Close(); err != nil {
				return nil, err
			}
			reader = &buf
		case "snappy":
			compressedData := snappy.Encode(nil, data)
			reader = bytes.NewReader(compressedData)
		default:
		}
	}
	return reader, nil
}

func (f *FlusherHTTP) flush(data []byte, varValues map[string]string) (ok, retryable bool, err error) {
	reader, err := f.compressData(data)
	if err != nil {
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "create reader error", err)
		return false, false, err
	}

	req, err := http.NewRequest(http.MethodPost, f.RemoteURL, reader)
	if err != nil {
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher create request fail, error", err)
		return false, false, err
	}

	if len(f.Query) > 0 {
		values := req.URL.Query()
		for k, v := range f.Query {
			if len(f.varKeys) == 0 {
				values.Add(k, v)
				continue
			}

			fv, ferr := fmtstr.FormatTopic(varValues, v)
			if ferr != nil {
				logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher format query fail, error", ferr)
			} else {
				v = *fv
			}
			values.Add(k, v)
		}
		req.URL.RawQuery = values.Encode()
	}

	for k, v := range f.Headers {
		if len(f.varKeys) == 0 {
			req.Header.Add(k, v)
			continue
		}
		fv, ferr := fmtstr.FormatTopic(varValues, v)
		if ferr != nil {
			logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher format header fail, error", ferr)
		} else {
			v = *fv
		}
		req.Header.Add(k, v)
	}
	response, err := f.client.Do(req)
	if logger.DebugFlag() {
		logger.Debugf(f.context.GetRuntimeContext(), "request [method]: %v; [header]: %v; [url]: %v; [body]: %v", req.Method, req.Header, req.URL, string(data))
	}
	if err != nil {
		urlErr, ok := err.(*url.Error)
		retry := false
		if ok && (urlErr.Timeout() || urlErr.Temporary()) {
			retry = true
		}
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALRAM", "http flusher send request fail, error", err)
		return false, retry, err
	}
	body, err := io.ReadAll(response.Body)
	if err != nil {
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALRAM", "http flusher read response fail, error", err)
		return false, false, err
	}
	err = response.Body.Close()
	if err != nil {
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher close response body fail, error", err)
		return false, false, err
	}
	switch response.StatusCode / 100 {
	case 2:
		return true, false, nil
	case 5:
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher write data returned error, url", req.URL.String(), "status", response.Status, "body", string(body))
		return false, true, fmt.Errorf("err status returned: %v", response.Status)
	default:
		if response.StatusCode == http.StatusUnauthorized || response.StatusCode == http.StatusForbidden {
			return false, true, fmt.Errorf("err status returned: %v", response.Status)
		}
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher write data returned error, url", req.URL.String(), "status", response.Status, "body", string(body))
		return false, false, fmt.Errorf("unexpected status returned: %v", response.Status)
	}
}

func (f *FlusherHTTP) buildVarKeys() {
	cache := map[string]struct{}{}
	defines := []map[string]string{f.Query, f.Headers}

	for _, define := range defines {
		for _, v := range define {
			keys, err := fmtstr.CompileKeys(v)
			if err != nil {
				logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "http flusher init varKeys fail, err", err)
			}
			for _, key := range keys {
				cache[key] = struct{}{}
			}
		}
	}

	varKeys := make([]string, 0, len(cache))
	for k := range cache {
		varKeys = append(varKeys, k)
	}
	f.varKeys = varKeys
}

func (f *FlusherHTTP) fillRequestContentType() {
	if f.Headers == nil {
		f.Headers = make(map[string]string, 4)
	}

	if f.Compression != "" {
		if _, ok := supportedCompressionType[f.Compression]; ok {
			f.Headers[contentEncodingHeader] = f.Compression
		}
	}

	_, ok := f.Headers[contentTypeHeader]
	if ok {
		return
	}

	contentType, ok := contentTypeMaps[f.Convert.Encoding]
	if !ok {
		contentType = defaultContentType
	}
	f.Headers[contentTypeHeader] = contentType
}

func init() {
	pipeline.Flushers["flusher_http"] = func() pipeline.Flusher {
		return NewHTTPFlusher()
	}
}
