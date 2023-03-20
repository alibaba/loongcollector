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
	"crypto/rand"
	"errors"
	"fmt"
	"io/ioutil"
	"math/big"
	"net/http"
	"net/url"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/fmtstr"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
)

const (
	defaultTimeout = time.Minute

	contentTypeHeader  = "Content-Type"
	defaultContentType = "application/octet-stream"
)

var contentTypeMaps = map[string]string{
	converter.EncodingJSON:     "application/json",
	converter.EncodingProtobuf: defaultContentType,
	converter.EncodingNone:     defaultContentType,
	converter.EncodingCustom:   defaultContentType,
}

type retryConfig struct {
	Enable        bool          // If enable retry, default is true
	MaxRetryTimes int           // Max retry times, default is 3
	InitialDelay  time.Duration // Delay time before the first retry, default is 1s
	MaxDelay      time.Duration // max delay time when retry, default is 30s
}

type FlusherHTTP struct {
	RemoteURL   string               // RemoteURL to request
	Headers     map[string]string    // Headers to append to the http request
	Query       map[string]string    // Query parameters to append to the http request
	Timeout     time.Duration        // Request timeout, default is 60s
	Retry       retryConfig          // Retry strategy, default is retry 3 times with delay time begin from 1second, max to 30 seconds
	Convert     helper.ConvertConfig // Convert defines which protocol and format to convert to
	Concurrency int                  // How many requests can be performed in concurrent

	varKeys []string

	context   pipeline.Context
	converter *converter.Converter
	client    *http.Client

	queue   chan interface{}
	counter sync.WaitGroup
}

func (f *FlusherHTTP) Description() string {
	return "http flusher for ilogtail"
}

func (f *FlusherHTTP) Init(context pipeline.Context) error {
	f.context = context
	logger.Info(f.context.GetRuntimeContext(), "http flusher init", "initializing")
	if f.RemoteURL == "" {
		err := errors.New("remoteURL is empty")
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "http flusher init fail, error", err)
		return err
	}

	if f.Concurrency < 1 {
		err := errors.New("concurrency must be greater than zero")
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "http flusher check concurrency fail, error", err)
		return err
	}

	converter, err := f.getConverter()
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "http flusher init converter fail, error", err)
		return err
	}
	f.converter = converter

	f.client = &http.Client{
		Timeout: f.Timeout,
	}
	transport, ok := http.DefaultTransport.(*http.Transport)
	if ok && f.Concurrency > transport.MaxIdleConnsPerHost {
		transport.MaxIdleConnsPerHost = f.Concurrency + 1
		f.client.Transport = transport
	}

	f.queue = make(chan interface{})
	for i := 0; i < f.Concurrency; i++ {
		go f.runFlushTask()
	}

	f.buildVarKeys()
	f.fillRequestContentType()

	logger.Info(f.context.GetRuntimeContext(), "http flusher init", "initialized")
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

func (f *FlusherHTTP) getConverter() (*converter.Converter, error) {
	return converter.NewConverterWithSep(f.Convert.Protocol, f.Convert.Encoding, f.Convert.Separator, f.Convert.IgnoreUnExpectedData, nil, nil)
}

func (f *FlusherHTTP) addTask(log interface{}) {
	f.counter.Add(1)
	f.queue <- log
}

func (f *FlusherHTTP) countDownTask() {
	f.counter.Done()
}

func (f *FlusherHTTP) runFlushTask() {
	for data := range f.queue {
		err := f.convertAndFlush(data)
		if err != nil {
			logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher failed convert or flush data, data dropped, error", err)
		}
	}
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
		logs, varValues, err = f.converter.ToByteStreamWithSelectedFieldsV2(v, f.varKeys)
	default:
		return fmt.Errorf("unsupport data type")
	}

	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher converter log fail, error", err)
		return err
	}
	switch rows := logs.(type) {
	case [][]byte:
		for idx, data := range rows {
			body, values := data, varValues[idx]
			err = f.flushWithRetry(body, values)
			if err != nil {
				logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher failed flush data after retry, data dropped, error", err)
			}
		}
		return nil
	case []byte:
		err = f.flushWithRetry(rows, nil)
		if err != nil {
			logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher failed flush data after retry, error", err)
		}
		return err
	default:
		err = fmt.Errorf("not supported logs type [%T]", logs)
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher failed flush data, error", err)
		return err
	}
}

func (f *FlusherHTTP) flushWithRetry(data []byte, varValues map[string]string) error {
	var err error
	for i := 0; i <= f.Retry.MaxRetryTimes; i++ {
		ok, retryable, e := f.flush(data, varValues)
		if ok || !retryable || !f.Retry.Enable {
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

func (f *FlusherHTTP) flush(data []byte, varValues map[string]string) (ok, retryable bool, err error) {
	req, err := http.NewRequest(http.MethodPost, f.RemoteURL, bytes.NewReader(data))
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher create request fail, error", err)
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
				logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher format query fail, error", ferr)
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
			logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher format header fail, error", ferr)
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
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALRAM", "http flusher send request fail, error", err)
		return false, retry, err
	}
	body, err := ioutil.ReadAll(response.Body)
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALRAM", "http flusher read response fail, error", err)
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
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher write data returned error, url", req.URL.String(), "status", response.Status, "body", string(body))
		return false, true, fmt.Errorf("err status returned: %v", response.Status)
	default:
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "http flusher write data returned error, url", req.URL.String(), "status", response.Status, "body", string(body))
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
		return &FlusherHTTP{
			Timeout:     defaultTimeout,
			Concurrency: 1,
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
}
