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
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

const protoJSONData = `
{"resource":{"attributes":[{"key":"cluster.logicId","value":{"stringValue":"1000"}},{"key":"cluster.name","value":{"stringValue":"1000_adb30_1000_information_schema"}},{"key":"group.id","value":{"stringValue":"1"}},{"key":"host.id","value":{"stringValue":"1004001"}},{"key":"instance.id","value":{"stringValue":"1004"}},{"key":"node.ip","value":{"stringValue":"100.81.136.64"}},{"key":"role","value":{"stringValue":"worker"}},{"key":"service.name","value":{"stringValue":"adb_worker"}},{"key":"telemetry.sdk.language","value":{"stringValue":"java"}},{"key":"telemetry.sdk.name","value":{"stringValue":"opentelemetry"}},{"key":"telemetry.sdk.version","value":{"stringValue":"1.27.0"}}]},"scopeSpans":[{"scope":{"name":"com.alibaba.cloud","attributes":[]},"spans":[{"traceId":"31646461386336653337343330356530","spanId":"0108b2d29b652107","parentSpanId":"468e99f19f43d0db","name":"QueryExecutor::localQuery()","kind":1,"startTimeUnixNano":"1689831889338531120","endTimeUnixNano":"1689831889338737020","attributes":[{"key":"query.early_stop.pe_query_cnt","value":{"stringValue":"1"}},{"key":"query.early_stop.rt_query_cnt","value":{"stringValue":"0"}},{"key":"query.visit_pe_num","value":{"stringValue":"1"}},{"key":"query.early_stop.total_limit","value":{"stringValue":"2"}},{"key":"query.early_stop.hits","value":{"stringValue":"2"}}],"events":[],"links":[],"status":{}},{"traceId":"31646461386336653337343330356530","spanId":"dd0317ef39c234f6","parentSpanId":"468e99f19f43d0db","name":"QueryExecutor::query()","kind":1,"startTimeUnixNano":"1689831889338404546","endTimeUnixNano":"1689831889338761308","attributes":[{"key":"query.row_count","value":{"stringValue":"2"}}],"events":[],"links":[],"status":{}},{"traceId":"31646461386336653337343330356530","spanId":"65a238c63ced06fd","parentSpanId":"468e99f19f43d0db","name":"PagedTableScanner::query()","kind":1,"startTimeUnixNano":"1689831889338194922","endTimeUnixNano":"1689831889338764401","attributes":[{"key":"query.row_count","value":{"stringValue":"2"}}],"events":[],"links":[],"status":{}},{"traceId":"31646461386336653337343330356530","spanId":"78bb003e87ffa1fc","parentSpanId":"468e99f19f43d0db","name":"StorageConnectorPageSource::initTableScanner()","kind":1,"startTimeUnixNano":"1689831889338189439","endTimeUnixNano":"1689831889338767221","attributes":[{"key":"query.total_time","value":{"stringValue":"1"}}],"events":[],"links":[],"status":{}},{"traceId":"31646461386336653337343330356530","spanId":"468e99f19f43d0db","parentSpanId":"3439303136343738","name":"WorkerCStoreEngine::createPageSource()","kind":1,"startTimeUnixNano":"1689831889338000000","endTimeUnixNano":"1689831889339198132","attributes":[{"key":"trace.token","value":{"stringValue":"2023072013444910008113606410000016478"}},{"key":"process.id","value":{"stringValue":"2023072013444910008113606410000016478"}}],"events":[{"timeUnixNano":"1689831889339192155","name":"QueryStatus::end()","attributes":[]}],"links":[],"status":{}},{"traceId":"31646461396166623831393430356630","spanId":"ad0721b4ea368459","parentSpanId":"060465cb0a7a6282","name":"QueryExecutor::localQuery()","kind":1,"startTimeUnixNano":"1689831890388523601","endTimeUnixNano":"1689831890388740396","attributes":[{"key":"query.early_stop.pe_query_cnt","value":{"stringValue":"1"}},{"key":"query.early_stop.rt_query_cnt","value":{"stringValue":"0"}},{"key":"query.visit_pe_num","value":{"stringValue":"1"}},{"key":"query.early_stop.total_limit","value":{"stringValue":"2"}},{"key":"query.early_stop.hits","value":{"stringValue":"2"}}],"events":[],"links":[],"status":{}},{"traceId":"31646461396166623831393430356630","spanId":"d9453f84efe995d7","parentSpanId":"060465cb0a7a6282","name":"QueryExecutor::query()","kind":1,"startTimeUnixNano":"1689831890388391527","endTimeUnixNano":"1689831890388762835","attributes":[{"key":"query.row_count","value":{"stringValue":"2"}}],"events":[],"links":[],"status":{}},{"traceId":"31646461396166623831393430356630","spanId":"4363c32d09f34bed","parentSpanId":"060465cb0a7a6282","name":"PagedTableScanner::query()","kind":1,"startTimeUnixNano":"1689831890388180437","endTimeUnixNano":"1689831890388765799","attributes":[{"key":"query.row_count","value":{"stringValue":"2"}}],"events":[],"links":[],"status":{}},{"traceId":"31646461396166623831393430356630","spanId":"3043d3fba424f5b8","parentSpanId":"060465cb0a7a6282","name":"StorageConnectorPageSource::initTableScanner()","kind":1,"startTimeUnixNano":"1689831890388174923","endTimeUnixNano":"1689831890388771625","attributes":[{"key":"query.total_time","value":{"stringValue":"1"}}],"events":[],"links":[],"status":{}},{"traceId":"31646461396166623831393430356630","spanId":"060465cb0a7a6282","parentSpanId":"3530303136343739","name":"WorkerCStoreEngine::createPageSource()","kind":1,"startTimeUnixNano":"1689831890388000000","endTimeUnixNano":"1689831890389242072","attributes":[{"key":"trace.token","value":{"stringValue":"2023072013445010008113606410000016479"}},{"key":"process.id","value":{"stringValue":"2023072013445010008113606410000016479"}}],"events":[{"timeUnixNano":"1689831890389235586","name":"QueryStatus::end()","attributes":[]}],"links":[],"status":{}}]}],"schemaUrl":"https://opentelemetry.io/schemas/1.20.0"}
`
const protoJSONWithFlags = `{"resource":{"attributes":[{"key":"service.name","value":{"stringValue":"test-service"}},{"key":"telemetry.sdk.language","value":{"stringValue":"go"}},{"key":"telemetry.sdk.name","value":{"stringValue":"opentelemetry"}},{"key":"telemetry.sdk.version","value":{"stringValue":"1.33.0"}}]},"scopeSpans":[{"scope":{"name":"github.com/pyet/framework/v2/pkg/core"},"spans":[{"traceId":"92ROnPOgG7S7B3VyyN/gwQ==","spanId":"BXxPh5hmJrU=","flags":256,"name":"syncResource","kind":"SPAN_KIND_INTERNAL","startTimeUnixNano":"1770195994522712216","endTimeUnixNano":"1770195994524000927","attributes":[{"key":"ref","value":{"stringValue":"testdata"}},{"key":"active","value":{"boolValue":false}}],"status":{}},{"traceId":"oZKhbsHGp0TB5mKbEPxvcA==","spanId":"ZIGxpIr1vFE=","flags":256,"name":"syncResource","kind":"SPAN_KIND_INTERNAL","startTimeUnixNano":"1770195994552858471","endTimeUnixNano":"1770195994554166025","attributes":[{"key":"ref","value":{"stringValue":"test-attribute-value"}},{"key":"active","value":{"boolValue":false}}],"status":{}}]}],"schemaUrl":"https://opentelemetry.io/schemas/1.26.0"}`

