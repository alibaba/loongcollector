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

	"go.opentelemetry.io/collector/pdata/ptrace"
	tracecollector "go.opentelemetry.io/proto/otlp/collector/trace/v1"
	tracepb "go.opentelemetry.io/proto/otlp/trace/v1"
	"google.golang.org/protobuf/encoding/protojson"
	"google.golang.org/protobuf/proto"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/opentelemetry"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

type ProcessorOtelTraceParser struct {
	SourceKey  string
	Format     string
	NoKeyError bool
	context    pipeline.Context
	// ID decoding is selected by the wire format. These fields are accepted for configuration compatibility.
	TraceIDNeedDecode      bool
	SpanIDNeedDecode       bool
	ParentSpanIDNeedDecode bool
}

const pluginType = "processor_otel_trace"

func (p *ProcessorOtelTraceParser) Init(context pipeline.Context) error {
	p.context = context
	if p.Format == "" {
		logger.Warningf(p.context.GetRuntimeContext(), selfmonitor.ProcessorOTELTraceDataFormat, "data format is empty, use protobuf")
		return errors.New("The format field is empty")
	}
	return nil
}

func (p *ProcessorOtelTraceParser) Description() string {
	return "otel trace parser for logtail"
}

func init() {
	pipeline.Processors[pluginType] = func() pipeline.Processor {
		return &ProcessorOtelTraceParser{
			SourceKey:  "",
			NoKeyError: false,
			Format:     "",
		}
	}
}

// Process converts the OTLP payload of each log to native events grouped by resource.
// Other event types pass through unchanged.
func (p *ProcessorOtelTraceParser) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
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

		groups, err := p.convertLogToSpanGroups(log)
		if err != nil {
			logger.Warningf(p.context.GetRuntimeContext(), selfmonitor.ProcessorOTELTraceParserAlarm, "parser otel trace error %v", err)
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

// convertLogToSpanGroups reads the OTLP trace payload from p.SourceKey in the
// log indices, decodes it per the configured format and converts it into native
// models.Span events grouped by OTLP resource.
func (p *ProcessorOtelTraceParser) convertLogToSpanGroups(log *models.Log) ([]*models.PipelineGroupEvents, error) {
	raw := log.GetIndices().Get(p.SourceKey)
	if raw == nil {
		if p.NoKeyError {
			logger.Warningf(p.context.GetRuntimeContext(), selfmonitor.ProcessorOTELTraceFindAlarm, "cannot find key %v", p.SourceKey)
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

	traces, err := p.unmarshalTraces(data)
	if err != nil {
		return nil, err
	}
	return opentelemetry.ConvertOtlpTracesToGroupEvents(traces)
}

// unmarshalTraces decodes the configured format. Protojson carries one ResourceSpans object.
func (p *ProcessorOtelTraceParser) unmarshalTraces(data []byte) (ptrace.Traces, error) {
	switch strings.ToLower(p.Format) {
	case "json":
		unmarshaler := ptrace.JSONUnmarshaler{}
		return unmarshaler.UnmarshalTraces(data)
	case "protobuf":
		unmarshaler := ptrace.ProtoUnmarshaler{}
		return unmarshaler.UnmarshalTraces(data)
	case "protojson":
		unmarshaler := ptrace.JSONUnmarshaler{}
		wrapped := []byte(`{"resourceSpans":[` + string(data) + `]}`)
		traces, err := unmarshaler.UnmarshalTraces(wrapped)
		if err == nil {
			return traces, nil
		}
		// Protobuf JSON encodes byte fields as base64. OTLP JSON uses hex IDs.
		resourceSpans := &tracepb.ResourceSpans{}
		if err = (protojson.UnmarshalOptions{DiscardUnknown: true}).Unmarshal(data, resourceSpans); err != nil {
			return traces, err
		}
		request := &tracecollector.ExportTraceServiceRequest{ResourceSpans: []*tracepb.ResourceSpans{resourceSpans}}
		wire, err := proto.Marshal(request)
		if err != nil {
			return traces, err
		}
		return (&ptrace.ProtoUnmarshaler{}).UnmarshalTraces(wire)
	}
	return ptrace.NewTraces(), nil
}
