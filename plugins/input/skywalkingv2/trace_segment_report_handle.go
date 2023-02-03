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
	"context"
	"crypto/rand"
	"errors"
	"fmt"
	"io"
	"math"
	"math/big"
	"runtime"
	"strconv"
	"strings"

	"google.golang.org/protobuf/proto"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/input/skywalkingv2/skywalking/apm/network/common"
	"github.com/alibaba/ilogtail/plugins/input/skywalkingv2/skywalking/apm/network/language/agent"
	v2 "github.com/alibaba/ilogtail/plugins/input/skywalkingv2/skywalking/apm/network/language/agent/v2"
	"github.com/alibaba/ilogtail/plugins/input/skywalkingv3"
)

type TraceSegmentReportHandle struct {
	RegistryInformationCache

	context   pipeline.Context
	collector pipeline.Collector

	compIDMessagingSystemMapping map[int32]string
}

func panicRecover() {
	if err := recover(); err != nil {
		trace := make([]byte, 2048)
		runtime.Stack(trace, true)
		logger.Error(context.Background(), "PLUGIN_RUNTIME_ALARM", "skywalking v2 runtime panic error", err, "stack", string(trace))
	}
}

func (t *TraceSegmentReportHandle) Collect(server v2.TraceSegmentReportService_CollectServer) error {
	defer panicRecover()
	for {
		segmentObject, err := server.Recv()
		if err != nil {
			if err == io.EOF {
				return server.SendAndClose(&common.Commands{})
			}
			return err
		}

		e := t.collectSegment(server, segmentObject)
		if e != nil {
			return server.SendAndClose(&common.Commands{})
		}
	}
}

