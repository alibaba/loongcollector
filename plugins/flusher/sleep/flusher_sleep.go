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
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"time"
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
	time.Sleep(time.Duration(p.SleepMS) * time.Millisecond)
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
