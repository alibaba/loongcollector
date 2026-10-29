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

//go:build linux || windows
// +build linux windows

package sls

import (
	"fmt"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logtail"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

// SlsFlusher implements both the v1 (Flush) and v2 (Export) flusher interfaces
// so it can be loaded by either Runner; it uses symbols in LogtailAdaptor(.so)
// to push log groups into Logtail's sending queue.
var (
	_ pipeline.FlusherV1 = (*SlsFlusher)(nil)
	_ pipeline.FlusherV2 = (*SlsFlusher)(nil)
)

// SlsFlusher uses symbols in LogtailAdaptor(.so) to flush log groups to Logtail.
type SlsFlusher struct { // nolint:revive
	EnableShardHash bool
	KeepShardHash   bool

	context pipeline.Context
	// sendPb / sendPbV2 abstract the cgo calls into Logtail so the send path can
	// be exercised in unit tests without a live Logtail adaptor. They default to
	// the real logtail symbols in Init.
	sendPb                 func(configName, logstore string, pbBuffer []byte, lines int) int
	sendPbV2               func(configName, logstore string, pbBuffer []byte, lines int, hash string) int
	sendPipelineEventGroup func(configName, logstore string, pbBuffer []byte, eventCount int, hash string) int
}

// Init ...
func (p *SlsFlusher) Init(context pipeline.Context) error {
	p.context = context
	if p.sendPb == nil {
		p.sendPb = logtail.SendPb
	}
	if p.sendPbV2 == nil {
		p.sendPbV2 = logtail.SendPbV2
	}
	if p.sendPipelineEventGroup == nil {
		p.sendPipelineEventGroup = logtail.SendPipelineEventGroup
	}
	return nil
}

// Description ...
func (p *SlsFlusher) Description() string {
	return "logtail flusher for log service"
}

// IsReady ...
// There is a sending queue in Logtail, call LogtailIsValidToSend through cgo
// to make sure if there is any space for coming data.
func (p *SlsFlusher) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return logtail.IsValidToSend(logstoreKey)
}

// Flush ...
// Because IsReady is called before, Logtail must have space in sending queue,
// just call LogtailSendPb through cgo to push data into queue, Logtail will
// send data to its destination (SLS mostly) according to its config.
func (p *SlsFlusher) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	for _, logGroup := range logGroupList {
		if err := p.sendLogGroup(configName, logGroup); err != nil {
			return err
		}
	}

	return nil
}

// Export sends typed PipelineEventGroup protobufs to C++. The protobuf uses a
// oneof, so mixed groups are split into consecutive runs of the same event kind
// without dropping events or changing their order. Flush retains the v1 ABI.
func (p *SlsFlusher) Export(groupEventsList []*models.PipelineGroupEvents, _ pipeline.PipelineContext) error {
	for _, groupEvents := range groupEventsList {
		if groupEvents == nil || len(groupEvents.Events) == 0 {
			continue
		}
		for start := 0; start < len(groupEvents.Events); {
			if groupEvents.Events[start] == nil {
				return fmt.Errorf("nil pipeline event at index %d", start)
			}
			kind := groupEvents.Events[start].GetType()
			end := start + 1
			for end < len(groupEvents.Events) && groupEvents.Events[end] != nil && groupEvents.Events[end].GetType() == kind {
				end++
			}
			if err := p.sendPipelineEvents(groupEvents.Group, groupEvents.Events[start:end]); err != nil {
				return err
			}
			start = end
		}
	}
	return nil
}

