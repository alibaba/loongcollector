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
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/opentelemetry"
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
		logger.Warningf(p.context.GetRuntimeContext(), "PROCESSOR_OTEL_TRACE_DATA_FORMAT", "data format is empty, use protobuf")
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
			logger.Errorf(p.context.GetRuntimeContext(), "PROCESSOR_OTEL_TRACE_PARSER_ALARM", "parser otel trace error %v", err)
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
		logger.Warningf(p.context.GetRuntimeContext(), "PROCESSOR_OTEL_TRACE_FIND_ALARM", "cannot find key %v", p.SourceKey)
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

	if err = protojson.Unmarshal([]byte(val), resourceSpans); err != nil {
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