func TestParserOtelData(t *testing.T) {
	parser := ProcessorOtelTraceParser{
		SourceKey:              "otel",
		Format:                 "protojson",
		TraceIDNeedDecode:      true,
		SpanIDNeedDecode:       true,
		ParentSpanIDNeedDecode: true,
	}

	log := &protocol.Log{
		Contents: []*protocol.Log_Content{
			{
				Key:   "otel",
				Value: protoJSONData,
			},
		},
	}

	if result, err := parser.processLog(log); err != nil {
		t.Fatalf("processLog failed: %v", err)
	} else {
		assert.Equal(t, 10, len(result))
		assert.Equal(t, "adb_worker", result[0].Contents[1].Value)
	}
}

func TestParserOtelBase64Data(t *testing.T) {
	parser := ProcessorOtelTraceParser{
		SourceKey:              "otel",
		Format:                 "protojson",
		TraceIDNeedDecode:      false,
		SpanIDNeedDecode:       false,
		ParentSpanIDNeedDecode: false,
	}

	log := &protocol.Log{
		Contents: []*protocol.Log_Content{
			{
				Key:   "otel",
				Value: protoJSONWithFlags,
			},
		},
	}

	if result, err := parser.processLog(log); err != nil {
		t.Fatalf("processLog failed: %v", err)
	} else {
		assert.Equal(t, 2, len(result))
		assert.Equal(t, "test-service", result[0].Contents[1].Value)
	}
}

// ---- v2 (PipelineEvent / SendPb) Process path tests ----

// A Log carrying an OTLP trace payload is converted to native models.Span
// events ("v2场景下输入Log输出Span"); the source Log is replaced (not preserved).
func TestProcessorOtelTraceParser_ProcessV2LogToSpan(t *testing.T) {
	parser := &ProcessorOtelTraceParser{SourceKey: "otel", Format: "protojson"}
	require.NoError(t, parser.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("otel", protoJSONData)

	context := helper.NewObservePipelineContext(10)
	parser.Process(&models.PipelineGroupEvents{Group: models.NewGroup(models.NewMetadata(), models.NewTags()), Events: []models.PipelineEvent{log}}, context)

	results := context.Collector().ToArray()
	require.NotEmpty(t, results)

	spanCount := 0
	for _, group := range results {
		for _, event := range group.Events {
			assert.Equal(t, models.EventTypeSpan, event.GetType(), "source log must be replaced by span events")
			_, ok := event.(*models.Span)
			assert.True(t, ok)
			spanCount++
		}
	}
	// protoJSONData carries ten spans.
	assert.Equal(t, 10, spanCount)
}

// A pre-existing Span event passes through unchanged.
func TestProcessorOtelTraceParser_ProcessV2PassesThroughSpan(t *testing.T) {
	parser := &ProcessorOtelTraceParser{SourceKey: "otel", Format: "protojson"}
	require.NoError(t, parser.Init(mock.NewEmptyContext("p", "l", "c")))

	span := models.NewSpan("preexist", "trace", "span", models.SpanKindClient, 0, 0, models.NewTags(), nil, nil)
	context := helper.NewObservePipelineContext(10)
	parser.Process(&models.PipelineGroupEvents{Group: models.NewGroup(models.NewMetadata(), models.NewTags()), Events: []models.PipelineEvent{span}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	passed, ok := results[0].Events[0].(*models.Span)
	require.True(t, ok, "span event must pass through unchanged")
	assert.Equal(t, "preexist", passed.GetName())
}
