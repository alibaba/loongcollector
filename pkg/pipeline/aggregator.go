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

import (
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

// Aggregator is an interface for implementing an Aggregator plugin.
// the RunningAggregator wraps this interface and guarantees that
type Aggregator interface {
	// Init called for init some system resources, like socket, mutex...
	// return flush interval(ms) and error flag, if interval is 0, use default interval
	Init(Context, LogGroupQueue) (int, error)

	// Description returns a one-sentence description on the Input.
	Description() string

	// Reset resets the aggregators caches and aggregates.
	Reset()
}

// AggregatorV1
// Add, Flush, and Reset can not be called concurrently, so locking is not
// required when implementing an Aggregator plugin.
type AggregatorV1 interface {
	Aggregator
	// Add the metric to the aggregator.
	Add(log *protocol.Log, ctx map[string]interface{}) error

	// Flush pushes the current aggregates to the accumulator.
	Flush() []*protocol.LogGroup
}

// AggregatorV2
// Apply, Push, and Reset can not be called concurrently, so locking is not
// required when implementing an Aggregator plugin.
type AggregatorV2 interface {
	Aggregator
	// Add the metric to the aggregator.
	Record(*models.PipelineGroupEvents, PipelineContext) error
	// GetResult the current aggregates to the accumulator.
	GetResult(PipelineContext) error
}
