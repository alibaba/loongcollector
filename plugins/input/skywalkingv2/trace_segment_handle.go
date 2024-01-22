// Copyright 2021 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http:www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package skywalkingv2

import (
	"errors"
	"fmt"
	"strconv"
	"strings"

	"github.com/gogo/protobuf/proto"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/input/skywalkingv2/skywalking/apm/network/language/agent"
	"github.com/alibaba/ilogtail/plugins/input/skywalkingv3"
)

const (
	skywalkingTopicKey = "mq.topic"
	skywalkingQueueKey = "mq.queue"
)

type TraceSegmentHandle struct {
	RegistryInformationCache

	context   pipeline.Context
	collector pipeline.Collector

	compIDMessagingSystemMapping map[int32]string
}

func (t *TraceSegmentHandle) Collect(server agent.TraceSegmentService_CollectServer) error {
	defer panicRecover()
	for {
		segmentObject, err := server.Recv()
		if err != nil {
			// 当Logtail重启后，使用这个版本的SW的服务会出现问题，由于SW没有下发命令的功能，所以没办法重新注册
			// 出现数据上不来的问题，只能重启服务
			return server.SendAndClose(&agent.Downstream{})
		}

		err = t.collectSegment(segmentObject)
		if err != nil {
			// 当Logtail重启后，使用这个版本的SW的服务会出现问题，由于SW没有下发命令的功能，所以没办法重新注册
			// 出现数据上不来的问题，只能重启服务
			return server.SendAndClose(&agent.Downstream{})
		}
	}
}

func (t *TraceSegmentHandle) collectSegment(upstream *agent.UpstreamSegment) error {
	if len(upstream.GlobalTraceIds) == 0 {
		return nil
	}
	segment := &agent.TraceSegmentObject{}
	traceID := convertUniIDToString(upstream.GlobalTraceIds[0])
	if e := proto.Unmarshal(upstream.GetSegment(), segment); e != nil {
		return e
	}

	applicationInstance, ok := t.RegistryInformationCache.findApplicationInstanceRegistryInfo(segment.ApplicationInstanceId)
	if !ok {
		return errors.New("Application Not found")
	}

	for _, span := range segment.Spans {
		if otTrace := t.parseSpan(span, applicationInstance, traceID, convertUniIDToString(segment.TraceSegmentId)); otTrace != nil {
			log, err := otTrace.ToLog()
			if err != nil {
				logger.Error(t.context.GetRuntimeContext(), "SKYWALKING_TO_OT_TRACE_ERR", "err", err)
				return err
			}
			t.collector.AddRawLog(log)
		}
	}

	return nil
}

