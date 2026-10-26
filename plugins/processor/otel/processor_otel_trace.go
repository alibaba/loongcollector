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
	v1 "go.opentelemetry.io/proto/otlp/trace/v1"
	"google.golang.org/protobuf/encoding/protojson"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/opentelemetry"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

type ProcessorOtelTraceParser struct {
	SourceKey              string
	Format                 string
	NoKeyError             bool
	context                pipeline.Context
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

func (p *ProcessorOtelTraceParser) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
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

func (p *ProcessorOtelTraceParser) processLog(log *protocol.Log) (logs []*protocol.Log, err error) {
	logs = make([]*protocol.Log, 0)

	findKey := false
	for idx := range log.Contents {
		if log.Contents[idx].Key == p.SourceKey {
			findKey = true
			objectVal := log.Contents[idx].Value
			var l []*protocol.Log

			switch strings.ToLower(p.Format) {
			case "json":
				if l, err = p.processJSONTraceData(objectVal); err != nil {
					return logs, err
				}
			case "protobuf":
				if l, err = p.processProtobufTraceData(objectVal); err != nil {
					return logs, err
				}
			case "protojson":
				if l, err = p.processProtoJSONTraceData(objectVal); err != nil {
					return logs, err
				}
			}
			logs = append(logs, l...)
		}
	}

	if !findKey && p.NoKeyError {
		logger.Warningf(p.context.GetRuntimeContext(), selfmonitor.ProcessorOTELTraceFindAlarm, "cannot find key %v", p.SourceKey)
		return logs, nil
	}

	return logs, nil
}

func (p *ProcessorOtelTraceParser) processJSONTraceData(data string) ([]*protocol.Log, error) {
	jsonUnmarshaler := ptrace.JSONUnmarshaler{}
	var trace ptrace.Traces
	var err error

	if trace, err = jsonUnmarshaler.UnmarshalTraces([]byte(data)); err != nil {
		return nil, err
	}

	log, _ := opentelemetry.ConvertTrace(trace)
	return log, nil
}

func (p *ProcessorOtelTraceParser) processProtobufTraceData(val string) ([]*protocol.Log, error) {
	protoUnmarshaler := ptrace.ProtoUnmarshaler{}
	var trace ptrace.Traces
	var err error

	if trace, err = protoUnmarshaler.UnmarshalTraces([]byte(val)); err != nil {
		return nil, err
	}

	log, _ := opentelemetry.ConvertTrace(trace)
	return log, nil
}

func (p *ProcessorOtelTraceParser) processProtoJSONTraceData(val string) ([]*protocol.Log, error) {
	resourceSpans := &v1.ResourceSpans{}
	var err error

	opt := protojson.UnmarshalOptions{DiscardUnknown: true}
	if err = opt.Unmarshal([]byte(val), resourceSpans); err != nil {
		return nil, err
	}

	var log []*protocol.Log

	if log, err = opentelemetry.ConvertResourceSpans(resourceSpans, p.TraceIDNeedDecode, p.SpanIDNeedDecode, p.ParentSpanIDNeedDecode); err != nil {
		return nil, err
	}

	return log, nil
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

// Process implements the v2 ProcessorV2 interface: "v2场景下输入Log输出Span".
// For each Log event it reads the OTLP trace payload from p.SourceKey, decodes
// it per the configured format and emits native models.Span events produced by
// the canonical OTLP→models converter. As in v1 (ProcessLogs/processLog), the
// source Log is NOT preserved: it is replaced by the converted spans. Non-Log
// events (Metric/Span) are passed through unchanged.
//
// Note: the v1 protojson-only TraceIDNeedDecode/SpanIDNeedDecode/
// ParentSpanIDNeedDecode flags do not apply here. The pdata trace unmarshaler
// decodes span/trace IDs itself, and the canonical converter renders them via
// (*ptrace.Span).TraceID().String(); those v1 flags are ignored in v2.
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
			// Mirror v1 ProcessLogs error handling: log and skip.
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

// unmarshalTraces decodes the OTLP trace payload into pdata ptrace.Traces (the
// input accepted by the canonical converter). json/protobuf mirror v1 exactly;
// protojson wraps the single ResourceSpans object (v1's protojson input shape)
// into the standard OTLP request envelope so the pdata JSON unmarshaler can
// parse it.
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
		return unmarshaler.UnmarshalTraces(wrapped)
	}
	return ptrace.NewTraces(), nil
}
