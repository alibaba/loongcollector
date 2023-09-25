package otel

import (
	"errors"
	"strings"

	"go.opentelemetry.io/collector/pdata/pmetric"
	v1 "go.opentelemetry.io/proto/otlp/metrics/v1"
	"google.golang.org/protobuf/encoding/protojson"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/opentelemetry"
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
		logger.Warningf(p.context.GetRuntimeContext(), "PROCESSOR_OTEL_METRIC_DATA_FORMAT", "data format is empty, use protobuf")
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
			logger.Errorf(p.context.GetRuntimeContext(), "PROCESSOR_OTEL_TRACE_PARSER_ALARM", "parser otel trace error %v", err)
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
		logger.Warningf(p.context.GetRuntimeContext(), "PROCESSOR_OTEL_METRIC_FIND_ALARM", "cannot find key %v", p.SourceKey)
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
