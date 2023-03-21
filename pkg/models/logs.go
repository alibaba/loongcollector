// Copyright 2023 iLogtail Authors
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

type Indices KeyValues[interface{}]

type Log struct {
	Name              string
	Level             string
	SpanID            string
	TraceID           string
	Tags              Tags
	Timestamp         uint64
	ObservedTimestamp uint64
	Offset            uint64
	Body              []byte
	Indices           Indices
}

func (m *Log) GetName() string {
	if m != nil {
		return m.Name
	}
	return ""
}

func (m *Log) SetName(name string) {
	if m != nil {
		m.Name = name
	}
}

func (m *Log) GetTags() Tags {
	if m != nil {
		return m.Tags
	}
	return NilStringValues
}

func (m *Log) GetType() EventType {
	return EventTypeLogging
}

func (m *Log) GetTimestamp() uint64 {
	if m != nil {
		return m.Timestamp
	}
	return 0
}

func (m *Log) GetObservedTimestamp() uint64 {
	if m != nil {
		return m.ObservedTimestamp
	}
	return 0
}

func (m *Log) SetObservedTimestamp(observedTimestamp uint64) {
	if m != nil {
		m.ObservedTimestamp = observedTimestamp
	}
}

func (m *Log) GetOffset() uint64 {
	if m != nil {
		return m.Offset
	}
	return 0
}

func (m *Log) SetOffset(offset uint64) {
	if m != nil {
		m.Offset = offset
	}
}

func (m *Log) GetLevel() string {
	if m != nil {
		return m.Level
	}
	return ""
}

func (m *Log) SetLevel(level string) {
	if m != nil {
		m.Level = level
	}
}

func (m *Log) GetSpanID() string {
	if m != nil {
		return m.SpanID
	}
	return ""
}

func (m *Log) SetSpanID(spanID string) {
	if m != nil {
		m.SpanID = spanID
	}
}

func (m *Log) GetTraceID() string {
	if m != nil {
		return m.TraceID
	}
	return ""
}

func (m *Log) SetTraceID(traceID string) {
	if m != nil {
		m.TraceID = traceID
	}
}

func (m *Log) GetBody() []byte {
	if m != nil {
		return m.Body
	}
	return nil
}

func (m *Log) SetBody(body []byte) {
	if m != nil {
		m.Body = body
	}
}

func (m *Log) SetIndices(indices Indices) {
	if m != nil {
		m.Indices = indices
	}
}

func (m *Log) GetIndices() Indices {
	if m != nil {
		return m.Indices
	}
	return NilInterfaceValues
}

func (m *Log) Clone() PipelineEvent {
	if m != nil {
		return &Log{
			Name:              m.Name,
			Level:             m.Level,
			SpanID:            m.SpanID,
			TraceID:           m.TraceID,
			Tags:              m.Tags,
			Timestamp:         m.Timestamp,
			ObservedTimestamp: m.ObservedTimestamp,
			Body:              m.Body,
			Indices:           m.Indices,
			Offset:            m.Offset,
		}
	}
	return nil
}
