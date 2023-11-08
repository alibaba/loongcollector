package opentelemetry

import (
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"strconv"
	"time"

	"go.opentelemetry.io/collector/pdata/pcommon"
	"go.opentelemetry.io/collector/pdata/ptrace"
	v1Common "go.opentelemetry.io/proto/otlp/common/v1"
	v1Resource "go.opentelemetry.io/proto/otlp/resource/v1"
	v1 "go.opentelemetry.io/proto/otlp/trace/v1"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

const (
	traceIDField         = "traceID"
	spanIDField          = "spanID"
	parentSpanIDField    = "parentSpanID"
	nameField            = "name"
	kindField            = "kind"
	linksField           = "links"
	timeField            = "time"
	startTimeField       = "start"
	endTimeField         = "end"
	traceStateField      = "traceState"
	durationField        = "duration"
	attributeField       = "attribute"
	statusCodeField      = "statusCode"
	statusMessageField   = "statusMessage"
	logsField            = "logs"
	slsLogTimeUnixNano   = "timeUnixNano"
	slsLogSeverityNumber = "severityNumber"
	slsLogSeverityText   = "severityText"
	slsLogName           = "name"
	slsLogContent        = "content"
	slsLogAttribute      = "attribute"
	slsLogFlags          = "flags"
	slsLogResource       = "resource"
	slsLogHost           = "host"
	slsLogService        = "service"
	// shortcut for "otlp.instrumentation.library.name" "otlp.instrumentation.library.version"
	slsLogInstrumentationName    = "otlp.name"
	slsLogInstrumentationVersion = "otlp.version"
)

// traceDataToLogService translates trace data into the LogService format.
func traceDataToLogServiceData(td ptrace.Traces) ([]*protocol.Log, int) {
	var slsLogs []*protocol.Log
	resourceSpansSlice := td.ResourceSpans()
	for i := 0; i < resourceSpansSlice.Len(); i++ {
		logs := resourceSpansToLogServiceData(resourceSpansSlice.At(i))
		slsLogs = append(slsLogs, logs...)
	}
	return slsLogs, 0
}

func resourceToLogContents(resource pcommon.Resource) []*protocol.Log_Content {
	logContents := make([]*protocol.Log_Content, 3)
	attrs := resource.Attributes()
	if hostName, ok := attrs.Get("host.name"); ok {
		logContents[0] = &protocol.Log_Content{
			Key:   slsLogHost,
			Value: hostName.AsString(),
		}
		attrs.Remove("host.name")
	} else {
		logContents[0] = &protocol.Log_Content{
			Key:   slsLogHost,
			Value: "",
		}
	}

	if serviceName, ok := attrs.Get("service.name"); ok {
		logContents[1] = &protocol.Log_Content{
			Key:   slsLogService,
			Value: serviceName.AsString(),
		}
		attrs.Remove("service.name")
	} else {
		logContents[1] = &protocol.Log_Content{
			Key:   slsLogService,
			Value: "",
		}
	}

	attributeBuffer, _ := json.Marshal(attrs.AsRaw())
	logContents[2] = &protocol.Log_Content{
		Key:   slsLogResource,
		Value: string(attributeBuffer),
	}

	return logContents
}

func instrumentationLibraryToLogContents(instrumentationLibrary pcommon.InstrumentationScope) []*protocol.Log_Content {
	logContents := make([]*protocol.Log_Content, 2)
	logContents[0] = &protocol.Log_Content{
		Key:   slsLogInstrumentationName,
		Value: instrumentationLibrary.Name(),
	}
	logContents[1] = &protocol.Log_Content{
		Key:   slsLogInstrumentationVersion,
		Value: instrumentationLibrary.Version(),
	}
	return logContents
}

func resourceSpansToLogServiceData(resourceSpans ptrace.ResourceSpans) []*protocol.Log {
	resourceContents := resourceToLogContents(resourceSpans.Resource())
	// v0.58.0 Beta: Remove the InstrumentationLibrary to Scope translation (part of transition to OTLP 0.19). (#5819)
	//  - This has a side effect that when sending JSON encoded telemetry using OTLP proto <= 0.15.0, telemetry will be dropped.
	var slsLogs []*protocol.Log
	scopeSpans := resourceSpans.ScopeSpans()
	for i := 0; i < scopeSpans.Len(); i++ {
		scopeSpan := resourceSpans.ScopeSpans().At(i)
		instrumentationLibraryContents := instrumentationLibraryToLogContents(scopeSpan.Scope())
		spans := scopeSpan.Spans()
		for j := 0; j < spans.Len(); j++ {
			if slsLog := spanToLogServiceData(spans.At(j), resourceContents, instrumentationLibraryContents); slsLog != nil {
				slsLogs = append(slsLogs, slsLog)
			}
		}
	}

	return slsLogs
}

func spanToLogServiceData(span ptrace.Span, resourceContents, instrumentationLibraryContents []*protocol.Log_Content) *protocol.Log {
	endTimeNano := span.EndTimestamp()
	if endTimeNano == 0 {
		endTimeNano = pcommon.Timestamp(time.Now().UnixNano())
	}
	slsLog := &protocol.Log{
		Time: uint32(endTimeNano / 1000 / 1000 / 1000),
	}
	// pre alloc, refine if logContent's len > 16
	preAllocCount := 16
	slsLog.Contents = make([]*protocol.Log_Content, 0, preAllocCount+len(resourceContents)+len(instrumentationLibraryContents))
	contentsBuffer := make([]protocol.Log_Content, 0, preAllocCount)

	slsLog.Contents = append(slsLog.Contents, resourceContents...)
	slsLog.Contents = append(slsLog.Contents, instrumentationLibraryContents...)

	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   traceIDField,
		Value: span.TraceID().String(),
	})
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   spanIDField,
		Value: span.SpanID().String(),
	})
	// if ParentSpanID is not valid, the return "", it is compatible for log service
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   parentSpanIDField,
		Value: span.ParentSpanID().String(),
	})

	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   kindField,
		Value: spanKindToShortString(span.Kind()),
	})
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   nameField,
		Value: span.Name(),
	})

	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   linksField,
		Value: spanLinksToString(span.Links()),
	})
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   logsField,
		Value: eventsToString(span.Events()),
	})
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   traceStateField,
		Value: span.TraceState().AsRaw(),
	})
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   startTimeField,
		Value: strconv.FormatUint(uint64(span.StartTimestamp()/1000), 10),
	})
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   endTimeField,
		Value: strconv.FormatUint(uint64(endTimeNano/1000), 10),
	})
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   durationField,
		Value: strconv.FormatUint(uint64((endTimeNano-span.StartTimestamp())/1000), 10),
	})
	attributeMap := span.Attributes().AsRaw()
	attributeJSONBytes, _ := json.Marshal(attributeMap)
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   attributeField,
		Value: string(attributeJSONBytes),
	})

	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   statusCodeField,
		Value: statusCodeToShortString(span.Status().Code()),
	})

	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   statusMessageField,
		Value: span.Status().Message(),
	})

	for i := range contentsBuffer {
		slsLog.Contents = append(slsLog.Contents, &contentsBuffer[i])
	}
	return slsLog
}

