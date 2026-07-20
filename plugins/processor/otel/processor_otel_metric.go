// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package otel

import (
	"errors"
	"strings"

	"go.opentelemetry.io/collector/pdata/pmetric"
	v1 "go.opentelemetry.io/proto/otlp/metrics/v1"
	"google.golang.org/protobuf/encoding/protojson"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/opentelemetry"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

type ProcessorOtelMetricParser struct {
	SourceKey  string
	Format     string
	NoKeyError bool
	context    pipeline.Context
}

const otelMetricPluginName = "processor_otel_metric"

func (p *ProcessorOtelMetricParser) Init(context pipeline.Context) error {
	p.context = context
	if p.Format == "" {
		logger.Warningf(p.context.GetRuntimeContext(), selfmonitor.ProcessorOTELMetricDataFormat, "data format is empty, use protobuf")
		return errors.New("The format field is empty")
	}
	return nil
}

func (p *ProcessorOtelMetricParser) Description() string {
	return "otel metrics parser for logtail"
}

func (p *ProcessorOtelMetricParser) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	var logs = make([]*protocol.Log, 0)
	for _, log := range logArray {
		if l, err := p.processLog(log); err != nil {
			logger.Warningf(p.context.GetRuntimeContext(), selfmonitor.ProcessorOTELTraceParserAlarm, "parser otel trace error %v", err)
		} else {
			logs = append(logs, l...)
		}
	}
	return logs
}

func (p *ProcessorOtelMetricParser) processLog(log *protocol.Log) (logs []*protocol.Log, err error) {
	logs = make([]*protocol.Log, 0)

	findKey := false
	for idx := range log.Contents {
		if log.Contents[idx].Key == p.SourceKey {
			findKey = true
			objectVal := log.Contents[idx].Value
			var l []*protocol.Log

			switch strings.ToLower(p.Format) {
			case "json":
				if l, err = p.processJSONMetricData(objectVal); err != nil {
					return logs, err
				}
			case "protobuf":
				if l, err = p.processProtobufMetricData(objectVal); err != nil {
					return logs, err
				}
			case "protojson":
				if l, err = p.processProtoJSONMetricData(objectVal); err != nil {
					return logs, err
				}
			}
			logs = append(logs, l...)
		}
	}

	if !findKey && p.NoKeyError {
		logger.Warningf(p.context.GetRuntimeContext(), selfmonitor.ProcessorOTELMetricFindAlarm, "cannot find key %v", p.SourceKey)
		return logs, nil
	}

	return logs, nil
}

func (p *ProcessorOtelMetricParser) processJSONMetricData(data string) ([]*protocol.Log, error) {
	jsonUnmarshaler := pmetric.JSONUnmarshaler{}
	var metrics pmetric.Metrics
	var err error

	if metrics, err = jsonUnmarshaler.UnmarshalMetrics([]byte(data)); err != nil {
		return nil, err
	}

	log, _ := opentelemetry.ConvertOtlpMetricV1(metrics)
	return log, nil
}

func (p *ProcessorOtelMetricParser) processProtobufMetricData(data string) ([]*protocol.Log, error) {
	protoUnmarshaler := pmetric.ProtoUnmarshaler{}
	var metrics pmetric.Metrics
	var err error

	if metrics, err = protoUnmarshaler.UnmarshalMetrics([]byte(data)); err != nil {
		return nil, err
	}

	log, _ := opentelemetry.ConvertOtlpMetricV1(metrics)
	return log, nil
}

func (p *ProcessorOtelMetricParser) processProtoJSONMetricData(data string) ([]*protocol.Log, error) {
	metrics := &v1.ResourceMetrics{}
	var err error

	if err = protojson.Unmarshal([]byte(data), metrics); err != nil {
		return nil, err
	}

	var log []*protocol.Log
	if log, err = opentelemetry.ConvertOtlpMetrics(metrics); err != nil {
		return nil, err
	}

	return log, nil
}

func init() {
	pipeline.Processors[otelMetricPluginName] = func() pipeline.Processor {
		return &ProcessorOtelMetricParser{
			SourceKey:  "",
			NoKeyError: false,
			Format:     "",
		}
	}
}

// Process implements the v2 ProcessorV2 interface: "v2场景下输入Log输出Metric".
// For each Log event it reads the OTLP metric payload from p.SourceKey, decodes
// it per the configured format and emits native models.Metric events produced
// by the canonical OTLP→models converter. As in v1 (ProcessLogs/processLog),
// the source Log is NOT preserved: it is replaced by the converted metrics.
// Non-Log events (Metric/Span) are passed through unchanged.
func (p *ProcessorOtelMetricParser) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	if in == nil {
		return
	}

	passThrough := make([]models.PipelineEvent, 0, len(in.Events))
	for _, event := range in.Events {
		log, ok := event.(*models.Log)
		if !ok || event.GetType() != models.EventTypeLogging {
			passThrough = append(passThrough, event)
			continue
		}

		groups, err := p.convertLogToMetricGroups(log)
		if err != nil {
			// Mirror v1 ProcessLogs error handling: log and skip.
			logger.Warningf(p.context.GetRuntimeContext(), selfmonitor.ProcessorOTELTraceParserAlarm, "parser otel metric error %v", err)
			continue
		}
		for _, group := range groups {
			pipeline.CollectGroupEvents(context, group)
		}
	}

	if len(passThrough) > 0 {
		context.Collector().Collect(in.Group, passThrough...)
	}
}

// convertLogToMetricGroups reads the OTLP metric payload from p.SourceKey in the
// log indices, decodes it per the configured format and converts it into native
// models.Metric events grouped by OTLP resource.
func (p *ProcessorOtelMetricParser) convertLogToMetricGroups(log *models.Log) ([]*models.PipelineGroupEvents, error) {
	raw := log.GetIndices().Get(p.SourceKey)
	if raw == nil {
		if p.NoKeyError {
			logger.Warningf(p.context.GetRuntimeContext(), selfmonitor.ProcessorOTELMetricFindAlarm, "cannot find key %v", p.SourceKey)
		}
		return nil, nil
	}

	var data []byte
	switch v := raw.(type) {
	case []byte:
		data = v
	case string:
		data = []byte(v)
	default:
		return nil, nil
	}

	metrics, err := p.unmarshalMetrics(data)
	if err != nil {
		return nil, err
	}
	return opentelemetry.ConvertOtlpMetricsToGroupEvents(metrics)
}

// unmarshalMetrics decodes the OTLP metric payload into pdata pmetric.Metrics
// (the input accepted by the canonical converter). json/protobuf mirror v1
// exactly; protojson wraps the single ResourceMetrics object (v1's protojson
// input shape) into the standard OTLP request envelope so the pdata JSON
// unmarshaler can parse it.
func (p *ProcessorOtelMetricParser) unmarshalMetrics(data []byte) (pmetric.Metrics, error) {
	switch strings.ToLower(p.Format) {
	case "json":
		unmarshaler := pmetric.JSONUnmarshaler{}
		return unmarshaler.UnmarshalMetrics(data)
	case "protobuf":
		unmarshaler := pmetric.ProtoUnmarshaler{}
		return unmarshaler.UnmarshalMetrics(data)
	case "protojson":
		unmarshaler := pmetric.JSONUnmarshaler{}
		wrapped := []byte(`{"resourceMetrics":[` + string(data) + `]}`)
		return unmarshaler.UnmarshalMetrics(wrapped)
	}
	return pmetric.NewMetrics(), nil
}
