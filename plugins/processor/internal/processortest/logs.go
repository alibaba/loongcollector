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

package processortest

import (
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

// Log creates an event from ordered key/value pairs, like an input record.
func Log(fields ...string) *models.Log {
	if len(fields)%2 != 0 {
		panic("log fields must be key/value pairs")
	}
	log := models.NewSimpleLog(nil, models.NewTags(), 0)
	log.Contents = models.NewOrderedLogContents(len(fields) / 2)
	for i := 0; i < len(fields); i += 2 {
		log.Contents.Add(fields[i], fields[i+1])
	}
	return log
}

// Apply runs a processor on a group of logs and returns the logs it emits.
func Apply(processor pipeline.Processor, logs ...*models.Log) []*models.Log {
	events := make([]models.PipelineEvent, len(logs))
	for i, log := range logs {
		events[i] = log
	}
	context := helper.NewGroupedPipelineContext()
	processor.Process(&models.PipelineGroupEvents{Events: events}, context)
	result := make([]*models.Log, 0, len(logs))
	for _, group := range context.Collector().ToArray() {
		for _, event := range group.Events {
			result = append(result, event.(*models.Log))
		}
	}
	return result
}