func spanKindToShortString(kind ptrace.SpanKind) string {
	switch kind {
	case ptrace.SpanKindInternal:
		return "internal"
	case ptrace.SpanKindClient:
		return "client"
	case ptrace.SpanKindServer:
		return "server"
	case ptrace.SpanKindProducer:
		return "producer"
	case ptrace.SpanKindConsumer:
		return "consumer"
	default:
		return ""
	}
}

func statusCodeToShortString(code ptrace.StatusCode) string {
	switch code {
	case ptrace.StatusCodeError:
		return "ERROR"
	case ptrace.StatusCodeOk:
		return "OK"
	default:
		return "UNSET"
	}
}

func v1StatusCodeToShortString(code v1.Status_StatusCode) string {
	switch code {
	case 2:
		return "ERROR"
	case 1:
		return "OK"
	default:
		return "UNSET"
	}
}

func eventsToString(events ptrace.SpanEventSlice) string {
	eventArray := make([]map[string]interface{}, 0, events.Len())
	for i := 0; i < events.Len(); i++ {
		spanEvent := events.At(i)
		event := map[string]interface{}{}
		event[nameField] = spanEvent.Name()
		event[timeField] = spanEvent.Timestamp()
		event[attributeField] = spanEvent.Attributes().AsRaw()
		eventArray = append(eventArray, event)
	}
	eventArrayBytes, _ := json.Marshal(&eventArray)
	return string(eventArrayBytes)

}

