package otel

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/protocol"
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
