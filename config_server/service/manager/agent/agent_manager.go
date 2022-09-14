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

package agentmanager

import (
	"sync"

	"github.com/alibaba/ilogtail/config_server/service/model"
	"github.com/alibaba/ilogtail/config_server/service/setting"
)

type AgentManager struct {
	AgentMessageList agentMessageList
}

func (a *AgentManager) Init() {
	a.AgentMessageList.Clear()
	go a.updateAgentMessage(setting.GetSetting().AgentUpdateInterval)
}

// batch write message from agent to databse

const (
	optHeartbeat  string = "HEARTBEAT"
	optAlarm      string = "ALARM"
	optStatistics string = "STATISTICS"
)

type agentMessageList struct {
	Alarm      map[string]*model.AgentAlarm
	Heartbeat  map[string]*model.Agent
	Statistics map[string]*model.RunningStatistics
	Mutex      sync.RWMutex
}

func (a *agentMessageList) Clear() {
	a.Alarm = make(map[string]*model.AgentAlarm, 0)
	a.Heartbeat = make(map[string]*model.Agent, 0)
	a.Statistics = make(map[string]*model.RunningStatistics, 0)
}

func (a *agentMessageList) Push(opt string, data interface{}) {
	a.Mutex.Lock()
	defer a.Mutex.Unlock()

	switch opt {
	case optAlarm:
		a.Alarm[data.(*model.AgentAlarm).AlarmKey] = data.(*model.AgentAlarm)
	case optHeartbeat:
		a.Heartbeat[data.(*model.Agent).AgentID] = data.(*model.Agent)
	case optStatistics:
		a.Statistics[data.(*model.RunningStatistics).AgentID] = data.(*model.RunningStatistics)
	}
}
