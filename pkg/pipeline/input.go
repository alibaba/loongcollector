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

// MetricInput collects events on a schedule.
type MetricInput interface {
	// Init returns the collection interval in milliseconds. Zero uses the default.
	Init(Context) (int, error)
	Description() string
	Read(PipelineContext) error
}

// ServiceInput runs a service that submits events to the pipeline.
type ServiceInput interface {
	Init(Context) (int, error)
	Description() string
	StartService(PipelineContext) error
	Stop() error
}

// PipelineRemovedCleaner is an optional interface a ServiceInput may implement to run
// cleanup when its pipeline config is permanently removed. It is invoked ONLY on genuine
// config removal — not on a reload/restart of the same config, and not on process
// shutdown — so plugins can safely tear down host-global external state they installed
// (e.g. an rsyslog forwarding drop-in) without disturbing it on ordinary reloads.
type PipelineRemovedCleaner interface {
	// OnPipelineRemoved is called after the pipeline has stopped and just before its
	// config is deleted. Implementations must not block for long.
	OnPipelineRemoved()
}
