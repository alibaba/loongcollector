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

package loki

import (
	"errors"
	"time"

	"github.com/grafana/loki-client-go/loki"
	"github.com/grafana/loki-client-go/pkg/backoff"
	"github.com/grafana/loki-client-go/pkg/labelutil"
	"github.com/grafana/loki-client-go/pkg/urlutil"
	promconf "github.com/prometheus/common/config"
	"github.com/prometheus/common/model"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
)

type FlusherLoki struct {
	// Convert ilogtail data convert config
	Convert convertConfig
	// URL url of loki server
	URL string
	// TenantID empty string means single tenant mode
	TenantID string
	// MaxMessageWait maximum wait period before sending batch of message
	MaxMessageWait time.Duration
	// MaxMessageBytes maximum batch size of message to accrue before sending
	MaxMessageBytes int
	// Timeout maximum time to wait for server to respond
	Timeout time.Duration
	// MinBackoff minimum backoff time between retries
	MinBackoff time.Duration
	// MaxBackoff maximum backoff time between retries
	MaxBackoff time.Duration
	// MaxRetries maximum number of retries when sending batches
	MaxRetries int
	// DynamicLabels labels to be parsed from logs
	DynamicLabels []string
	// StaticLabels labels to add to each log
	StaticLabels model.LabelSet
	// ClientConfig http client config
	ClientConfig promconf.HTTPClientConfig

	context    pipeline.Context
	converter  *converter.Converter
	lokiClient *loki.Client
}

type convertConfig struct {
	// Rename one or more fields from tags.
	TagFieldsRename map[string]string
	// Rename one or more fields, The protocol field options can only be: contents, tags, time
	ProtocolFieldsRename map[string]string
	// Convert protocol, default value: custom_single
	Protocol string
	// Convert encoding, default value:json
	// The options are: 'json'
	Encoding string
	// Include only contents in the final result.
	OnlyContents bool
}

func NewFlusherLoki() *FlusherLoki {
	return &FlusherLoki{
		URL:             "",
		TenantID:        "",
		MaxMessageWait:  time.Duration(1) * time.Second,
		MaxMessageBytes: 1024 * 1024,
		Timeout:         time.Duration(10) * time.Second,
		MinBackoff:      time.Duration(500) * time.Millisecond,
		MaxBackoff:      time.Duration(5) * time.Minute,
		MaxRetries:      10,
		StaticLabels:    model.LabelSet{},
		Convert: convertConfig{
			Protocol: converter.ProtocolCustomSingle,
			Encoding: converter.EncodingJSON,
		},
	}
}

func (f *FlusherLoki) Init(context pipeline.Context) error {
	f.context = context
	if err := f.Validate(); err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init loki flusher error", err)
		return err
	}
	// Set default value while not set
	if f.Convert.Encoding == "" {
		f.converter.Encoding = converter.EncodingJSON
	}
	if f.Convert.Protocol == "" {
		f.Convert.Protocol = converter.ProtocolCustomSingle
	}
	// Init converter
	convert, err := f.getConverter()
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init loki flusher converter error", err)
		return err
	}
	f.converter = convert
	config, err := f.buildLokiConfig()
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init loki flusher error", err)
		return err
	}
	client, err := loki.New(config)
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init loki flusher error", err)
		return err
	}
	f.lokiClient = client
	return nil
}

func (f *FlusherLoki) Validate() error {
	if f.URL == "" {
		return errors.New("url is nil")
	}
	if f.MaxMessageBytes <= 0 {
		return errors.New("batch size is required > 0")
	}
	if len(f.StaticLabels) == 0 && len(f.DynamicLabels) == 0 {
		return errors.New("at least one label should be set")
	}
	return nil
}

func (f *FlusherLoki) Description() string {
	return "Loki flusher for logtail"
}

func (f *FlusherLoki) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return true
}

func (f *FlusherLoki) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	for _, logGroup := range logGroupList {
		logger.Debug(f.context.GetRuntimeContext(), "[LogGroup] topic", logGroup.Topic, "logstore", logGroup.Category, "logcount", len(logGroup.Logs), "tags", logGroup.LogTags)
		serializedLogs, values, err := f.converter.ToByteStreamWithSelectedFields(logGroup, f.DynamicLabels)
		if err != nil {
			logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "flush loki convert log fail, error", err)
			continue
		}

		for i, log := range serializedLogs.([][]byte) {
			labels := f.buildLokiLabels(values[i])
			// Append a log to the next batch, the sending is async
			err = f.lokiClient.Handle(labels, time.Unix(int64(logGroup.Logs[i].Time), 0), string(log))
			if err != nil {
				logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "flush loki convert log fail, error", err)
			}
		}
	}
	return nil
}

func (f *FlusherLoki) SetUrgent(flag bool) {}

func (f *FlusherLoki) Stop() error {
	f.lokiClient.Stop()
	return nil
}

func (f *FlusherLoki) getConverter() (*converter.Converter, error) {
	logger.Debug(f.context.GetRuntimeContext(), "[ilogtail data convert config] Protocol", f.Convert.Protocol,
		"Encoding", f.Convert.Encoding, "TagFieldsRename", f.Convert.TagFieldsRename, "ProtocolFieldsRename", f.Convert.ProtocolFieldsRename)
	cvt, err := converter.NewConverter(f.Convert.Protocol, f.Convert.Encoding, f.Convert.TagFieldsRename, f.Convert.ProtocolFieldsRename, f.context.GetPipelineScopeConfig())
	if err != nil {
		return nil, err
	}
	// only custom_single_flatten support contents_only
	if f.Convert.Protocol == converter.ProtocolCustomSingleFlatten && f.Convert.OnlyContents {
		cvt.OnlyContents = true
	}
	return cvt, nil
}

func (f *FlusherLoki) buildLokiConfig() (loki.Config, error) {
	config := loki.Config{
		TenantID:  f.TenantID,
		BatchWait: f.MaxMessageWait,
		BatchSize: f.MaxMessageBytes,
		Timeout:   f.Timeout,
		BackoffConfig: backoff.BackoffConfig{
			MinBackoff: f.MinBackoff,
			MaxBackoff: f.MaxBackoff,
			MaxRetries: f.MaxRetries,
		},
		ExternalLabels: labelutil.LabelSet{
			LabelSet: f.StaticLabels,
		},
	}
	var url urlutil.URLValue
	err := url.Set(f.URL)
	if err != nil {
		return config, errors.New("url is invalid")
	}
	config.URL = url
	return config, nil
}

func (f *FlusherLoki) buildLokiLabels(value map[string]string) model.LabelSet {
	labels := model.LabelSet{}
	// Skip if the labels is invalid or not found
	for key, val := range value {
		// Remove prefix, Loki doesn't support `.` in label
		key = converter.TrimPrefix(key)
		labels[model.LabelName(key)] = model.LabelValue(val)
	}
	return labels
}

func init() {
	pipeline.Flushers["flusher_loki"] = func() pipeline.Flusher {
		return NewFlusherLoki()
	}
}
