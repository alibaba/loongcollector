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

// MetricInput ...
type MetricInput interface {
	// Init called for init some system resources, like socket, mutex...
	// return call interval(ms) and error flag, if interval is 0, use default interval
	Init(Context) (int, error)

	// Description returns a one-sentence description on the Input
	Description() string
}

type MetricInputV1 interface {
	MetricInput
	// Collect takes in an accumulator and adds the metrics that the Input
	// gathers. This is called every "interval"
	Collect(Collector) error
}

type MetricInputV2 interface {
	MetricInput
	// Collect takes in an accumulator and adds the metrics that the Input
	// gathers. This is called every "interval"
	Read(PipelineContext) error
}

// ServiceInput ...
type ServiceInput interface {
	// Init called for init some system resources, like socket, mutex...
	// return interval(ms) and error flag, if interval is 0, use default interval
	Init(Context) (int, error)

	// Description returns a one-sentence description on the Input
	Description() string

	// Stop stops the services and closes any necessary channels and connections
	Stop() error
}

type ServiceInputV1 interface {
	ServiceInput
	// Start starts the ServiceInput's service, whatever that may be
	Start(Collector) error
}

type ServiceInputV2 interface {
	ServiceInput
	// StartService starts the ServiceInput's service, whatever that may be
	StartService(PipelineContext) error
}