func spanLinksToString(spanLinkSlice ptrace.SpanLinkSlice) string {
	linkArray := make([]map[string]interface{}, 0, spanLinkSlice.Len())
	for i := 0; i < spanLinkSlice.Len(); i++ {
		spanLink := spanLinkSlice.At(i)
		link := map[string]interface{}{}
		link[spanIDField] = spanLink.SpanID().String()
		link[traceIDField] = spanLink.TraceID().String()
		link[attributeField] = spanLink.Attributes().AsRaw()
		linkArray = append(linkArray, link)
	}
	linkArrayBytes, _ := json.Marshal(&linkArray)
	return string(linkArrayBytes)
}

// Special Add
func ConvertTrace(td ptrace.Traces) ([]*protocol.Log, int) {
	return traceDataToLogServiceData(td)
}

func ConvertResourceSpans(rs *v1.ResourceSpans, traceIDNeedDecode, spanIDNeedDecode, parentSpanIDNeedDecode bool) ([]*protocol.Log, error) {
	return convertResourceSpansToLogData(rs, traceIDNeedDecode, spanIDNeedDecode, parentSpanIDNeedDecode)
}

func convertResourceSpansToLogData(rs *v1.ResourceSpans, traceIDNeedDecode, spanIDNeedDecode, parentSpanIDNeedDecode bool) (slsLogs []*protocol.Log, e error) {
	resourceContents := v1ResourceToLogContents(rs.GetResource())
	scopeSpans := rs.GetScopeSpans()

	for _, span := range scopeSpans {
		instrumentationLibraryContents := v1InstrumentationLibraryToLogContents(span.GetScope())
		for _, s := range span.GetSpans() {
			if traceIDNeedDecode {
				if s.TraceId, e = hex.DecodeString(base64.StdEncoding.EncodeToString(s.GetTraceId())); e != nil {
					return nil, e
				}
			}

			if spanIDNeedDecode {
				if s.SpanId, e = hex.DecodeString(base64.StdEncoding.EncodeToString(s.GetSpanId())); e != nil {
					return nil, e
				}
			}

			if parentSpanIDNeedDecode && s.ParentSpanId != nil {
				if s.ParentSpanId, e = hex.DecodeString(base64.StdEncoding.EncodeToString(s.GetParentSpanId())); e != nil {
					return nil, e
				}
			}

			if slsLog := v1SpanToLogServiceData(s, resourceContents, instrumentationLibraryContents); slsLog != nil {
				slsLogs = append(slsLogs, slsLog)
			}
		}
	}

	return slsLogs, nil
}

func v1ResourceToLogContents(resource *v1Resource.Resource) []*protocol.Log_Content {
	logContents := make([]*protocol.Log_Content, 3)
	attrs := resource.GetAttributes()

	var hostfined = false
	var serviceName = false
	fields := map[string]interface{}{}

	for _, attr := range attrs {
		switch attr.Key {
		case "host.name":
			logContents[0] = &protocol.Log_Content{
				Key:   slsLogHost,
				Value: attr.Value.GetStringValue(),
			}
			hostfined = true
		case "service.name":
			logContents[1] = &protocol.Log_Content{
				Key:   slsLogService,
				Value: attr.Value.GetStringValue(),
			}
			serviceName = true
		default:
			fields[attr.Key] = attr.Value.GetStringValue()
		}
	}

	if !hostfined {
		logContents[0] = &protocol.Log_Content{
			Key:   slsLogHost,
			Value: "",
		}
	}

	if !serviceName {
		logContents[1] = &protocol.Log_Content{
			Key:   slsLogService,
			Value: "",
		}
	}

	attributeBuffer, _ := json.Marshal(fields)
	logContents[2] = &protocol.Log_Content{
		Key:   slsLogResource,
		Value: string(attributeBuffer),
	}

	return logContents
}

func v1InstrumentationLibraryToLogContents(instrumentationLibrary *v1Common.InstrumentationScope) []*protocol.Log_Content {
	logContents := make([]*protocol.Log_Content, 2)
	logContents[0] = &protocol.Log_Content{
		Key:   slsLogInstrumentationName,
		Value: instrumentationLibrary.GetName(),
	}
	logContents[1] = &protocol.Log_Content{
		Key:   slsLogInstrumentationVersion,
		Value: instrumentationLibrary.GetVersion(),
	}
	return logContents
}

