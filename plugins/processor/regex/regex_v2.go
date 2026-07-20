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

package regex

import (
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

// Process implements the v2 ProcessorV2 interface so this plugin can load in a
// v2 (models.PipelineGroupEvents) pipeline. It has no v2-native processing yet
// and therefore explicitly passes all events (Log/Metric/Span) through
// unchanged, rather than leaving v2 support undefined.
func (p *ProcessorRegex) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	pipeline.CollectGroupEvents(context, in)
}