func (p *SlsFlusher) sendPipelineEvents(group *models.GroupInfo, events []models.PipelineEvent) error {
	pb := &protocol.PipelineEventGroup{
		Tags: make(map[string][]byte), Metadata: make(map[string][]byte),
	}
	for k, v := range group.GetTags().Iterator() {
		pb.Tags[k] = []byte(v)
	}
	for k, v := range group.GetMetadata().Iterator() {
		pb.Metadata[k] = []byte(v)
	}
	// Check conversion errors explicitly: the generic group helper ignores
	// event conversion failures, which would turn malformed data into data loss.
	switch events[0].GetType() {
	case models.EventTypeLogging:
		logs := &protocol.PipelineEventGroup_LogEvents{}
		for _, event := range events {
			log, ok := event.(*models.Log)
			if !ok || log == nil {
				return fmt.Errorf("invalid log event %T", event)
			}
			converted, err := helper.TransferLogEventToPB(log)
			if err != nil {
				return fmt.Errorf("convert log event: %w", err)
			}
			logs.Events = append(logs.Events, converted)
		}
		pb.PipelineEvents = &protocol.PipelineEventGroup_Logs{Logs: logs}
	case models.EventTypeMetric:
		metrics := &protocol.PipelineEventGroup_MetricEvents{}
		for _, event := range events {
			metric, ok := event.(*models.Metric)
			if !ok || metric == nil {
				return fmt.Errorf("invalid metric event %T", event)
			}
			converted, err := helper.TransferMetricEventToPB(metric)
			if err != nil {
				return fmt.Errorf("convert metric event: %w", err)
			}
			metrics.Events = append(metrics.Events, converted)
		}
		pb.PipelineEvents = &protocol.PipelineEventGroup_Metrics{Metrics: metrics}
	case models.EventTypeSpan:
		spans := &protocol.PipelineEventGroup_SpanEvents{}
		for _, event := range events {
			span, ok := event.(*models.Span)
			if !ok || span == nil {
				return fmt.Errorf("invalid span event %T", event)
			}
			converted, err := helper.TransferSpanEventToPB(span)
			if err != nil {
				return fmt.Errorf("convert span event: %w", err)
			}
			spans.Events = append(spans.Events, converted)
		}
		pb.PipelineEvents = &protocol.PipelineEventGroup_Spans{Spans: spans}
	default:
		return fmt.Errorf("unsupported pipeline event type %v", events[0].GetType())
	}
	var shardHash string
	if p.EnableShardHash {
		shardHash = string(pb.Tags[util.ShardHashTagKey])
		if !p.KeepShardHash {
			delete(pb.Tags, util.ShardHashTagKey)
		}
	}
	buf, err := pb.Marshal()
	if err != nil {
		return fmt.Errorf("marshal pipeline event group: %w", err)
	}
	if result := p.sendPipelineEventGroup(p.context.GetConfigName(), p.context.GetLogstore(), buf, len(events), shardHash); result < 0 {
		return fmt.Errorf("send pipeline event group: error %d", result)
	}
	return nil
}

// sendLogGroup marshals a single LogGroup and pushes it into Logtail's sending
// queue via the legacy v1 SendPb/SendPbV2 ABI.
func (p *SlsFlusher) sendLogGroup(configName string, logGroup *protocol.LogGroup) error {
	if len(logGroup.Logs) == 0 {
		return nil
	}

	var shardHash string
	if p.EnableShardHash {
		for idx, tag := range logGroup.LogTags {
			if tag.Key == util.ShardHashTagKey {
				shardHash = tag.Value
				if !p.KeepShardHash {
					logGroup.LogTags = append(logGroup.LogTags[0:idx], logGroup.LogTags[idx+1:]...)
				}
				break
			}
		}
	}
	buf, err := logGroup.Marshal()
	if err != nil {
		return fmt.Errorf("loggroup marshal err %v", err)
	}

	var rst int
	if !p.EnableShardHash {
		rst = p.sendPb(configName, logGroup.Category, buf, len(logGroup.Logs))
	} else {
		rst = p.sendPbV2(configName, logGroup.Category, buf, len(logGroup.Logs), shardHash)
	}
	if rst < 0 {
		return fmt.Errorf("send error %d", rst)
	}
	return nil
}

// SetUrgent ...
// We do nothing here because necessary flag has already been set in Logtail
// before this method is called. Any future call of IsReady will return
// true so that remaining data can be flushed to Logtail (which will flush
// data to local file system) before it quits.
func (*SlsFlusher) SetUrgent(flag bool) {
}

// Stop ...
// We do nothing here because SlsFlusher does not create any resources.
func (*SlsFlusher) Stop() error {
	return nil
}

func init() {
	pipeline.Flushers["flusher_sls"] = func() pipeline.Flusher {
		return &SlsFlusher{
			EnableShardHash: false,
			KeepShardHash:   true,
		}
	}
}
