// Copyright 2021 iLogtail Authors
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

package skywalkingv3

import (
	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/agent/v3"
	management "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/management/v3"

	"strconv"
	"strings"
)

type SampleStatus int

// OpenTracingSpanKind are possible values for TagSpanKind and match the OpenTracing
// conventions: https://github.com/opentracing/specification/blob/master/semantic_conventions.md
// These values are used for representing span kinds that have no
// equivalents in OpenCensus format. They are stored as values of TagSpanKind
const (
	OpenTracingSpanKindUnspecified = ""
	OpenTracingSpanKindClient      = "client"
	OpenTracingSpanKindServer      = "server"
	OpenTracingSpanKindConsumer    = "consumer"
	OpenTracingSpanKindProducer    = "producer"
	OpenTracingSpanKindInternal    = "internal"
)

const (
	skywalkingTopicKey = "mq.topic"
	skywalkingQueueKey = "mq.queue"
)

// map to open telemetry
var OtResourceMapping = map[string]string{
	"hostname":    AttributeHostName,
	"Process No.": AttributeProcessID,
	"OS Name":     AttributeOSType,
	"language":    AttributeTelemetrySDKLanguage,
}

var OtSpanTagsMapping = map[string]string{
	"url":         AttributeHTTPURL,
	"status_code": AttributeHTTPStatusCode,
	"db.type":     AttributeDBSystem,
	"db.instance": AttributeDBName,
	"mq.broker":   AttributeNetPeerName,
}

func ParseSegment(span *v3.SpanObject, segment *v3.SegmentObject, cache *ResourcePropertiesCache, mapping map[int32]string) *OtSpan {
	resource := cache.get(segment.Service, segment.ServiceInstance)
	if resource == nil {
		cache.put(segment.Service, segment.GetServiceInstance(), make(map[string]string))
		resource = cache.get(segment.Service, segment.ServiceInstance)
	}

	otSpan := NewOtSpan()
	otSpan.Resource = resource
	otSpan.Service = segment.Service
	otSpan.Host = resource[AttributeHostName]
	otSpan.Name = span.OperationName
	switch {
	case span.SpanLayer == v3.SpanLayer_MQ:
		if span.SpanType == v3.SpanType_Entry {
			otSpan.Kind = OpenTracingSpanKindConsumer
		} else if span.SpanType == v3.SpanType_Exit {
			otSpan.Kind = OpenTracingSpanKindProducer
		}
		mappingMessageSystemTag(span, otSpan, mapping)
	case span.SpanType == v3.SpanType_Entry:
		otSpan.Kind = OpenTracingSpanKindServer
	case span.SpanType == v3.SpanType_Exit:
		otSpan.Kind = OpenTracingSpanKindClient
	case span.SpanType == v3.SpanType_Local:
		otSpan.Kind = OpenTracingSpanKindInternal
	default:
		otSpan.Kind = OpenTracingSpanKindUnspecified
	}

	otSpan.TraceID = segment.TraceId
	otSpan.SpanID = segment.TraceSegmentId + "." + strconv.FormatInt(int64(span.SpanId), 10)
	if span.ParentSpanId < 0 {
		otSpan.ParentSpanID = ""
	} else {
		otSpan.ParentSpanID = segment.TraceSegmentId + "." + strconv.FormatInt(int64(span.ParentSpanId), 10)
	}
	otSpan.Logs = make([]map[string]string, len(span.Logs))
	for i, log := range span.Logs {
		logEvent := make(map[string]string)
		logEvent["time"] = strconv.FormatInt(log.Time, 10)
		for _, kv := range log.Data {
			logEvent[kv.Key] = kv.Value
			// try extract status message, can be overwritten
			if kv.Key == "error.kind" && len(kv.Value) > 0 {
				otSpan.StatusMessage = kv.Value
			}
		}
		otSpan.Logs[i] = logEvent
	}
	otSpan.Links = make([]*OtSpanRef, len(span.Refs))
	if len(span.Refs) > 0 {
		for i, ref := range span.Refs {
			spanRef := &OtSpanRef{
				TraceID:    ref.TraceId,
				SpanID:     ref.ParentTraceSegmentId + "." + strconv.FormatInt(int64(ref.ParentSpanId), 10),
				TraceState: "",
				Attributes: nil,
			}
			otSpan.Links[i] = spanRef
		}
		otSpan.ParentSpanID = span.Refs[0].ParentTraceSegmentId + "." + strconv.FormatInt(int64(span.Refs[0].ParentSpanId), 10)
	}
	otSpan.Start = span.StartTime * 1000
	otSpan.End = span.EndTime * 1000
	otSpan.Duration = 1000 * (span.EndTime - span.StartTime)
	otSpan.Attribute = make(map[string]string, len(span.Tags)+8)
	if len(span.Peer) > 0 {
		hostport := strings.Split(span.Peer, ":")
		otSpan.Attribute[AttributeNetPeerIP] = hostport[0]
		if len(hostport) == 2 {
			otSpan.Attribute[AttributeNetPeerPort] = hostport[1]
		}
	}
	if len(span.Tags) > 0 {
		for _, tag := range span.Tags {
			otKey, ok := OtSpanTagsMapping[tag.Key]
			if ok {
				otSpan.Attribute[otKey] = tag.Value
			} else {
				if skywalkingTopicKey == tag.Key {
					otSpan.Attribute[AttributeMessagingDestinationKind] = "topic"
					otSpan.Attribute[AttributeMessagingDestination] = tag.Value
				} else if skywalkingQueueKey == tag.Key {
					otSpan.Attribute[AttributeMessagingDestinationKind] = "queue"
					otSpan.Attribute[AttributeMessagingDestination] = tag.Value
				}
				otSpan.Attribute[tag.Key] = tag.Value
			}
		}
	}
	if span.IsError {
		otSpan.StatusCode = StatusCodeError
	} else {
		otSpan.StatusCode = StatusCodeOk
	}

	switch {
	case span.SpanLayer == v3.SpanLayer_MQ:
		mappingMessageSystemTag(span, otSpan, mapping)
	case span.SpanType == v3.SpanType_Exit:
		mappingDatabaseTag(span, otSpan)
	}

	return otSpan
}

func mappingDatabaseTag(span *v3.SpanObject, otSpan *OtSpan) {
	if span.GetPeer() == "" {
		return
	}

	if span.SpanLayer != v3.SpanLayer_Database {
		return
	}

	var dbType string
	for _, tag := range span.GetTags() {
		if tag.Key == "db.type" {
			dbType = tag.Value
			break
		}
	}

	if dbType == "" {
		return
	}

	otSpan.Attribute[AttributeDBConnectionString] = strings.ToLower(dbType) + "://" + span.GetPeer()
}

func mappingMessageSystemTag(span *v3.SpanObject, otSpan *OtSpan, mapping map[int32]string) {
	messageSystem, finded := mapping[span.ComponentId]
	if finded {
		otSpan.Attribute[AttributeMessagingSystem] = messageSystem
	} else {
		otSpan.Attribute[AttributeMessagingSystem] = "MessagingSystem"
	}
}

func ConvertResourceOt(props *management.InstanceProperties) map[string]string {
	propertyMap := make(map[string]string, len(props.Properties))
	for _, p := range props.Properties {
		otKey, ok := OtResourceMapping[p.Key]
		if ok {
			propertyMap[otKey] = p.Value
		} else {
			propertyMap[p.Key] = p.Value
		}
	}
	return propertyMap
}
