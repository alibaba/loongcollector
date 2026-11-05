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

package helper

import (
	"maps"

	"github.com/alibaba/ilogtail/pkg/models"
)

// ForEachLogTagGroup visits consecutive runs with identical Log.Tags at an
// output boundary. PipelineEventGroup and LogGroup carry log tags at group
// level, so different per-log tags need separate wire groups. Event order and
// the input model are preserved. Group tags take precedence on collisions,
// matching the converter's existing tag merge rules.
func ForEachLogTagGroup(group *models.PipelineGroupEvents, visit func(*models.PipelineGroupEvents) error) error {
	if group == nil || len(group.Events) == 0 {
		return nil
	}
	for start := 0; start < len(group.Events); {
		tags := logEventTags(group.Events[start])
		end := start + 1
		for end < len(group.Events) && maps.Equal(tags.Iterator(), logEventTags(group.Events[end]).Iterator()) {
			end++
		}
		if start == 0 && end == len(group.Events) && tags.Len() == 0 {
			return visit(group)
		}
		info := group.Group
		if tags.Len() > 0 {
			merged := models.NewTags()
			merged.AddAll(tags.Iterator())
			merged.AddAll(info.GetTags().Iterator())
			info = models.NewGroup(info.GetMetadata(), merged)
		}
		if err := visit(&models.PipelineGroupEvents{Group: info, Events: group.Events[start:end]}); err != nil {
			return err
		}
		start = end
	}
	return nil
}

func logEventTags(event models.PipelineEvent) models.Tags {
	if log, ok := event.(*models.Log); ok && log != nil && log.Tags != nil {
		return log.Tags
	}
	return models.NilStringValues
}
