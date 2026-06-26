// Copyright 2026 iLogtail Authors
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

package exportutil

import (
	"bytes"
	"encoding/json"
	"fmt"

	"github.com/alibaba/ilogtail/pkg/models"
)

// SerializePassthroughEvent JSON-encodes Metric/Span (and other non-log) events for log-only flushers.
func SerializePassthroughEvent(event models.PipelineEvent) ([]byte, error) {
	payload, err := passthroughPayload(event)
	if err != nil {
		return nil, err
	}
	var buf bytes.Buffer
	encoder := json.NewEncoder(&buf)
	encoder.SetEscapeHTML(false)
	if err := encoder.Encode(payload); err != nil {
		return nil, err
	}
	return bytes.TrimSpace(buf.Bytes()), nil
}

func passthroughPayload(event models.PipelineEvent) (map[string]interface{}, error) {
	payload := map[string]interface{}{
		"name":              event.GetName(),
		"timestamp":         event.GetTimestamp(),
		"observedTimestamp": event.GetObservedTimestamp(),
	}
	tags := make(map[string]string, event.GetTags().Len())
	for k, v := range event.GetTags().Iterator() {
		tags[k] = v
	}
	if len(tags) > 0 {
		payload["tags"] = tags
	}

	switch event.GetType() {
	case models.EventTypeMetric:
		payload["eventType"] = "metric"
		metric, ok := event.(*models.Metric)
		if !ok {
			return nil, fmt.Errorf("expected metric event, got %T", event)
		}
		payload["metricType"] = models.MetricTypeTexts[metric.GetMetricType()]
		if metric.GetValue().IsSingleValue() {
			payload["value"] = metric.GetValue().GetSingleValue()
		} else if metric.GetValue().IsMultiValues() {
			values := make(map[string]float64, metric.GetValue().GetMultiValues().Len())
			for k, v := range metric.GetValue().GetMultiValues().Iterator() {
				values[k] = v
			}
			payload["values"] = values
		}
		if metric.GetTypedValue().Len() > 0 {
			typedValues := make(map[string]interface{}, metric.GetTypedValue().Len())
			for k, v := range metric.GetTypedValue().Iterator() {
				typedValues[k] = v.Value
			}
			payload["typedValues"] = typedValues
		}
	case models.EventTypeSpan:
		payload["eventType"] = "span"
		span, ok := event.(*models.Span)
		if !ok {
			return nil, fmt.Errorf("expected span event, got %T", event)
		}
		payload["traceID"] = span.GetTraceID()
		payload["spanID"] = span.GetSpanID()
		payload["parentSpanID"] = span.GetParentSpanID()
		payload["kind"] = spanKindText(span.GetKind())
		payload["statusCode"] = span.GetStatus()
	case models.EventTypeByteArray:
		payload["eventType"] = "byteArray"
		payload["body"] = string(event.(models.ByteArray))
	default:
		return nil, fmt.Errorf("unsupported passthrough event type: %v", event.GetType())
	}
	return payload, nil
}

func spanKindText(kind models.SpanKind) string {
	switch kind {
	case models.SpanKindInternal:
		return models.SpanKindTextInternal
	case models.SpanKindServer:
		return models.SpanKindTextServer
	case models.SpanKindClient:
		return models.SpanKindTextClient
	case models.SpanKindProducer:
		return models.SpanKindTextProducer
	case models.SpanKindConsumer:
		return models.SpanKindTextConsumer
	default:
		return models.SpanKindTextInternal
	}
}
