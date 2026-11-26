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

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
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

func init() {
	pipeline.Processors[otelMetricPluginName] = func() pipeline.Processor {
		return &ProcessorOtelMetricParser{
			SourceKey:  "",
			NoKeyError: false,
			Format:     "",
		}
	}
}

// Process converts the OTLP payload of each log to native events grouped by resource.
// Other event types pass through unchanged.
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
			logger.Warningf(p.context.GetRuntimeContext(), selfmonitor.ProcessorOTELMetricParserAlarm, "parser otel metric error %v", err)
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

// unmarshalMetrics decodes the configured format. Protojson carries one ResourceMetrics object.
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
