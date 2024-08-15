// Copyright 2022 iLogtail Authors
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
)

// PipelineCollector collect data in the plugin and send the data to the next operator
type PipelineCollector interface { //nolint

	// Collect single group and events data belonging to this group
	Collect(groupInfo *models.GroupInfo, eventList ...models.PipelineEvent)

	// CollectList collect GroupEvents list that have been grouped
	CollectList(groupEventsList ...*models.PipelineGroupEvents)

	// ToArray returns an array containing all of the PipelineGroupEvents in this collector.
	ToArray() []*models.PipelineGroupEvents

	// Observe returns a chan that can consume PipelineGroupEvents from this collector.
	Observe() chan *models.PipelineGroupEvents

	Close()
}
