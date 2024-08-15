// Copyright 2022 iLogtail Authors
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

package models

type SpanKind int

const (
	_ SpanKind = iota
	// Indicates that the span represents an internal operation within an application,
	// as opposed to an operation happening at the boundaries. Default value.
	SpanKindInternal

	// Indicates that the span covers server-side handling of an RPC or other
	// remote network request.
	SpanKindServer

	// Indicates that the span describes a request to some remote service.
	SpanKindClient

	// Indicates that the span describes a producer sending a message to a broker.
	// Unlike CLIENT and SERVER, there is often no direct critical path latency relationship
	// between producer and consumer spans. A PRODUCER span ends when the message was accepted
	// by the broker while the logical processing of the message might span a much longer time.
	SpanKindProducer

	// Indicates that the span describes consumer receiving a message from a broker.
	// Like the PRODUCER kind, there is often no direct critical path latency relationship
	// between producer and consumer spans.
	SpanKindConsumer
)

type SpanKindText string

const (
	SpanKindTextInternal = "internal"
	SpanKindTextServer   = "server"
	SpanKindTextClient   = "client"
	SpanKindTextProducer = "producer"
	SpanKindTextConsumer = "consumer"
)

type StatusCode int

const (
	StatusCodeUnSet StatusCode = iota
	StatusCodeOK
	StatusCodeError
)

var (
	SpanKindTexts = map[SpanKind]SpanKindText{
		SpanKindInternal: SpanKindTextInternal,
		SpanKindServer:   SpanKindTextServer,
		SpanKindClient:   SpanKindTextClient,
		SpanKindProducer: SpanKindTextProducer,
		SpanKindConsumer: SpanKindTextConsumer,
	}
	SpanKindValues = map[SpanKindText]SpanKind{
		SpanKindTextInternal: SpanKindInternal,
		SpanKindTextServer:   SpanKindServer,
		SpanKindTextClient:   SpanKindClient,
		SpanKindTextProducer: SpanKindProducer,
		SpanKindTextConsumer: SpanKindConsumer,
	}

	noopSpanEvents = make([]*SpanEvent, 0)
	noopSpanLinks  = make([]*SpanLink, 0)
)

type SpanLink struct {
	TraceID    string
	SpanID     string
	TraceState string
	Tags       Tags
}

type SpanEvent struct {
	Timestamp int64
	Name      string
	Tags      Tags
}

// A Span represents a single operation performed by a single component of the system.
type Span struct {
	TraceID      string
	SpanID       string
	ParentSpanID string
	Name         string
	TraceState   string

	StartTime         uint64
	EndTime           uint64
	ObservedTimestamp uint64

	Kind   SpanKind
	Status StatusCode
	Tags   Tags
	Links  []*SpanLink
	Events []*SpanEvent
}

func (m *Span) GetName() string {
	if m != nil {
		return m.Name
	}
	return ""
}

func (m *Span) SetName(name string) {
	if m != nil {
		m.Name = name
	}
}

func (m *Span) GetTags() Tags {
	if m != nil {
		return m.Tags
	}
	return NilStringValues
}

func (m *Span) GetType() EventType {
	return EventTypeSpan
}

func (m *Span) GetTimestamp() uint64 {
	if m != nil {
		return m.StartTime
	}
	return 0
}

func (m *Span) GetObservedTimestamp() uint64 {
	if m != nil {
		return m.ObservedTimestamp
	}
	return 0
}

func (m *Span) SetObservedTimestamp(timestamp uint64) {
	if m != nil {
		m.ObservedTimestamp = timestamp
	}
}

func (m *Span) GetTraceID() string {
	if m != nil {
		return m.TraceID
	}
	return ""
}

func (m *Span) GetSpanID() string {
	if m != nil {
		return m.SpanID
	}
	return ""
}

func (m *Span) GetParentSpanID() string {
	if m != nil {
		return m.ParentSpanID
	}
	return ""
}

func (m *Span) GetTraceState() string {
	if m != nil {
		return m.TraceState
	}
	return ""
}

func (m *Span) GetStartTime() uint64 {
	if m != nil {
		return m.StartTime
	}
	return 0
}

func (m *Span) GetEndTime() uint64 {
	if m != nil {
		return m.EndTime
	}
	return 0
}

func (m *Span) GetKind() SpanKind {
	if m != nil {
		return m.Kind
	}
	return SpanKind(0)
}

func (m *Span) GetStatus() StatusCode {
	if m != nil {
		return m.Status
	}
	return StatusCodeUnSet
}

func (m *Span) GetLinks() []*SpanLink {
	if m != nil {
		return m.Links
	}
	return noopSpanLinks
}

func (m *Span) GetEvents() []*SpanEvent {
	if m != nil {
		return m.Events
	}
	return noopSpanEvents
}

func (m *Span) GetSize() int64 {
	if m == nil {
		return 0
	}

	var size int64

	// Calculate size of string fields
	size += int64(len(m.TraceID))
	size += int64(len(m.SpanID))
	size += int64(len(m.ParentSpanID))
	size += int64(len(m.Name))
	size += int64(len(m.TraceState))

	// Calculate size of Tags
	if m.Tags.Len() > 0 {
		sortedTags := m.Tags.SortTo(nil)
		for _, tags := range sortedTags {
			size += int64(len(tags.Key))
			size += int64(len(tags.Value))
		}
	}

	// Calculate size of Links
	for _, link := range m.Links {
		if link != nil {
			size += int64(len(link.TraceID))
			size += int64(len(link.SpanID))
			size += int64(len(link.TraceState))
			if link.Tags.Len() > 0 {
				sortedTags := link.Tags.SortTo(nil)
				for _, tags := range sortedTags {
					size += int64(len(tags.Key))
					size += int64(len(tags.Value))
				}
			}
		}
	}

	// Calculate size of Events
	for _, event := range m.Events {
		if event != nil {
			size += int64(len(event.Name))
			if event.Tags.Len() > 0 {
				sortedTags := event.Tags.SortTo(nil)
				for _, tags := range sortedTags {
					size += int64(len(tags.Key))
					size += int64(len(tags.Value))
				}
			}
		}
	}

	return size
}

func (m *Span) Clone() PipelineEvent {
	if m != nil {
		return &Span{
			TraceID:           m.TraceID,
			SpanID:            m.SpanID,
			ParentSpanID:      m.ParentSpanID,
			Name:              m.Name,
			TraceState:        m.TraceState,
			StartTime:         m.StartTime,
			EndTime:           m.EndTime,
			ObservedTimestamp: m.ObservedTimestamp,
			Kind:              m.Kind,
			Status:            m.Status,
			Tags:              m.Tags,
			Links:             m.Links,
			Events:            m.Events,
		}
	}
	return nil
}