func (t *TraceSegmentReportHandle) collectSegment(server v2.TraceSegmentReportService_CollectServer, upstream *agent.UpstreamSegment) error {
	if len(upstream.GlobalTraceIds) == 0 {
		return nil
	}
	segment := &v2.SegmentObject{}

	if err := proto.Unmarshal(upstream.GetSegment(), segment); err != nil {
		return err
	}

	jaegerFormat, traceID := getTraceID(upstream.GlobalTraceIds[0])
	applicationInstance, ok := t.RegistryInformationCache.findApplicationInstanceRegistryInfo(segment.ServiceInstanceId)
	if !ok || applicationInstance.application == nil {
		// 如果Instance不存在，让应用重新注册，会造成丢失部分数据
		args := make([]*common.KeyStringValuePair, 0)
		serializeNumber, _ := rand.Int(rand.Reader, big.NewInt(math.MaxInt64))
		args = append(args, &common.KeyStringValuePair{Key: "SerialNumber", Value: fmt.Sprintf("reset-%d", serializeNumber)})
		var commands []*common.Command
		commands = append(commands, &common.Command{Command: "ServiceMetadataReset", Args: args})
		return server.SendAndClose(&common.Commands{
			Commands: commands,
		})
	}

	spanMapping := make(map[int32]*v2.SpanObjectV2)
	for _, span := range segment.Spans {
		spanMapping[span.SpanId] = span
	}

	for _, span := range segment.Spans {
		_, traceSegmentID := getTraceID(segment.TraceSegmentId)
		var otTrace *skywalkingv3.OtSpan
		var err error

		if jaegerFormat {
			otTrace, err = t.parseSpan(span, applicationInstance, traceID, traceSegmentID, spanMapping, generateSpanIDByJaeger, generateParentSpanIDByJaeger)
		} else {
			otTrace, err = t.parseSpan(span, applicationInstance, traceID, traceSegmentID, spanMapping, generateSpanIDByOriginal, generateParentSpanIDByOriginal)
		}

		if otTrace == nil {
			if err != nil {
				args := make([]*common.KeyStringValuePair, 0)
				serializeNumber, _ := rand.Int(rand.Reader, big.NewInt(math.MaxInt64))
				args = append(args, &common.KeyStringValuePair{Key: "SerialNumber", Value: fmt.Sprintf("reset-%d", serializeNumber)})
				_ = server.SendMsg(common.Command{
					Command: "ServiceMetadataReset",
					Args:    args})
			}
		} else {
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

func (t *TraceSegmentReportHandle) parseSpan(span *v2.SpanObjectV2, applicationInstance *ApplicationInstance,
	traceID string, traceSegmentID string,
	spanMapping map[int32]*v2.SpanObjectV2,
	generateSpanID func(applicationInstance *ApplicationInstance, traceSegmentID string, span *v2.SpanObjectV2) string,
	generateParentSpanID func(ref *v2.SegmentReference) string) (*skywalkingv3.OtSpan, error) {
	otSpan := skywalkingv3.NewOtSpan()
	otSpan.Resource = applicationInstance.properties
	otSpan.Service = applicationInstance.application.applicationName
	otSpan.Host = applicationInstance.properties[skywalkingv3.AttributeHostName]

	if span.OperationNameId != 0 {
		e, ok := t.RegistryInformationCache.findEndpointRegistryInfoByID(span.OperationNameId)

		if !ok || e == nil {
			return nil, errors.New("Failed to find OperationName ID")
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
	case span.SpanType == agent.SpanType_Local:
		otSpan.Kind = skywalkingv3.OpenTracingSpanKindInternal
	default:
		otSpan.Kind = skywalkingv3.OpenTracingSpanKindUnspecified
	}

	otSpan.TraceID = traceID
	otSpan.SpanID = generateSpanID(applicationInstance, traceSegmentID, span)
	if span.ParentSpanId < 0 {
		otSpan.ParentSpanID = ""
	} else {
		otSpan.ParentSpanID = generateSpanID(applicationInstance, traceSegmentID, spanMapping[span.ParentSpanId])
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
			spanRef := &skywalkingv3.OtSpanRef{
				TraceID: traceID,
				SpanID:  generateParentSpanID(ref),
			}
			otSpan.Links[i] = spanRef
		}
		otSpan.ParentSpanID = generateParentSpanID(span.Refs[0])
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
	return otSpan, nil
}

func generateParentSpanIDByJaeger(ref *v2.SegmentReference) string {
	return fmt.Sprintf("%08x", uint32(ref.ParentServiceInstanceId)) + fmt.Sprintf("%08x", uint32(ref.ParentSpanId))
}

func generateParentSpanIDByOriginal(ref *v2.SegmentReference) string {
	return convertUniIDToString(ref.ParentTraceSegmentId) + "." + strconv.FormatInt(int64(ref.ParentSpanId), 10)
}

func generateSpanIDByJaeger(applicationInstance *ApplicationInstance, traceSegmentID string, span *v2.SpanObjectV2) string {
	return traceSegmentID[len(traceSegmentID)-12:] + fmt.Sprintf("%04x", span.SpanId)[0:4]
}

func generateSpanIDByOriginal(applicationInstance *ApplicationInstance, traceSegmentID string, span *v2.SpanObjectV2) string {
	return traceSegmentID + "." + strconv.FormatInt(int64(span.SpanId), 10)
}

func getTraceID(u *agent.UniqueId) (jeagerFormat bool, traceID string) {
	if len(u.IdParts) == 0 {
		return false, ""
	}

	if u.IdParts[0] == 648495579 {
		jeagerFormat = true
	}

	if jeagerFormat {
		for i := 1; i < len(u.IdParts); i++ {
			traceID += fmt.Sprintf("%016x", uint64(u.IdParts[i]))
		}
	} else {
		for _, part := range u.IdParts {
			traceID += fmt.Sprintf("%d.", part)
		}
		traceID = traceID[0 : len(traceID)-1]
	}

	return jeagerFormat, traceID
}

func (t *TraceSegmentReportHandle) mappingMessageSystemTag(span *v2.SpanObjectV2, otSpan *skywalkingv3.OtSpan) {
	messageSystem, finded := t.compIDMessagingSystemMapping[span.ComponentId]
	if finded {
		otSpan.Attribute[skywalkingv3.AttributeMessagingSystem] = messageSystem
	} else {
		otSpan.Attribute[skywalkingv3.AttributeMessagingSystem] = "MessagingSystem"
	}
}
