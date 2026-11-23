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

package checker

import (
	"sync"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

type FlusherChecker struct {
	// GroupEvents records event groups received from the pipeline.
	GroupEvents []*models.PipelineGroupEvents
	Lock        sync.RWMutex
	Ready       bool
}

func (p *FlusherChecker) Init(context pipeline.Context) error {
	p.Ready = true
	return nil
}

func (*FlusherChecker) Description() string {
	return "checking flusher for logtail"
}

func (p *FlusherChecker) GetLogCount() int {
	p.Lock.RLock()
	defer p.Lock.RUnlock()
	count := 0
	for _, groupEvents := range p.GroupEvents {
		count += len(groupEvents.Events)
	}
	return count
}

func (p *FlusherChecker) SetUrgent(flag bool) {
}

// Export saves groups for test assertions.
func (p *FlusherChecker) Export(groups []*models.PipelineGroupEvents, _ pipeline.PipelineContext) error {
	p.Lock.Lock()
	defer p.Lock.Unlock()
	for _, groupEvents := range groups {
		if groupEvents == nil || len(groupEvents.Events) == 0 {
			continue
		}
		p.GroupEvents = append(p.GroupEvents, groupEvents)
	}
	return nil
}

// IsReady is ready to flush
func (p *FlusherChecker) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return p.Ready
}

// Stop ...
func (p *FlusherChecker) Stop() error {
	return nil
}

func init() {
	pipeline.Flushers["flusher_checker"] = func() pipeline.Flusher {
		return &FlusherChecker{}
	}
}
