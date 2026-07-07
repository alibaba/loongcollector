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

package sleep

import (
	"time"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
)

type FlusherSleep struct {
	SleepMS int
	context pipeline.Context
}

func (p *FlusherSleep) Init(context pipeline.Context) error {
	p.context = context
	return nil
}

func (*FlusherSleep) Description() string {
	return "stdout flusher for logtail"
}

func (p *FlusherSleep) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	return p.flushLogGroups()
}

// flushLogGroups performs the configured sleep. It is shared by the v1 Flush
// entry point and the v2 Export so that Export does not depend on Flush, which
// is planned for removal.
func (p *FlusherSleep) flushLogGroups() error {
	time.Sleep(time.Duration(p.SleepMS) * time.Millisecond)
	return nil
}

func (p *FlusherSleep) Export(groups []*models.PipelineGroupEvents, _ pipeline.PipelineContext) error {
	for _, groupEvents := range groups {
		logGroup, err := converter.PipelineGroupEventsToLogGroup(groupEvents)
		if err != nil {
			return err
		}
		if logGroup == nil || len(logGroup.Logs) == 0 {
			continue
		}
		if err := p.flushLogGroups(); err != nil {
			return err
		}
	}
	return nil
}

func (*FlusherSleep) SetUrgent(flag bool) {
}

// IsReady is ready to flush
func (*FlusherSleep) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return true
}

// Stop ...
func (*FlusherSleep) Stop() error {
	return nil
}

func init() {
	pipeline.Flushers["flusher_sleep"] = func() pipeline.Flusher {
		return &FlusherSleep{}
	}
}