func v1SpanToLogServiceData(span *v1.Span, resourceContents, instrumentationLibraryContents []*protocol.Log_Content) *protocol.Log {
	endTimeNano := span.GetEndTimeUnixNano()
	if endTimeNano == 0 {
		endTimeNano = uint64(time.Now().Unix() * 1000 * 1000 * 1000)
	}
	slsLog := &protocol.Log{
		Time: uint32(endTimeNano / 1000 / 1000 / 1000),
	}
	// pre alloc, refine if logContent's len > 16
	preAllocCount := 16
	slsLog.Contents = make([]*protocol.Log_Content, 0, preAllocCount+len(resourceContents)+len(instrumentationLibraryContents))
	contentsBuffer := make([]protocol.Log_Content, 0, preAllocCount)

	slsLog.Contents = append(slsLog.Contents, resourceContents...)
	slsLog.Contents = append(slsLog.Contents, instrumentationLibraryContents...)

	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   traceIDField,
		Value: hex.EncodeToString(span.GetTraceId()),
	})
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   spanIDField,
		Value: hex.EncodeToString(span.GetSpanId()),
	})
	// if ParentSpanID is not valid, the return "", it is compatible for log service
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   parentSpanIDField,
		Value: hex.EncodeToString(span.GetParentSpanId()),
	})

	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   kindField,
		Value: v1SpanKindToShortString(span.Kind),
	})
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   nameField,
		Value: span.Name,
	})

	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   linksField,
		Value: v1SpanLinksToString(span.GetLinks()),
	})
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   logsField,
		Value: v1EventsToString(span.GetEvents()),
	})
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   traceStateField,
		Value: span.TraceState,
	})
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   startTimeField,
		Value: strconv.FormatUint(span.StartTimeUnixNano/1000, 10),
	})
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   endTimeField,
		Value: strconv.FormatUint(endTimeNano/1000, 10),
	})
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   durationField,
		Value: strconv.FormatUint((endTimeNano-span.StartTimeUnixNano)/1000, 10),
	})
	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   attributeField,
		Value: keyValueToString(span.GetAttributes()),
	})

	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   statusCodeField,
		Value: v1StatusCodeToShortString(span.Status.Code),
	})

	contentsBuffer = append(contentsBuffer, protocol.Log_Content{
		Key:   statusMessageField,
		Value: span.Status.String(),
	})

	for i := range contentsBuffer {
		slsLog.Contents = append(slsLog.Contents, &contentsBuffer[i])
	}
	return slsLog
}

func v1SpanLinksToString(spanLinkSlice []*v1.Span_Link) string {
	linkArray := make([]map[string]interface{}, 0, len(spanLinkSlice))
	for _, link := range spanLinkSlice {
		linkMap := map[string]interface{}{}
		linkMap[spanIDField] = string(link.SpanId)
		linkMap[traceIDField] = string(link.TraceId)
		linkMap[attributeField] = keyValueToString(link.Attributes)
		linkArray = append(linkArray, linkMap)
	}
	linkArrayBytes, _ := json.Marshal(&linkArray)
	return string(linkArrayBytes)
}

func v1EventsToString(events []*v1.Span_Event) string {
	eventArray := make([]map[string]interface{}, 0, len(events))
	for _, event := range events {
		eventMap := map[string]interface{}{}
		eventMap[nameField] = event.Name
		eventMap[timeField] = event.TimeUnixNano
		eventMap[attributeField] = keyValueToString(event.Attributes)
		eventArray = append(eventArray, eventMap)
	}

	eventArrayBytes, _ := json.Marshal(&eventArray)
	return string(eventArrayBytes)
}

func keyValueToString(keyValues []*v1Common.KeyValue) string {
	var results = make(map[string]string)
	for _, keyValue := range keyValues {
		results[keyValue.Key] = anyValueToString(keyValue.Value)
	}

	var d []byte
	var e error
	if d, e = json.Marshal(results); e != nil {
		return ""
	}

	return string(d)
}

func v1SpanKindToShortString(kind v1.Span_SpanKind) string {
	switch int32(kind.Number()) {
	case int32(ptrace.SpanKindInternal):
		return "internal"
	case int32(ptrace.SpanKindClient):
		return "client"
	case int32(ptrace.SpanKindServer):
		return "server"
	case int32(ptrace.SpanKindProducer):
		return "producer"
	case int32(ptrace.SpanKindConsumer):
		return "consumer"
	default:
		return ""
	}
}
