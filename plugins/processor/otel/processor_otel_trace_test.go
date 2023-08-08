package otel

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

const protoJSONData = `
{"resource":{"attributes":[{"key":"cluster.logicId","value":{"stringValue":"1000"}},{"key":"cluster.name","value":{"stringValue":"1000_adb30_1000_information_schema"}},{"key":"group.id","value":{"stringValue":"1"}},{"key":"host.id","value":{"stringValue":"1004001"}},{"key":"instance.id","value":{"stringValue":"1004"}},{"key":"node.ip","value":{"stringValue":"100.81.136.64"}},{"key":"role","value":{"stringValue":"worker"}},{"key":"service.name","value":{"stringValue":"adb_worker"}},{"key":"telemetry.sdk.language","value":{"stringValue":"java"}},{"key":"telemetry.sdk.name","value":{"stringValue":"opentelemetry"}},{"key":"telemetry.sdk.version","value":{"stringValue":"1.27.0"}}]},"scopeSpans":[{"scope":{"name":"com.alibaba.cloud","attributes":[]},"spans":[{"traceId":"31646461386336653337343330356530","spanId":"0108b2d29b652107","parentSpanId":"468e99f19f43d0db","name":"QueryExecutor::localQuery()","kind":1,"startTimeUnixNano":"1689831889338531120","endTimeUnixNano":"1689831889338737020","attributes":[{"key":"query.early_stop.pe_query_cnt","value":{"stringValue":"1"}},{"key":"query.early_stop.rt_query_cnt","value":{"stringValue":"0"}},{"key":"query.visit_pe_num","value":{"stringValue":"1"}},{"key":"query.early_stop.total_limit","value":{"stringValue":"2"}},{"key":"query.early_stop.hits","value":{"stringValue":"2"}}],"events":[],"links":[],"status":{}},{"traceId":"31646461386336653337343330356530","spanId":"dd0317ef39c234f6","parentSpanId":"468e99f19f43d0db","name":"QueryExecutor::query()","kind":1,"startTimeUnixNano":"1689831889338404546","endTimeUnixNano":"1689831889338761308","attributes":[{"key":"query.row_count","value":{"stringValue":"2"}}],"events":[],"links":[],"status":{}},{"traceId":"31646461386336653337343330356530","spanId":"65a238c63ced06fd","parentSpanId":"468e99f19f43d0db","name":"PagedTableScanner::query()","kind":1,"startTimeUnixNano":"1689831889338194922","endTimeUnixNano":"1689831889338764401","attributes":[{"key":"query.row_count","value":{"stringValue":"2"}}],"events":[],"links":[],"status":{}},{"traceId":"31646461386336653337343330356530","spanId":"78bb003e87ffa1fc","parentSpanId":"468e99f19f43d0db","name":"StorageConnectorPageSource::initTableScanner()","kind":1,"startTimeUnixNano":"1689831889338189439","endTimeUnixNano":"1689831889338767221","attributes":[{"key":"query.total_time","value":{"stringValue":"1"}}],"events":[],"links":[],"status":{}},{"traceId":"31646461386336653337343330356530","spanId":"468e99f19f43d0db","parentSpanId":"3439303136343738","name":"WorkerCStoreEngine::createPageSource()","kind":1,"startTimeUnixNano":"1689831889338000000","endTimeUnixNano":"1689831889339198132","attributes":[{"key":"trace.token","value":{"stringValue":"2023072013444910008113606410000016478"}},{"key":"process.id","value":{"stringValue":"2023072013444910008113606410000016478"}}],"events":[{"timeUnixNano":"1689831889339192155","name":"QueryStatus::end()","attributes":[]}],"links":[],"status":{}},{"traceId":"31646461396166623831393430356630","spanId":"ad0721b4ea368459","parentSpanId":"060465cb0a7a6282","name":"QueryExecutor::localQuery()","kind":1,"startTimeUnixNano":"1689831890388523601","endTimeUnixNano":"1689831890388740396","attributes":[{"key":"query.early_stop.pe_query_cnt","value":{"stringValue":"1"}},{"key":"query.early_stop.rt_query_cnt","value":{"stringValue":"0"}},{"key":"query.visit_pe_num","value":{"stringValue":"1"}},{"key":"query.early_stop.total_limit","value":{"stringValue":"2"}},{"key":"query.early_stop.hits","value":{"stringValue":"2"}}],"events":[],"links":[],"status":{}},{"traceId":"31646461396166623831393430356630","spanId":"d9453f84efe995d7","parentSpanId":"060465cb0a7a6282","name":"QueryExecutor::query()","kind":1,"startTimeUnixNano":"1689831890388391527","endTimeUnixNano":"1689831890388762835","attributes":[{"key":"query.row_count","value":{"stringValue":"2"}}],"events":[],"links":[],"status":{}},{"traceId":"31646461396166623831393430356630","spanId":"4363c32d09f34bed","parentSpanId":"060465cb0a7a6282","name":"PagedTableScanner::query()","kind":1,"startTimeUnixNano":"1689831890388180437","endTimeUnixNano":"1689831890388765799","attributes":[{"key":"query.row_count","value":{"stringValue":"2"}}],"events":[],"links":[],"status":{}},{"traceId":"31646461396166623831393430356630","spanId":"3043d3fba424f5b8","parentSpanId":"060465cb0a7a6282","name":"StorageConnectorPageSource::initTableScanner()","kind":1,"startTimeUnixNano":"1689831890388174923","endTimeUnixNano":"1689831890388771625","attributes":[{"key":"query.total_time","value":{"stringValue":"1"}}],"events":[],"links":[],"status":{}},{"traceId":"31646461396166623831393430356630","spanId":"060465cb0a7a6282","parentSpanId":"3530303136343739","name":"WorkerCStoreEngine::createPageSource()","kind":1,"startTimeUnixNano":"1689831890388000000","endTimeUnixNano":"1689831890389242072","attributes":[{"key":"trace.token","value":{"stringValue":"2023072013445010008113606410000016479"}},{"key":"process.id","value":{"stringValue":"2023072013445010008113606410000016479"}}],"events":[{"timeUnixNano":"1689831890389235586","name":"QueryStatus::end()","attributes":[]}],"links":[],"status":{}}]}],"schemaUrl":"https://opentelemetry.io/schemas/1.20.0"}
`

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
