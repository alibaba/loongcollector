// Copyright 2026 iLogtail Authors
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

// Package telemetrytest decodes self-monitor payloads for bridge tests.
package telemetrytest

import (
	"strconv"
	"time"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

// TransferPipelineEventGroupToAlarms converts a PipelineEventGroup (containing LogEvents)
// back to a slice of AlarmExportMessage. This is the inverse of TransferAlarmsToPipelineEventGroup.
func TransferPipelineEventGroupToAlarms(group *protocol.PipelineEventGroup) []selfmonitor.AlarmExportMessage {
	logsWrapper := group.GetLogs()
	if logsWrapper == nil {
		return nil
	}
	alarms := make([]selfmonitor.AlarmExportMessage, 0, len(logsWrapper.Events))
	for _, logEvent := range logsWrapper.Events {
		alarms = append(alarms, transferLogEventToAlarm(logEvent))
	}
	return alarms
}

func transferLogEventToAlarm(logEvent *protocol.LogEvent) selfmonitor.AlarmExportMessage {
	msg := selfmonitor.AlarmExportMessage{}
	for _, content := range logEvent.Contents {
		key := string(content.Key)
		val := string(content.Value)
		switch key {
		case "alarm_type":
			msg.AlarmType = val
		case "alarm_level":
			msg.AlarmLevel = val
		case "alarm_message":
			msg.AlarmMessage = val
		case "alarm_count":
			msg.Count, _ = strconv.Atoi(val)
		case "project_name":
			msg.ProjectName = val
		case "category":
			msg.Category = val
		case "config":
			msg.Config = val
		}
	}
	return msg
}

// TransferPipelineEventGroupToMetrics converts a PipelineEventGroup (containing MetricEvents)
// back to a slice of MetricExportRecord. This is the inverse of TransferMetricsToPipelineEventGroup.
func TransferPipelineEventGroupToMetrics(group *protocol.PipelineEventGroup) []selfmonitor.MetricExportRecord {
	metricsWrapper := group.GetMetrics()
	if metricsWrapper == nil {
		return nil
	}
	records := make([]selfmonitor.MetricExportRecord, 0, len(metricsWrapper.Events))
	for _, metricEvent := range metricsWrapper.Events {
		records = append(records, transferMetricEventToRecord(metricEvent))
	}
	return records
}

func transferMetricEventToRecord(metricEvent *protocol.MetricEvent) selfmonitor.MetricExportRecord {
	record := selfmonitor.MetricExportRecord{
		Category: string(metricEvent.Name),
		Labels:   make(map[string]string, len(metricEvent.Tags)),
		Counters: make(map[string]uint64),
		Gauges:   make(map[string]float64),
	}

	for k, v := range metricEvent.Tags {
		record.Labels[k] = string(v)
	}

	if multiValues := metricEvent.GetUntypedMultiDoubleValues(); multiValues != nil {
		for name, val := range multiValues.Values {
			switch val.MetricType {
			case protocol.UntypedValueMetricType_METRIC_TYPE_COUNTER:
				record.Counters[name] = uint64(val.Value)
			case protocol.UntypedValueMetricType_METRIC_TYPE_GAUGE:
				record.Gauges[name] = val.Value
			}
		}
	}

	return record
}

// TransferMetricsToPipelineEventGroup converts a batch of MetricExportRecord to a
// PipelineEventGroup containing MetricEvents, using the existing PipelineEventGroup PB
// as the transport format. Each record becomes one MetricEvent with UntypedMultiDoubleValues.
//
// Field mapping aligns with C++ SelfMonitorMetricEvent::ReadAsMetricEvent:
//
//	Category -> MetricEvent.Name       (C++ mCategory)
//	Labels   -> MetricEvent.Tags       (C++ mLabels)
//	Counters -> UntypedMultiDoubleValues with METRIC_TYPE_COUNTER (C++ mCounters)
//	Gauges   -> UntypedMultiDoubleValues with METRIC_TYPE_GAUGE   (C++ mGauges)
func TransferMetricsToPipelineEventGroup(records []selfmonitor.MetricExportRecord, t time.Time) *protocol.PipelineEventGroup {
	metricEvents := make([]*protocol.MetricEvent, 0, len(records))
	for i := range records {
		metricEvents = append(metricEvents, transferMetricRecordToMetricEvent(&records[i], t))
	}
	return &protocol.PipelineEventGroup{
		PipelineEvents: &protocol.PipelineEventGroup_Metrics{
			Metrics: &protocol.PipelineEventGroup_MetricEvents{Events: metricEvents},
		},
	}
}

// TransferRawMetricsToPipelineEventGroup is a convenience wrapper that accepts the
// existing []map[string]string export format and converts via ParseMetricExportRecord.
// Records that fail to parse are silently skipped.
func TransferRawMetricsToPipelineEventGroup(rawMetrics []map[string]string, t time.Time) *protocol.PipelineEventGroup {
	records := make([]selfmonitor.MetricExportRecord, 0, len(rawMetrics))
	for _, raw := range rawMetrics {
		record, err := selfmonitor.ParseMetricExportRecord(raw)
		if err != nil {
			continue
		}
		records = append(records, record)
	}
	return TransferMetricsToPipelineEventGroup(records, t)
}

func transferMetricRecordToMetricEvent(record *selfmonitor.MetricExportRecord, t time.Time) *protocol.MetricEvent {
	ts := uint64(t.Unix())*1e9 + uint64(t.Nanosecond())

	tags := make(map[string][]byte, len(record.Labels))
	for k, v := range record.Labels {
		tags[k] = []byte(v)
	}

	values := make(map[string]*protocol.UntypedMultiDoubleValue, len(record.Counters)+len(record.Gauges))
	for name, val := range record.Counters {
		values[name] = &protocol.UntypedMultiDoubleValue{
			MetricType: protocol.UntypedValueMetricType_METRIC_TYPE_COUNTER,
			Value:      float64(val),
		}
	}
	for name, val := range record.Gauges {
		values[name] = &protocol.UntypedMultiDoubleValue{
			MetricType: protocol.UntypedValueMetricType_METRIC_TYPE_GAUGE,
			Value:      val,
		}
	}

	return &protocol.MetricEvent{
		Timestamp: ts,
		Name:      []byte(record.Category),
		Tags:      tags,
		Value: &protocol.MetricEvent_UntypedMultiDoubleValues{
			UntypedMultiDoubleValues: &protocol.UntypedMultiDoubleValues{Values: values},
		},
	}
}
