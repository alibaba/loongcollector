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

// Processor also can be a filter
type Processor interface {
	// Init called for init some system resources, like socket, mutex...
	Init(pipelineContext Context) error

	// Description returns a one-sentence description on the Input
	Description() string
}

type ProcessorV1 interface {
	Processor
	// ProcessLogs the filter to the given metric
	ProcessLogs(logArray []*protocol.Log) []*protocol.Log
}

type ProcessorV2 interface {
	Processor
	Process(in *models.PipelineGroupEvents, context PipelineContext)
}
