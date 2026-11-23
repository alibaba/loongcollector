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

var _ pipeline.Flusher = (*SlsFlusher)(nil)

// SlsFlusher uses symbols in LogtailAdaptor(.so) to flush log groups to Logtail.
type SlsFlusher struct { // nolint:revive
	EnableShardHash bool
	KeepShardHash   bool

	context pipeline.Context
	// sendPipelineEventGroup wraps the C++ bridge for testing.
	sendPipelineEventGroup func(configName, logstore string, pbBuffer []byte, eventCount int, hash string) int
}

// Init ...
func (p *SlsFlusher) Init(context pipeline.Context) error {
	p.context = context
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

// Export sends typed PipelineEventGroups to C++. Log tags are group-level
// protocol attributes; adjacent events are partitioned by log tags and event
// kind without changing their order.
func (p *SlsFlusher) Export(groups []*models.PipelineGroupEvents, _ pipeline.PipelineContext) error {
	for _, group := range groups {
		if err := helper.ForEachLogTagGroup(group, p.exportPipelineGroup); err != nil {
			return err
		}
	}
	return nil
}

func (p *SlsFlusher) exportPipelineGroup(group *models.PipelineGroupEvents) error {
	for start := 0; start < len(group.Events); {
		if group.Events[start] == nil {
			return fmt.Errorf("nil pipeline event at index %d", start)
		}
		kind := group.Events[start].GetType()
		end := start + 1
		for end < len(group.Events) && group.Events[end] != nil && group.Events[end].GetType() == kind {
			end++
		}
		if err := p.sendPipelineEvents(group.Group, group.Events[start:end]); err != nil {
			return err
		}
		start = end
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
	var shardHash string
	if p.EnableShardHash {
		shardHash = string(pb.Tags[util.ShardHashTagKey])
		if !p.KeepShardHash {
			delete(pb.Tags, util.ShardHashTagKey)
		}
	}
	// Check conversion errors explicitly: the generic group helper ignores
	// event conversion failures, which would turn malformed data into data loss.
	eventCount := len(events)
	switch events[0].GetType() {
	case models.EventTypeLogging:
		buf, count, err := marshalLogGroup(pb, events)
		if err != nil || count == 0 {
			return err
		}
		return p.sendEventGroupBytes(group, buf, count, shardHash)
	case models.EventTypeMetric:
		metrics := &protocol.PipelineEventGroup_MetricEvents{
			Events: make([]*protocol.MetricEvent, 0, len(events)),
		}
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
		spans := &protocol.PipelineEventGroup_SpanEvents{
			Events: make([]*protocol.SpanEvent, 0, len(events)),
		}
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
	buf, err := pb.Marshal()
	if err != nil {
		return fmt.Errorf("marshal pipeline event group: %w", err)
	}
	return p.sendEventGroupBytes(group, buf, eventCount, shardHash)
}

func (p *SlsFlusher) sendEventGroupBytes(group *models.GroupInfo, buf []byte, eventCount int, shardHash string) error {
	logstore := group.GetDestinationLogstore()
	if logstore == "" {
		logstore = p.context.GetLogstore()
	}
	if result := p.sendPipelineEventGroup(p.context.GetConfigName(), logstore, buf, eventCount, shardHash); result < 0 {
		return fmt.Errorf("send pipeline event group: error %d", result)
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
