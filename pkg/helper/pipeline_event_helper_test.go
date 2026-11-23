package helper

import (
	"bytes"
	"fmt"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/encoding/protowire"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

func TestTransferMetricEventMultiValueRoundTrip(t *testing.T) {
	tags := models.NewTags()
	tags.Add("hostname", "host-1")
	multiValue := models.NewMetricMultiValueWithMap(map[string]float64{
		"cpu":            0.2,
		"memory_used_mb": 30,
	})
	metric := models.NewMultiValuesMetric("agent", models.MetricTypeUntyped, tags, 1234567890, multiValue.Values)

	pbMetric, err := TransferMetricEventToPB(metric)
	require.NoError(t, err)

	restored, err := TransferPBToMetricEvent(pbMetric)
	require.NoError(t, err)
	assert.Equal(t, "agent", restored.GetName())
	assert.True(t, restored.GetValue().IsMultiValues())
	assert.Equal(t, 0.2, restored.GetValue().GetMultiValues().Get("cpu"))
	assert.Equal(t, 30.0, restored.GetValue().GetMultiValues().Get("memory_used_mb"))
}

func TestTransferLogEventRoundTrip(t *testing.T) {
	log := models.NewLog("", []byte("hello"), "info", "", "", models.NewTagsWithKeyValues("host", "h1"), 1_000_000_000)
	log.Offset = 42

	pbLog, err := TransferLogEventToPB(log)
	require.NoError(t, err)

	restored, err := TransferPBToLogEvent(pbLog)
	require.NoError(t, err)
	assert.Equal(t, uint64(1_000_000_000), restored.GetTimestamp())
	assert.Equal(t, "info", restored.GetLevel())
	assert.Equal(t, uint64(42), restored.GetOffset())
	assert.Equal(t, "hello", string(restored.GetBody()))
}

func TestTransferPBToLogEventsKeepBodyIndependent(t *testing.T) {
	body := []byte("original")
	pb := &protocol.LogEvent{Contents: []*protocol.LogEvent_Content{
		{Key: []byte(models.BodyKey), Value: body},
		{Key: []byte(models.BodyKey), Value: []byte{}},
	}}

	log, err := TransferPBToLogEvent(pb)
	require.NoError(t, err)
	group, err := TransferPBToPipelineGroupEvents(&protocol.PipelineEventGroup{
		PipelineEvents: &protocol.PipelineEventGroup_Logs{Logs: &protocol.PipelineEventGroup_LogEvents{
			Events: []*protocol.LogEvent{pb},
		}},
	})
	require.NoError(t, err)
	groupLog := group.Events[0].(*models.Log)

	body[0] = 'O'
	require.Equal(t, []byte("original"), log.GetBody())
	require.Equal(t, []byte("original"), groupLog.GetBody())
	log.GetBody()[1] = 'R'
	require.Equal(t, []byte("Original"), body)
	require.Equal(t, []byte("original"), groupLog.GetBody())
}

func TestDecodePipelineGroupEventsOwnsLogData(t *testing.T) {
	wire := marshalPipelineLogGroup(t,
		&protocol.LogEvent{
			Timestamp:  1_700_000_000_000_000_123,
			Level:      []byte("warning"),
			FileOffset: 100,
			RawSize:    127,
			Contents: []*protocol.LogEvent_Content{
				{Key: []byte("before"), Value: []byte("original")},
				{Key: []byte(models.BodyKey), Value: []byte("original body")},
				{Key: []byte("after"), Value: []byte("last field")},
				{Key: []byte("before"), Value: []byte("updated")},
				{Key: []byte(models.BodyKey), Value: []byte{0xff, 0xfe, 0}},
				{Key: []byte(models.BodyKey)},
				{Key: []byte("empty")},
			},
		},
		&protocol.LogEvent{},
		&protocol.LogEvent{Contents: []*protocol.LogEvent_Content{{Key: []byte(models.BodyKey)}}},
	)
	require.NotNil(t, decodePipelineLogGroup(wire))

	want, err := decodePipelineGroupEventsWithProtobuf(wire)
	require.NoError(t, err)
	got, err := DecodePipelineGroupEvents(wire)
	require.NoError(t, err)
	clear(wire)

	require.Equal(t, want, got)
	log := got.Events[0].(*models.Log)
	require.Equal(t, []byte{0xff, 0xfe, 0}, log.GetBody())
	require.Equal(t, "updated", log.GetIndices().Get("before"))
	require.True(t, log.GetIndices().Contains("empty"))
	var keys []string
	for key := range models.LogContentEntries(log.GetIndices()) {
		keys = append(keys, key)
	}
	require.Equal(t, []string{"before", models.BodyKey, "after", "empty"}, keys)
	require.Equal(t, "host.example", got.Group.Tags.Get("hostname"))
	require.Equal(t, "/var/log/app.log", got.Group.Metadata.Get("log.file.path"))
}

func TestDecodePipelineGroupEventsWireCompatibility(t *testing.T) {
	logs := marshalPipelineLogGroup(t, &protocol.LogEvent{Timestamp: 100})
	metrics, err := (&protocol.PipelineEventGroup{
		PipelineEvents: &protocol.PipelineEventGroup_Metrics{Metrics: &protocol.PipelineEventGroup_MetricEvents{
			Events: []*protocol.MetricEvent{{
				Name: []byte("cpu"), Value: &protocol.MetricEvent_UntypedSingleValue{
					UntypedSingleValue: &protocol.UntypedSingleValue{Value: 1.5},
				},
			}},
		}},
	}).Marshal()
	require.NoError(t, err)
	spans, err := (&protocol.PipelineEventGroup{
		PipelineEvents: &protocol.PipelineEventGroup_Spans{Spans: &protocol.PipelineEventGroup_SpanEvents{
			Events: []*protocol.SpanEvent{{Name: []byte("request"), TraceID: []byte("trace-id")}},
		}},
	}).Marshal()
	require.NoError(t, err)
	unknown := protowire.AppendTag(bytes.Clone(logs), 6, protowire.VarintType)
	unknown = protowire.AppendVarint(unknown, 42)

	for _, tc := range []struct {
		name string
		wire []byte
		fast bool
	}{
		{name: "empty log wrapper", wire: marshalPipelineLogGroup(t), fast: true},
		{name: "metrics", wire: metrics},
		{name: "spans", wire: spans},
		{name: "unknown field", wire: unknown},
		{name: "last oneof member wins", wire: append(bytes.Clone(logs), metrics...)},
		{name: "last repeated log wrapper wins", wire: append(bytes.Clone(logs), marshalPipelineLogGroup(t)...)},
		{name: "invalid metadata entry", wire: []byte{0x0a, 0x01, 0x0a, 0x1a, 0x00}},
		{name: "invalid log event", wire: []byte{0x1a, 0x03, 0x0a, 0x01, 0x80}},
		{name: "truncated group", wire: logs[:len(logs)-1]},
		{name: "missing event type"},
	} {
		t.Run(tc.name, func(t *testing.T) {
			require.Equal(t, tc.fast, decodePipelineLogGroup(tc.wire) != nil)
			want, wantErr := decodePipelineGroupEventsWithProtobuf(tc.wire)
			got, err := DecodePipelineGroupEvents(tc.wire)
			require.Equal(t, wantErr, err)
			require.Equal(t, want, got)
		})
	}
}

func marshalPipelineLogGroup(t testing.TB, logs ...*protocol.LogEvent) []byte {
	t.Helper()
	wire, err := (&protocol.PipelineEventGroup{
		Metadata: map[string][]byte{"log.file.path": []byte("/var/log/app.log")},
		Tags:     map[string][]byte{"hostname": []byte("host.example")},
		PipelineEvents: &protocol.PipelineEventGroup_Logs{
			Logs: &protocol.PipelineEventGroup_LogEvents{Events: logs},
		},
	}).Marshal()
	require.NoError(t, err)
	return wire
}

func decodePipelineGroupEventsWithProtobuf(wire []byte) (*models.PipelineGroupEvents, error) {
	var group protocol.PipelineEventGroup
	if err := group.Unmarshal(wire); err != nil {
		return nil, fmt.Errorf("%w: %w", ErrInvalidPipelineEventGroupWire, err)
	}
	return transferPBToPipelineGroupEvents(&group, true)
}

func BenchmarkDecodePipelineGroupEvents(b *testing.B) {
	for _, fieldCount := range []int{1, 6} {
		b.Run(fmt.Sprintf("128_logs_%d_fields", fieldCount), func(b *testing.B) {
			contents := []*protocol.LogEvent_Content{
				{Key: []byte(models.BodyKey), Value: bytes.Repeat([]byte("x"), 512)},
			}
			for i := 1; i < fieldCount; i++ {
				contents = append(contents, &protocol.LogEvent_Content{
					Key: []byte(fmt.Sprintf("field-%d", i)), Value: []byte("app log field value"),
				})
			}
			logs := make([]*protocol.LogEvent, 128)
			for i := range logs {
				logs[i] = &protocol.LogEvent{
					Timestamp:  1_700_000_000_000_000_000 + uint64(i),
					Level:      []byte("info"),
					FileOffset: uint64(i * 512),
					RawSize:    512,
					Contents:   contents,
				}
			}
			wire := marshalPipelineLogGroup(b, logs...)
			b.ReportAllocs()
			b.SetBytes(int64(len(wire)))
			for b.Loop() {
				_, err := DecodePipelineGroupEvents(wire)
				if err != nil {
					b.Fatal(err)
				}
			}
		})
	}
}

func TestTransferLogEventToPBPreservesContentOrderAndOwnership(t *testing.T) {
	body := []byte("body")
	contents := models.NewOrderedLogContents(3)
	contents.Add("prefix", "first")
	contents.Add(models.BodyKey, body)
	contents.Add("suffix", "last")

	pbLog, err := TransferLogEventToPB(&models.Log{Contents: contents})
	require.NoError(t, err)

	body[0] = 'B'
	require.Equal(t, []*protocol.LogEvent_Content{
		{Key: []byte("prefix"), Value: []byte("first")},
		{Key: []byte(models.BodyKey), Value: []byte("body")},
		{Key: []byte("suffix"), Value: []byte("last")},
	}, pbLog.Contents)
}

func TestTransferSpanEventRoundTrip(t *testing.T) {
	tags := models.NewTagsWithKeyValues("service", "svc")
	innerEvents := []*models.SpanEvent{{
		Timestamp: 1500,
		Name:      "evt",
		Tags:      models.NewTagsWithKeyValues("ekey", "eval"),
	}}
	links := []*models.SpanLink{{
		TraceID:    "lt",
		SpanID:     "ls",
		TraceState: "lstate",
		Tags:       models.NewTagsWithKeyValues("lkey", "lval"),
	}}
	span := models.NewSpan("op", "trace-1", "span-1", models.SpanKindClient, 1000, 2000, tags, innerEvents, links)
	span.ParentSpanID = "parent-1"
	span.TraceState = "state"
	span.Status = models.StatusCodeOK

	pbSpan, err := TransferSpanEventToPB(span)
	require.NoError(t, err)

	restored, err := TransferPBToSpanEvent(pbSpan)
	require.NoError(t, err)
	assert.Equal(t, "trace-1", restored.GetTraceID())
	assert.Equal(t, "span-1", restored.GetSpanID())
	assert.Equal(t, "parent-1", restored.GetParentSpanID())
	assert.Equal(t, models.SpanKindClient, restored.GetKind())
	assert.Equal(t, models.StatusCodeOK, restored.GetStatus())
	assert.Equal(t, 1, len(restored.GetEvents()))
	assert.Equal(t, "evt", restored.GetEvents()[0].Name)
}

func TestTransferPBToPipelineGroupEvents_Metric(t *testing.T) {
	tags := models.NewTags()
	multiValue := models.NewMetricMultiValueWithMap(map[string]float64{"cpu": 0.3})
	metric := models.NewMultiValuesMetric("agent", models.MetricTypeUntyped, tags, 99, multiValue.Values)
	groupInfo := models.NewGroup(models.NewMetadata(), models.NewTagsWithKeyValues("env", "test"))
	pbGroup, err := TransferPipelineEventGroupToPB(groupInfo, []models.PipelineEvent{metric})
	require.NoError(t, err)

	got, err := TransferPBToPipelineGroupEvents(pbGroup)
	require.NoError(t, err)
	assert.Equal(t, "test", got.Group.Tags.Get("env"))
	require.Len(t, got.Events, 1)
	m, ok := got.Events[0].(*models.Metric)
	require.True(t, ok)
	assert.True(t, m.GetValue().IsMultiValues())
}

func TestTransferPBToPipelineGroupEvents_Log(t *testing.T) {
	log := models.NewSimpleLog([]byte("body"), models.NewTags(), 100)
	groupInfo := models.NewGroup(models.NewMetadata(), models.NewTags())
	pbGroup, err := TransferPipelineEventGroupToPB(groupInfo, []models.PipelineEvent{log})
	require.NoError(t, err)

	got, err := TransferPBToPipelineGroupEvents(pbGroup)
	require.NoError(t, err)
	require.Len(t, got.Events, 1)
	_, ok := got.Events[0].(*models.Log)
	assert.True(t, ok)
}

func TestTransferPBToPipelineGroupEvents_Span(t *testing.T) {
	span := models.NewSpan("op", "t", "s", models.SpanKindServer, 1, 2, models.NewTags(), nil, nil)
	groupInfo := models.NewGroup(models.NewMetadata(), models.NewTags())
	pbGroup, err := TransferPipelineEventGroupToPB(groupInfo, []models.PipelineEvent{span})
	require.NoError(t, err)

	got, err := TransferPBToPipelineGroupEvents(pbGroup)
	require.NoError(t, err)
	require.Len(t, got.Events, 1)
	s, ok := got.Events[0].(*models.Span)
	require.True(t, ok)
	assert.Equal(t, "op", s.GetName())
	assert.Equal(t, models.SpanKindServer, s.GetKind())
}

func TestTransferMetricEventSingleValueRoundTrip(t *testing.T) {
	tags := models.NewTagsWithKeyValues("host", "node-a")
	metric := models.NewSingleValueMetric("cpu", models.MetricTypeUntyped, tags, 1234567890, 0.5)

	pbMetric, err := TransferMetricEventToPB(metric)
	require.NoError(t, err)

	restored, err := TransferPBToMetricEvent(pbMetric)
	require.NoError(t, err)
	assert.Equal(t, "cpu", restored.GetName())
	assert.True(t, restored.GetValue().IsSingleValue())
	assert.Equal(t, 0.5, restored.GetValue().GetSingleValue())
	assert.Equal(t, "node-a", restored.GetTags().Get("host"))
}

// Group-level tags and metadata must survive the PB round trip, since the
// self-monitor bridge relies on metadata (e.g. internal data type) downstream.
func TestTransferPBToPipelineGroupEvents_TagsAndMetadata(t *testing.T) {
	metric := models.NewSingleValueMetric("m", models.MetricTypeUntyped, models.NewTags(), 1, 1.0)
	groupInfo := models.NewGroup(
		models.NewMetadataWithKeyValues("internal.data.type", "self_monitor"),
		models.NewTagsWithKeyValues("env", "prod"),
	)
	pbGroup, err := TransferPipelineEventGroupToPB(groupInfo, []models.PipelineEvent{metric})
	require.NoError(t, err)

	got, err := TransferPBToPipelineGroupEvents(pbGroup)
	require.NoError(t, err)
	assert.Equal(t, "prod", got.Group.Tags.Get("env"))
	assert.Equal(t, "self_monitor", got.Group.Metadata.Get("internal.data.type"))
}

func TestTransferPipelineEventGroupToPB_EmptyEvents(t *testing.T) {
	groupInfo := models.NewGroup(models.NewMetadata(), models.NewTags())
	_, err := TransferPipelineEventGroupToPB(groupInfo, nil)
	assert.Error(t, err)
}

func TestTransferPBToPipelineGroupEvents_Nil(t *testing.T) {
	_, err := TransferPBToPipelineGroupEvents(nil)
	assert.Error(t, err)
}

func TestTransferPBToEvent_Nil(t *testing.T) {
	_, err := TransferPBToLogEvent(nil)
	assert.Error(t, err)
	_, err = TransferPBToMetricEvent(nil)
	assert.Error(t, err)
	_, err = TransferPBToSpanEvent(nil)
	assert.Error(t, err)
}

func TestTransferPBToMetricEvent_UnsupportedValue(t *testing.T) {
	// A metric event with no value set must be rejected rather than silently
	// producing an empty metric.
	_, err := TransferPBToMetricEvent(&protocol.MetricEvent{Name: []byte("m")})
	assert.Error(t, err)
}