func (t *TraceSegmentHandle) parseSpan(span *agent.SpanObject, applicationInstance *ApplicationInstance, traceID string, traceSegmentID string) *skywalkingv3.OtSpan {
	otSpan := skywalkingv3.NewOtSpan()
	otSpan.Resource = applicationInstance.properties
	otSpan.Service = applicationInstance.application.applicationName
	otSpan.Host = applicationInstance.properties[skywalkingv3.AttributeHostName]

	if span.OperationNameId != 0 {
		e, ok := t.RegistryInformationCache.findEndpointRegistryInfoByID(span.OperationNameId)
		if !ok {
			return nil
		}
		otSpan.Name = e.endpointName
	} else {
		otSpan.Name = span.OperationName
	}

	switch {
	case span.SpanLayer == agent.SpanLayer_MQ:
		if span.SpanType == agent.SpanType_Entry {
			otSpan.Kind = skywalkingv3.OpenTracingSpanKindConsumer
		} else if span.SpanType == agent.SpanType_Exit {
			otSpan.Kind = skywalkingv3.OpenTracingSpanKindProducer
		}
		t.mappingMessageSystemTag(span, otSpan)
	case span.SpanType == agent.SpanType_Entry:
		otSpan.Kind = skywalkingv3.OpenTracingSpanKindServer
	case span.SpanType == agent.SpanType_Exit:
		otSpan.Kind = skywalkingv3.OpenTracingSpanKindClient
		mappingDatabaseTag(span, otSpan)
	case span.SpanType == agent.SpanType_Local:
		otSpan.Kind = skywalkingv3.OpenTracingSpanKindInternal
	default:
		otSpan.Kind = skywalkingv3.OpenTracingSpanKindUnspecified
	}

	otSpan.TraceID = traceID
	otSpan.SpanID = traceSegmentID + "." + strconv.FormatInt(int64(span.SpanId), 10)
	if span.ParentSpanId < 0 {
		otSpan.ParentSpanID = ""
	} else {
		otSpan.ParentSpanID = traceSegmentID + "." + strconv.FormatInt(int64(span.ParentSpanId), 10)
	}
	otSpan.Logs = make([]map[string]string, len(span.Logs))
	for i, log := range span.Logs {
		logEvent := make(map[string]string)
		logEvent["time"] = strconv.FormatInt(log.Time, 10)
		for _, kv := range log.Data {
			logEvent[kv.Key] = kv.Value
			if kv.Key == "error.kind" && len(kv.Value) > 0 {
				otSpan.StatusMessage = kv.Value
			}
		}
		otSpan.Logs[i] = logEvent
	}
	otSpan.Links = make([]*skywalkingv3.OtSpanRef, len(span.Refs))
	if len(span.Refs) > 0 {
		for i, ref := range span.Refs {
			parentTraceSegmentID := ""
			for _, part := range ref.ParentTraceSegmentId.IdParts {
				parentTraceSegmentID += fmt.Sprintf("%d", part)
			}
			spanRef := &skywalkingv3.OtSpanRef{
				TraceID:    traceID,
				SpanID:     convertUniIDToString(ref.ParentTraceSegmentId) + "." + strconv.FormatInt(int64(ref.ParentSpanId), 10),
				TraceState: "",
				Attributes: nil,
			}
			otSpan.Links[i] = spanRef
		}
		otSpan.ParentSpanID = convertUniIDToString(span.Refs[0].ParentTraceSegmentId) + "." + strconv.FormatInt(int64(span.Refs[0].ParentSpanId), 10)
	}
	otSpan.Start = span.StartTime * 1000
	otSpan.End = span.EndTime * 1000
	otSpan.Duration = 1000 * (span.EndTime - span.StartTime)
	if len(span.Peer) > 0 {
		hostport := strings.Split(span.Peer, ":")
		otSpan.Attribute[skywalkingv3.AttributeNetPeerIP] = hostport[0]
		if len(hostport) == 2 {
			otSpan.Attribute[skywalkingv3.AttributeNetPeerPort] = hostport[1]
		}
	}
	if len(span.Tags) > 0 {
		for _, tag := range span.Tags {
			otKey, ok := skywalkingv3.OtSpanTagsMapping[tag.Key]
			if ok {
				otSpan.Attribute[otKey] = tag.Value
			} else {
				if skywalkingTopicKey == tag.Key {
					otSpan.Attribute[skywalkingv3.AttributeMessagingDestinationKind] = "topic"
					otSpan.Attribute[skywalkingv3.AttributeMessagingDestination] = tag.Value
				} else if skywalkingQueueKey == tag.Key {
					otSpan.Attribute[skywalkingv3.AttributeMessagingDestinationKind] = "queue"
					otSpan.Attribute[skywalkingv3.AttributeMessagingDestination] = tag.Value
				}
				otSpan.Attribute[tag.Key] = tag.Value
			}
		}
	}
	if span.IsError {
		otSpan.StatusCode = skywalkingv3.StatusCodeError
	} else {
		otSpan.StatusCode = skywalkingv3.StatusCodeOk
	}

	switch {
	case span.SpanLayer == agent.SpanLayer_MQ:
		t.mappingMessageSystemTag(span, otSpan)
	case span.SpanType == agent.SpanType_Exit:
		mappingDatabaseTag(span, otSpan)
	}

	return otSpan
}

func mappingDatabaseTag(span *agent.SpanObject, otSpan *skywalkingv3.OtSpan) {
	if span.GetPeer() == "" {
		return
	}

	if span.SpanLayer != agent.SpanLayer_Database {
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

	otSpan.Attribute[skywalkingv3.AttributeDBConnectionString] = strings.ToLower(dbType) + "://" + span.GetPeer()
}

func convertUniIDToString(u *agent.UniqueId) string {
	if len(u.IdParts) == 0 {
		return ""
	}

	parentTraceSegmentID := ""
	for _, part := range u.IdParts {
		parentTraceSegmentID += fmt.Sprintf("%d.", part)
	}
	return parentTraceSegmentID[:len(parentTraceSegmentID)-1]
}

func (t *TraceSegmentHandle) mappingMessageSystemTag(span *agent.SpanObject, otSpan *skywalkingv3.OtSpan) {
	messageSystem, finded := t.compIDMessagingSystemMapping[span.ComponentId]
	if finded {
		otSpan.Attribute[skywalkingv3.AttributeMessagingSystem] = messageSystem
	} else {
		otSpan.Attribute[skywalkingv3.AttributeMessagingSystem] = "MessagingSystem"
	}
}
