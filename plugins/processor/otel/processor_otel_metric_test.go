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

const protoJSONMetricData = `
{ "resource": { "attributes": [] }, "scopeMetrics": [ { "scope": { "name": "proxy-meter", "attributes": [] }, "metrics": [ { "name": "rocketmq.consumer.connections", "description": "default view for gauge.", "gauge": { "dataPoints": [ { "startTimeUnixNano": "1694766421663956000", "timeUnixNano": "1694766431663946000", "asDouble": 1.0, "exemplars": [], "attributes": [ { "key": "aggregation", "value": { "stringValue": "delta" } }, { "key": "cluster", "value": { "stringValue": "serverless-rocketmq-proxy-2" } }, { "key": "consume_mode", "value": { "stringValue": "push" } }, { "key": "consumer_group", "value": { "stringValue": "group_sub4" } }, { "key": "instance_id", "value": { "stringValue": "rmq-cn-2093d0d6g05" } }, { "key": "language", "value": { "stringValue": "java" } }, { "key": "node_id", "value": { "stringValue": "serverless-rocketmq-proxy-2-546c7c9777-gnh9s" } }, { "key": "node_type", "value": { "stringValue": "proxy" } }, { "key": "protocol_type", "value": { "stringValue": "remoting" } }, { "key": "uid", "value": { "stringValue": "1936715356116916" } }, { "key": "version", "value": { "stringValue": "v4_4_4_snapshot" } } ] } ] } }, { "name": "rocketmq.rpc.latency", "histogram": { "dataPoints": [ { "startTimeUnixNano": "1694766421663956000", "timeUnixNano": "1694766431663946000", "count": "150", "sum": 14.0, "min": 0.0, "max": 1.0, "bucketCounts": [ "150", "0", "0", "0", "0", "0" ], "explicitBounds": [ 1.0, 10.0, 100.0, 1000.0, 3000.0 ], "exemplars": [], "attributes": [ { "key": "aggregation", "value": { "stringValue": "delta" } }, { "key": "cluster", "value": { "stringValue": "serverless-rocketmq-proxy-2" } }, { "key": "instance_id", "value": { "stringValue": "rmq-cn-2093d0d6g05" } }, { "key": "node_id", "value": { "stringValue": "serverless-rocketmq-proxy-2-546c7c9777-gnh9s" } }, { "key": "node_type", "value": { "stringValue": "proxy" } }, { "key": "protocol_type", "value": { "stringValue": "remoting" } }, { "key": "request_code", "value": { "stringValue": "get_consumer_list_by_group" } }, { "key": "response_code", "value": { "stringValue": "system_error" } }, { "key": "uid", "value": { "stringValue": "1936715356116916" } } ] } ], "aggregationTemporality": 1 } } ] } ] }
`

func TestParserOtelMetricData(t *testing.T) {
	parser := ProcessorOtelMetricParser{
		SourceKey: "otel",
		Format:    "protojson",
	}

	log := &protocol.Log{
		Contents: []*protocol.Log_Content{
			{
				Key:   "otel",
				Value: protoJSONMetricData,
			},
		},
	}

	if result, err := parser.processLog(log); err != nil {
		t.Fatalf("processLog failed: %v", err)
	} else {
		assert.Equal(t, 10, len(result))
	}
}

// ---- v2 (PipelineEvent / SendPb) Process path tests ----

// A Log carrying an OTLP metric payload is converted to native models.Metric
// events ("v2场景下输入Log输出Metric"); the source Log is replaced (not preserved).
func TestProcessorOtelMetricParser_ProcessV2LogToMetric(t *testing.T) {
	parser := &ProcessorOtelMetricParser{SourceKey: "otel", Format: "protojson"}
	require.NoError(t, parser.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("otel", protoJSONMetricData)

	context := helper.NewObservePipelineContext(10)
	parser.Process(&models.PipelineGroupEvents{Group: models.NewGroup(models.NewMetadata(), models.NewTags()), Events: []models.PipelineEvent{log}}, context)

	results := context.Collector().ToArray()
	require.NotEmpty(t, results)

	metricCount := 0
	for _, group := range results {
		for _, event := range group.Events {
			assert.Equal(t, models.EventTypeMetric, event.GetType(), "source log must be replaced by metric events")
			_, ok := event.(*models.Metric)
			assert.True(t, ok)
			metricCount++
		}
	}
	// protoJSONMetricData carries one gauge and one histogram datapoint.
	assert.Equal(t, 2, metricCount)
}

// A pre-existing Metric event passes through unchanged.
func TestProcessorOtelMetricParser_ProcessV2PassesThroughMetric(t *testing.T) {
	parser := &ProcessorOtelMetricParser{SourceKey: "otel", Format: "protojson"}
	require.NoError(t, parser.Init(mock.NewEmptyContext("p", "l", "c")))

	metric := models.NewSingleValueMetric("preexist", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	context := helper.NewObservePipelineContext(10)
	parser.Process(&models.PipelineGroupEvents{Group: models.NewGroup(models.NewMetadata(), models.NewTags()), Events: []models.PipelineEvent{metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	passed, ok := results[0].Events[0].(*models.Metric)
	require.True(t, ok, "metric event must pass through unchanged")
	assert.Equal(t, "preexist", passed.GetName())
}
