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

package pipeline

import "github.com/alibaba/ilogtail/pkg/models"

// TopicTagKey is the group tag carried as Topic by log protocols.
const TopicTagKey = "__topic__"

// Aggregator groups events before they reach a flusher.
// Record and GetResult may run concurrently.
type Aggregator interface {
	// Init returns the flush interval in milliseconds. Zero uses the pipeline default.
	Init(Context) (int, error)
	Description() string
	// Record accepts a group and may emit complete groups immediately.
	Record(*models.PipelineGroupEvents, PipelineContext) error
	// GetResult emits pending groups on the timer and when the pipeline stops.
	GetResult(PipelineContext) error
	// Reset clears pending state.
	Reset()
}
