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

package pipeline

import (
	"fmt"

	"github.com/alibaba/ilogtail/pkg/models"
)

// Event pass-through contract for v2 Processor/Flusher plugins:
//
//   - Plugins that only handle a subset of event kinds MUST NOT silently drop the rest.
//   - Log-only processors MUST pass Metric and Span events through unchanged.
//   - Prefer CollectGroupEvents or ProcessLogEventsOnly to satisfy the contract.
//   - When matched events are transformed 1:1 (one processed event per matched event),
//     use RecombineEvents to preserve original order. RecombineEvents does NOT support
//     filtering/dropping matched events; for that, use PartitionEvents and rebuild the
//     output slice manually.
type EventKindSet struct {
	Log    bool
	Metric bool
	Span   bool
}

// LogOnlyEventKinds is the supported-kind set for text-parsing style processors.
var LogOnlyEventKinds = EventKindSet{Log: true}

// AllPipelineEventKinds covers Log, Metric, and Span pipeline events.
var AllPipelineEventKinds = EventKindSet{Log: true, Metric: true, Span: true}

// Supports reports whether this set handles the given event type.
// Unknown kinds (e.g. EventTypeByteArray) are not supported and should pass through.
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
// Order within each slice follows the original order.
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

// PassThroughEvents returns events whose kinds are not handled by handledKinds.
func PassThroughEvents(events []models.PipelineEvent, handledKinds EventKindSet) []models.PipelineEvent {
	_, passThrough := PartitionEvents(events, handledKinds)
	return passThrough
}

// RecombineEvents merges processed matched events back into their original positions,
// preserving the order and identity of pass-through (unhandled) events.
//
// Contract: processedMatched MUST contain exactly one entry per matched event, in original
// order (strict 1:1 transformation). RecombineEvents is NOT a filtering primitive and cannot
// represent dropped events; callers that filter must rebuild the slice via PartitionEvents.
//
// If original is empty, it is returned as-is (including an empty-but-non-nil slice) so the
// caller's slice identity and emptiness are preserved rather than collapsed to nil.
//
// Defensive fallback when the 1:1 precondition is violated (indicates a caller bug, never
// panics):
//   - If processedMatched is shorter than the matched count, the surplus matched positions
//     keep their original events.
//   - If processedMatched is longer than the matched count, the extra trailing entries are
//     ignored.
func RecombineEvents(original []models.PipelineEvent, handledKinds EventKindSet, processedMatched []models.PipelineEvent) []models.PipelineEvent {
	if len(original) == 0 {
		return original
	}
	result := make([]models.PipelineEvent, len(original))
	matchedIdx := 0
	for i, event := range original {
		if handledKinds.Supports(event.GetType()) {
			if matchedIdx < len(processedMatched) {
				result[i] = processedMatched[matchedIdx]
				matchedIdx++
			} else {
				result[i] = event
			}
			continue
		}
		result[i] = event
	}
	return result
}

// ApplyToSupportedEvents invokes fn on each event whose kind is in supported.
// Unsupported events are left untouched (implicit pass-through when the slice is collected as-is).
func ApplyToSupportedEvents(events []models.PipelineEvent, supported EventKindSet, fn func(models.PipelineEvent)) {
	for _, event := range events {
		if supported.Supports(event.GetType()) {
			fn(event)
		}
	}
}

// CollectGroupEvents emits all events in the group without filtering.
func CollectGroupEvents(ctx PipelineContext, in *models.PipelineGroupEvents) {
	if in == nil || len(in.Events) == 0 {
		return
	}
	ctx.Collector().Collect(in.Group, in.Events...)
}

// GetStringValue converts a v2 LogContents value to a string. A v2 log body is
// stored under the "content" key as []byte (see models.Log.SetBody and the
// C++->Go bridge in pluginv2Runner.convertToPipelineEvent), so callers that
// treat a field as text MUST handle []byte: fmt.Sprintf("%v", []byte(...))
// renders the byte numbers (e.g. "[108 101 ...]") instead of the string, which
// silently breaks text-parsing processors (csv/grok/regex/split_*). Non-string,
// non-[]byte values fall back to fmt.Sprint to preserve prior behavior.
func GetStringValue(v interface{}) string {
	switch tv := v.(type) {
	case string:
		return tv
	case []byte:
		return string(tv)
	default:
		return fmt.Sprint(tv)
	}
}

// ProcessLogEventsOnly runs processLog on Log events and collects all events (Metric/Span pass through).
func ProcessLogEventsOnly(in *models.PipelineGroupEvents, ctx PipelineContext, processLog func(*models.Log)) {
	if in == nil {
		return
	}
	for _, event := range in.Events {
		if event.GetType() == models.EventTypeLogging {
			processLog(event.(*models.Log))
		}
	}
	CollectGroupEvents(ctx, in)
}
