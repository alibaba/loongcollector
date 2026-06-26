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

package exportutil

import "github.com/alibaba/ilogtail/pkg/models"

// EventKindSet describes which pipeline event kinds a log-only flusher handles directly.
type EventKindSet struct {
	Log    bool
	Metric bool
	Span   bool
}

// LogOnlyEventKinds is the supported-kind set for text-oriented flushers.
var LogOnlyEventKinds = EventKindSet{Log: true}

// Supports reports whether this set handles the given event type.
func (s EventKindSet) Supports(eventType models.EventType) bool {
	switch eventType {
	case models.EventTypeLogging:
		return s.Log
	case models.EventTypeMetric:
		return s.Metric
	case models.EventTypeSpan:
		return s.Span
	default:
		return false
	}
}

// PartitionEvents splits events into matched (supported) and pass-through (unsupported) slices.
func PartitionEvents(events []models.PipelineEvent, supported EventKindSet) (matched, passThrough []models.PipelineEvent) {
	for _, event := range events {
		if supported.Supports(event.GetType()) {
			matched = append(matched, event)
		} else {
			passThrough = append(passThrough, event)
		}
	}
	return matched, passThrough
}
