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

// Processor transforms pipeline event groups.
type Processor interface {
	// Init initializes plugin resources.
	Init(Context) error

	// Description returns a short description of the processor.
	Description() string

	// Process handles an event group and collects its output.
	Process(*models.PipelineGroupEvents, PipelineContext)
}
