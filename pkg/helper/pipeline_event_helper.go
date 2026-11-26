package helper

import (
	"errors"
	"fmt"
	"slices"
	"strings"

	"google.golang.org/protobuf/encoding/protowire"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

func TransferLogEventToPB(log *models.Log) (*protocol.LogEvent, error) {
	logEvent := new(protocol.LogEvent)
	logEvent.Timestamp = log.GetTimestamp()
	fields := log.GetIndices()
	contents := make([]protocol.LogEvent_Content, 0, fields.Len())
	for k, v := range models.LogContentEntries(fields) {
		var val string
		switch tv := v.(type) {
		case string:
			val = tv
		case []byte:
			val = string(tv)
		default:
			return nil, fmt.Errorf("unsupported log content type for key %s: %T", k, v)
		}
		contents = append(contents, protocol.LogEvent_Content{
			Key:   util.ZeroCopyStringToBytes(k),
			Value: util.ZeroCopyStringToBytes(val),
		})
	}
	logEvent.Contents = make([]*protocol.LogEvent_Content, len(contents))
	for i := range contents {
		logEvent.Contents[i] = &contents[i]
	}
	logEvent.Level = util.ZeroCopyStringToBytes(log.GetLevel())
	logEvent.FileOffset = log.GetOffset()
	logEvent.RawSize = log.GetRawSize()
	return logEvent, nil
}

func TransferMetricEventToPB(metric *models.Metric) (*protocol.MetricEvent, error) {
	var metricEvent protocol.MetricEvent
	metricEvent.Timestamp = metric.GetTimestamp()
	metricEvent.Name = util.ZeroCopyStringToBytes(metric.GetName())
	switch {
	case metric.GetValue().IsSingleValue():
		metricEvent.Value = &protocol.MetricEvent_UntypedSingleValue{
			UntypedSingleValue: &protocol.UntypedSingleValue{Value: metric.Value.GetSingleValue()},
		}
	case metric.GetValue().IsMultiValues():
		multiPb := &protocol.UntypedMultiDoubleValues{Values: make(map[string]*protocol.UntypedMultiDoubleValue)}
		for k, v := range metric.GetValue().GetMultiValues().Iterator() {
			multiPb.Values[k] = &protocol.UntypedMultiDoubleValue{
				MetricType: toPBMetricType(models.MetricTypeUntyped),
				Value:      v,
			}
		}
		metricEvent.Value = &protocol.MetricEvent_UntypedMultiDoubleValues{UntypedMultiDoubleValues: multiPb}
	default:
		return nil, fmt.Errorf("unsupported metric value type")
	}
	metricEvent.Tags = make(map[string][]byte, metric.GetTags().Len())
	for k, v := range metric.GetTags().Iterator() {
		metricEvent.Tags[k] = util.ZeroCopyStringToBytes(v)
	}
	return &metricEvent, nil
}

func toPBMetricType(metricType models.MetricType) protocol.UntypedValueMetricType {
	if metricType == models.MetricTypeGauge {
		return protocol.UntypedValueMetricType_METRIC_TYPE_GAUGE
	}
	return protocol.UntypedValueMetricType_METRIC_TYPE_COUNTER
}

func TransferPBToLogEvent(src *protocol.LogEvent) (*models.Log, error) {
	return transferPBToLogEvent(src, false)
}

func transferPBToLogEvent(src *protocol.LogEvent, takeBody bool) (*models.Log, error) {
	if src == nil {
		return nil, fmt.Errorf("nil log event")
	}
	log := &models.Log{
		Timestamp: src.Timestamp,
		Tags:      models.NewTags(),
		Offset:    src.FileOffset,
		RawSize:   src.RawSize,
	}
	if len(src.Level) > 0 {
		log.Level = string(src.Level)
	}
	if len(src.Contents) > 0 {
		log.Contents = models.NewOrderedLogContents(len(src.Contents))
	}
	for _, content := range src.Contents {
		addPipelineLogContent(log, content.Key, content.Value, takeBody)
	}
	return log, nil
}

func addPipelineLogContent(log *models.Log, key, value []byte, takeBody bool) {
	if string(key) != models.BodyKey {
		log.Contents.Add(string(key), string(value))
		return
	}
	// Empty protobuf values do not insert or overwrite the body field.
	if len(value) == 0 {
		return
	}
	if !takeBody {
		value = slices.Clone(value)
	}
	models.SetOwnedLogBody(log, value)
}

func TransferPBToMetricEvent(src *protocol.MetricEvent) (*models.Metric, error) {
	if src == nil {
		return nil, fmt.Errorf("nil metric event")
	}
	tags := models.NewTags()
	for k, v := range src.Tags {
		tags.Add(k, string(v))
	}
	switch value := src.Value.(type) {
	case *protocol.MetricEvent_UntypedSingleValue:
		return models.NewSingleValueMetric(string(src.Name), models.MetricTypeUntyped, tags, int64(src.Timestamp), value.UntypedSingleValue.Value), nil
	case *protocol.MetricEvent_UntypedMultiDoubleValues:
		multiValue := models.NewMetricMultiValue()
		for k, v := range value.UntypedMultiDoubleValues.Values {
			multiValue.Add(k, v.Value)
		}
		return models.NewMultiValuesMetric(string(src.Name), models.MetricTypeUntyped, tags, int64(src.Timestamp), multiValue.Values), nil
	default:
		return nil, fmt.Errorf("unsupported metric value type")
	}
}

// ErrInvalidPipelineEventGroupWire indicates a protobuf unmarshal failure.
var ErrInvalidPipelineEventGroupWire = errors.New("invalid pipeline event group wire data")

// DecodePipelineGroupEvents converts borrowed C++ wire data to Go-owned events.
func DecodePipelineGroupEvents(wire []byte) (*models.PipelineGroupEvents, error) {
	if group := decodePipelineLogGroup(wire); group != nil {
		return group, nil
	}

	var group protocol.PipelineEventGroup
	if err := group.Unmarshal(wire); err != nil {
		return nil, fmt.Errorf("%w: %w", ErrInvalidPipelineEventGroupWire, err)
	}
	return transferPBToPipelineGroupEvents(&group, true)
}

// decodePipelineLogGroup decodes the wire layout from
// protobuf_public/models/pipeline_event_group.proto and log_event.proto. Each
// accepted field has its usual one-byte tag. The generated decoder handles
// other event types, unknown fields, repeated oneof members, and wire errors.
func decodePipelineLogGroup(wire []byte) *models.PipelineGroupEvents {
	const (
		metadataTag = 1<<3 | byte(protowire.BytesType)
		tagsTag     = 2<<3 | byte(protowire.BytesType)
		logsTag     = 3<<3 | byte(protowire.BytesType)
		logEventTag = 1<<3 | byte(protowire.BytesType)
	)

	// Read the group framing before allocating events or metadata. Metric and
	// span groups take the generated decode path.
	var logs []byte
	for remaining := wire; len(remaining) > 0; {
		tag := remaining[0]
		if tag != metadataTag && tag != tagsTag && tag != logsTag {
			return nil
		}
		data, n := protowire.ConsumeBytes(remaining[1:])
		if n < 0 {
			return nil
		}
		if tag == logsTag {
			if logs != nil {
				return nil
			}
			logs = data
		}
		remaining = remaining[1+n:]
	}
	if logs == nil {
		return nil
	}

	// Count the log frames to allocate the event list once. This pass only
	// reads length prefixes and skips each log payload.
	count := 0
	for remaining := logs; len(remaining) > 0; count++ {
		if remaining[0] != logEventTag {
			return nil
		}
		_, n := protowire.ConsumeBytes(remaining[1:])
		if n < 0 {
			return nil
		}
		remaining = remaining[1+n:]
	}
	events := make([]models.PipelineEvent, count)
	var fields [8]pipelineLogWireField
	var names nativeLogNames
	for i := range events {
		data, n := protowire.ConsumeBytes(logs[1:])
		log := decodePipelineLog(data, fields[:0], &names)
		if log == nil {
			return nil
		}
		events[i] = log
		logs = logs[1+n:]
	}

	group := models.NewGroup(models.NewMetadata(), models.NewTags())
	for len(wire) > 0 {
		tag := wire[0]
		data, n := protowire.ConsumeBytes(wire[1:])
		wire = wire[1+n:]
		if tag == logsTag {
			continue
		}
		field, ok := decodePipelineLogWireField(data)
		if !ok {
			return nil
		}
		if tag == tagsTag {
			group.Tags.Add(string(field.key), string(field.value))
		} else {
			group.Metadata.Add(string(field.key), string(field.value))
		}
	}
	return &models.PipelineGroupEvents{Group: group, Events: events}
}

func decodePipelineLog(wire []byte, fields []pipelineLogWireField, names *nativeLogNames) *models.Log {
	const (
		timestampTag  = 1<<3 | byte(protowire.VarintType)
		contentsTag   = 2<<3 | byte(protowire.BytesType)
		levelTag      = 3<<3 | byte(protowire.BytesType)
		fileOffsetTag = 4<<3 | byte(protowire.VarintType)
		rawSizeTag    = 5<<3 | byte(protowire.VarintType)
	)

	var timestamp, offset, rawSize uint64
	var level []byte
	for len(wire) > 0 {
		tag := wire[0]
		wire = wire[1:]
		switch tag {
		case timestampTag, fileOffsetTag, rawSizeTag:
			value, n := protowire.ConsumeVarint(wire)
			if n < 0 {
				return nil
			}
			switch tag {
			case timestampTag:
				timestamp = value
			case fileOffsetTag:
				offset = value
			case rawSizeTag:
				rawSize = value
			}
			wire = wire[n:]
		case contentsTag, levelTag:
			value, n := protowire.ConsumeBytes(wire)
			if n < 0 {
				return nil
			}
			if tag == levelTag {
				level = value
			} else {
				field, ok := decodePipelineLogWireField(value)
				if !ok {
					return nil
				}
				fields = append(fields, field)
			}
			wire = wire[n:]
		default:
			return nil
		}
	}

	log := models.NewLogWithOrderedContents(len(fields))
	log.Level = names.levelName(level)
	log.Timestamp = timestamp
	log.Offset = offset
	log.RawSize = rawSize
	switch len(fields) {
	case 0:
	case 1:
		addPipelineLogContent(log, fields[0].key, fields[0].value, false)
	default:
		addOwnedNativeLogContents(log, fields, names)
	}
	return log
}

// Cache the first small row schema and level of a native group. A log with
// different keys still packs its own strings, so dynamic fields do not expand
// the cache or create a per-key allocation.
type nativeLogNames struct {
	keys  [8]string
	count int
	level string
}

func (n *nativeLogNames) levelName(value []byte) string {
	if n.level == string(value) {
		return n.level
	}
	text := string(value)
	if n.level == "" {
		n.level = text
	}
	return text
}

func (n *nativeLogNames) matchingKeys(fields []pipelineLogWireField) []string {
	if len(fields) > len(n.keys) {
		return nil
	}
	if n.count == 0 {
		n.count = len(fields)
		for i, field := range fields {
			n.keys[i] = string(field.key)
		}
	} else {
		if n.count != len(fields) {
			return nil
		}
		for i, field := range fields {
			if n.keys[i] != string(field.key) {
				return nil
			}
		}
	}
	return n.keys[:n.count]
}

// Small immutable values from the same native log can share Go-owned string
// storage. Keep the mutable body separate so dropping it releases its buffer.
func addOwnedNativeLogContents(log *models.Log, fields []pipelineLogWireField, names *nativeLogNames) {
	keys := names.matchingKeys(fields)
	immutableSize := 0
	for _, field := range fields {
		if string(field.key) != models.BodyKey {
			immutableSize += len(field.value)
			if keys == nil {
				immutableSize += len(field.key)
			}
		}
	}
	// A surviving small field should not pin a large deleted value.
	const maxPackedLogFields = 4 * 1024
	if immutableSize > maxPackedLogFields {
		for _, field := range fields {
			addPipelineLogContent(log, field.key, field.value, false)
		}
		return
	}
	var packed strings.Builder
	packed.Grow(immutableSize)
	for _, field := range fields {
		if string(field.key) != models.BodyKey {
			if keys == nil {
				packed.Write(field.key)
			}
			packed.Write(field.value)
		}
	}
	owned := packed.String()
	for i, field := range fields {
		if string(field.key) == models.BodyKey {
			addPipelineLogContent(log, field.key, field.value, false)
			continue
		}
		var key string
		if keys == nil {
			key = owned[:len(field.key)]
			owned = owned[len(field.key):]
		} else {
			key = keys[i]
		}
		log.Contents.Add(key, owned[:len(field.value)])
		owned = owned[len(field.value):]
	}
}

type pipelineLogWireField struct {
	key, value []byte
}

func decodePipelineLogWireField(wire []byte) (pipelineLogWireField, bool) {
	const (
		keyTag   = 1<<3 | byte(protowire.BytesType)
		valueTag = 2<<3 | byte(protowire.BytesType)
	)
	var field pipelineLogWireField
	for len(wire) > 0 {
		tag := wire[0]
		value, n := protowire.ConsumeBytes(wire[1:])
		if n < 0 {
			return field, false
		}
		switch tag {
		case keyTag:
			field.key = value
		case valueTag:
			field.value = value
		default:
			return field, false
		}
		wire = wire[1+n:]
	}
	return field, true
}

func TransferPBToPipelineGroupEvents(src *protocol.PipelineEventGroup) (*models.PipelineGroupEvents, error) {
	return transferPBToPipelineGroupEvents(src, false)
}

func transferPBToPipelineGroupEvents(src *protocol.PipelineEventGroup, takeLogBodies bool) (*models.PipelineGroupEvents, error) {
	if src == nil {
		return nil, fmt.Errorf("nil pipeline event group")
	}
	group := models.NewGroup(models.NewMetadata(), models.NewTags())
	for k, v := range src.Tags {
		group.Tags.Add(k, string(v))
	}
	for k, v := range src.Metadata {
		group.Metadata.Add(k, string(v))
	}

	var events []models.PipelineEvent
	switch pipelineEvents := src.PipelineEvents.(type) {
	case *protocol.PipelineEventGroup_Logs:
		events = make([]models.PipelineEvent, 0, len(pipelineEvents.Logs.Events))
		for _, logEvent := range pipelineEvents.Logs.Events {
			log, err := transferPBToLogEvent(logEvent, takeLogBodies)
			if err != nil {
				return nil, err
			}
			events = append(events, log)
		}
	case *protocol.PipelineEventGroup_Metrics:
		events = make([]models.PipelineEvent, 0, len(pipelineEvents.Metrics.Events))
		for _, metricEvent := range pipelineEvents.Metrics.Events {
			metric, err := TransferPBToMetricEvent(metricEvent)
			if err != nil {
				return nil, err
			}
			events = append(events, metric)
		}
	case *protocol.PipelineEventGroup_Spans:
		events = make([]models.PipelineEvent, 0, len(pipelineEvents.Spans.Events))
		for _, spanEvent := range pipelineEvents.Spans.Events {
			span, err := TransferPBToSpanEvent(spanEvent)
			if err != nil {
				return nil, err
			}
			events = append(events, span)
		}
	default:
		return nil, fmt.Errorf("unsupported pipeline event group type")
	}

	return &models.PipelineGroupEvents{
		Group:  group,
		Events: events,
	}, nil
}

func TransferPBToSpanEvent(src *protocol.SpanEvent) (*models.Span, error) {
	if src == nil {
		return nil, fmt.Errorf("nil span event")
	}
	tags := models.NewTags()
	for k, v := range src.Tags {
		tags.Add(k, string(v))
	}
	innerEvents := make([]*models.SpanEvent, 0, len(src.Events))
	for _, e := range src.Events {
		eventTags := models.NewTags()
		for k, v := range e.Tags {
			eventTags.Add(k, string(v))
		}
		innerEvents = append(innerEvents, &models.SpanEvent{
			Timestamp: int64(e.Timestamp),
			Name:      string(e.Name),
			Tags:      eventTags,
		})
	}
	links := make([]*models.SpanLink, 0, len(src.Links))
	for _, l := range src.Links {
		linkTags := models.NewTags()
		for k, v := range l.Tags {
			linkTags.Add(k, string(v))
		}
		links = append(links, &models.SpanLink{
			TraceID:    string(l.TraceID),
			SpanID:     string(l.SpanID),
			TraceState: string(l.TraceState),
			Tags:       linkTags,
		})
	}
	span := models.NewSpan(
		string(src.Name),
		string(src.TraceID),
		string(src.SpanID),
		models.SpanKind(src.Kind),
		src.StartTime,
		src.EndTime,
		tags,
		innerEvents,
		links,
	)
	span.ParentSpanID = string(src.ParentSpanID)
	span.TraceState = string(src.TraceState)
	span.Status = models.StatusCode(src.Status)
	if src.Timestamp != 0 {
		span.ObservedTimestamp = src.Timestamp
	}
	return span, nil
}

func TransferSpanEventToPB(span *models.Span) (*protocol.SpanEvent, error) {
	var spanEvent protocol.SpanEvent
	spanEvent.Timestamp = span.GetTimestamp()
	spanEvent.TraceID = util.ZeroCopyStringToBytes(span.GetTraceID())
	spanEvent.SpanID = util.ZeroCopyStringToBytes(span.GetSpanID())
	spanEvent.TraceState = util.ZeroCopyStringToBytes(span.GetTraceState())
	spanEvent.ParentSpanID = util.ZeroCopyStringToBytes(span.GetParentSpanID())
	spanEvent.Name = util.ZeroCopyStringToBytes(span.GetName())
	spanEvent.Kind = protocol.SpanEvent_SpanKind(span.GetKind())
	spanEvent.StartTime = span.GetStartTime()
	spanEvent.EndTime = span.GetEndTime()
	spanEvent.Tags = reportBytes(span.GetTags())
	spanEvent.Events = make([]*protocol.SpanEvent_InnerEvent, 0, len(span.GetEvents()))
	for _, srcEvent := range span.GetEvents() {
		if srcEvent == nil {
			return nil, fmt.Errorf("nil span inner event")
		}
		dstEvent := protocol.SpanEvent_InnerEvent{
			Timestamp: uint64(srcEvent.Timestamp),
			Name:      util.ZeroCopyStringToBytes(srcEvent.Name),
			Tags:      reportBytes(srcEvent.Tags),
		}
		spanEvent.Events = append(spanEvent.Events, &dstEvent)
	}
	spanEvent.Links = make([]*protocol.SpanEvent_SpanLink, 0, len(span.GetLinks()))
	for _, srcLink := range span.GetLinks() {
		if srcLink == nil {
			return nil, fmt.Errorf("nil span link")
		}
		dstLink := protocol.SpanEvent_SpanLink{
			TraceID:    util.ZeroCopyStringToBytes(srcLink.TraceID),
			SpanID:     util.ZeroCopyStringToBytes(srcLink.SpanID),
			TraceState: util.ZeroCopyStringToBytes(srcLink.TraceState),
			Tags:       reportBytes(srcLink.Tags),
		}
		spanEvent.Links = append(spanEvent.Links, &dstLink)
	}
	spanEvent.Status = protocol.SpanEvent_StatusCode(span.GetStatus())
	return &spanEvent, nil
}

func TransferPipelineEventGroupToPB(groupInfo *models.GroupInfo, events []models.PipelineEvent) (*protocol.PipelineEventGroup, error) {
	var pipelineEventGroup protocol.PipelineEventGroup
	if len(events) == 0 {
		return nil, fmt.Errorf("events is empty")
	}

	eventType := events[0].GetType()
	switch eventType {
	case models.EventTypeLogging:
		logEvents := make([]*protocol.LogEvent, 0, len(events))
		for _, event := range events {
			if logSrc, ok := event.(*models.Log); ok {
				logDst, _ := TransferLogEventToPB(logSrc)
				logEvents = append(logEvents, logDst)
			}
		}
		pipelineEventGroup.PipelineEvents = &protocol.PipelineEventGroup_Logs{Logs: &protocol.PipelineEventGroup_LogEvents{Events: logEvents}}
	case models.EventTypeMetric:
		metricEvents := make([]*protocol.MetricEvent, 0, len(events))
		for _, event := range events {
			if metricSrc, ok := event.(*models.Metric); ok {
				metricDst, _ := TransferMetricEventToPB(metricSrc)
				metricEvents = append(metricEvents, metricDst)
			}
		}
		pipelineEventGroup.PipelineEvents = &protocol.PipelineEventGroup_Metrics{Metrics: &protocol.PipelineEventGroup_MetricEvents{Events: metricEvents}}
	case models.EventTypeSpan:
		spanEvents := make([]*protocol.SpanEvent, 0, len(events))
		for _, event := range events {
			if spanSrc, ok := event.(*models.Span); ok {
				spanDst, _ := TransferSpanEventToPB(spanSrc)
				spanEvents = append(spanEvents, spanDst)
			}
		}
		pipelineEventGroup.PipelineEvents = &protocol.PipelineEventGroup_Spans{Spans: &protocol.PipelineEventGroup_SpanEvents{Events: spanEvents}}
	}

	pipelineEventGroup.Tags = make(map[string][]byte, groupInfo.Tags.Len())
	for k, v := range groupInfo.Tags.Iterator() {
		pipelineEventGroup.Tags[k] = util.ZeroCopyStringToBytes(v)
	}
	pipelineEventGroup.Metadata = make(map[string][]byte, groupInfo.Metadata.Len())
	for k, v := range groupInfo.Metadata.Iterator() {
		pipelineEventGroup.Metadata[k] = util.ZeroCopyStringToBytes(v)
	}
	return &pipelineEventGroup, nil
}
