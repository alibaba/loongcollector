package otel

import (
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/opentelemetry"
	"go.opentelemetry.io/collector/pdata/ptrace"
	v1 "go.opentelemetry.io/proto/otlp/trace/v1"
	"google.golang.org/protobuf/encoding/protojson"
	"strings"
)

type ProcessorOtelTraceParser struct {
	SourceKey              string
	Format                 string
	NoKeyError             bool
	KeepSource             bool
	context                pipeline.Context
	TraceIDNeedDecode      bool
	SpanIDNeedDecode       bool
	ParentSpanIDNeedDecode bool
}

const pluginName = "processor_otel_trace"

func (p *ProcessorOtelTraceParser) Init(context pipeline.Context) error {
	p.context = context
	if p.Format == "" {
		logger.Warningf(p.context.GetRuntimeContext(), "PROCESSOR_OTEL_TRACE_DATA_FORMAT", "data format is empty, use protobuf")
		p.Format = "protobuf"
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

func (p *ProcessorOtelTraceParser) processLog(log *protocol.Log) ([]*protocol.Log, error) {
	var logs = make([]*protocol.Log, 0)

	findKey := false
	for idx := range log.Contents {
		if log.Contents[idx].Key == p.SourceKey {
			findKey = true
			objectVal := log.Contents[idx].Value

			switch strings.ToLower(p.Format) {
			case "json":
				if l, err := p.processJsonTraceData(objectVal); err != nil {
					return logs, err
				} else {
					logs = append(logs, l...)
				}
				break
			case "protobuf":
				if l, err := p.processProtobufTraceData(objectVal); err != nil {
					return logs, err
				} else {
					logs = append(logs, l...)
				}
			case "protojson":
				if l, err := p.processProtoJsonTraceData(objectVal); err != nil {
					return logs, err
				} else {
					logs = append(logs, l...)
				}
			}
		}
	}

	if !findKey && p.NoKeyError {
		logger.Warningf(p.context.GetRuntimeContext(), "PROCESSOR_OTEL_TRACE_FIND_ALARM", "cannot find key %v", p.SourceKey)
		return logs, nil
	}

	return logs, nil
}

func (p *ProcessorOtelTraceParser) processJsonTraceData(data string) ([]*protocol.Log, error) {
	var logs = make([]*protocol.Log, 0)
	jsonUnmarshaler := ptrace.JSONUnmarshaler{}
	if trace, err := jsonUnmarshaler.UnmarshalTraces([]byte(data)); err != nil {
		return nil, err
	} else {
		log, count := opentelemetry.ConvertTrace(trace)
		if count > 0 {
			logs = append(logs, log...)
		}
	}

	return logs, nil
}

func (p *ProcessorOtelTraceParser) processProtobufTraceData(val string) ([]*protocol.Log, error) {
	var logs = make([]*protocol.Log, 0)
	protoUnmarshaler := ptrace.ProtoUnmarshaler{}
	if trace, err := protoUnmarshaler.UnmarshalTraces([]byte(val)); err != nil {
		return nil, err
	} else {
		log, count := opentelemetry.ConvertTrace(trace)
		if count > 0 {
			logs = append(logs, log...)
		}
	}
	return logs, nil
}

func (p *ProcessorOtelTraceParser) processProtoJsonTraceData(val string) ([]*protocol.Log, error) {
	var logs = make([]*protocol.Log, 0)
	resourceSpans := &v1.ResourceSpans{}
	if err := protojson.Unmarshal([]byte(val), resourceSpans); err != nil {
		return nil, err
	} else {
		if log, e := opentelemetry.ConvertResourceSpans(resourceSpans, p.TraceIDNeedDecode, p.SpanIDNeedDecode, p.ParentSpanIDNeedDecode); e != nil {
			return nil, e
		} else if len(log) > 0 {
			logs = append(logs, log...)
		}
	}
	return logs, nil
}

func init() {
	pipeline.Processors[pluginName] = func() pipeline.Processor {
		return &ProcessorOtelTraceParser{
			SourceKey:  "",
			NoKeyError: true,
			KeepSource: false,
			Format:     "protobuf",
		}
	}
}
