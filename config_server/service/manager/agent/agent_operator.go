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
	"log"
	"sync"
	"time"

	"config-server/common"
	"config-server/model"
	proto "config-server/proto"
	"config-server/store"
)

func (a *AgentManager) HeartBeat(req *proto.HeartBeatRequest, res *proto.HeartBeatResponse) (int, *proto.HeartBeatResponse) {
	agent := new(model.Agent)
	agent.AgentID = req.AgentId
	agent.AgentType = req.AgentType
	agent.Attributes.ParseProto(req.Attributes)
	agent.Tags = req.Tags
	agent.RunningStatus = req.RunningStatus
	agent.StartupTime = req.StartupTime
	agent.Interval = req.Interval
	agent.Timestamp = time.Now().UTC()

	a.AgentMessageList.Push(optHeartbeat, agent)

	res.Code = proto.RespCode_ACCEPT
	res.Message = "Send heartbeat success"
	return common.Accept.Status, res
}

var wg sync.WaitGroup

func (a *AgentManager) updateAgentMessage(interval int) {
	ticker := time.NewTicker(time.Duration(interval) * time.Second)
	defer ticker.Stop()

	for range ticker.C {
		wg.Add(1)
		go a.batchUpdateAgentMessage()
		wg.Wait()
	}
}

func (a *AgentManager) batchUpdateAgentMessage() {
	s := store.GetStore()
	b := store.CreateBacth()

	a.AgentMessageList.Mutex.Lock()
	for k, v := range a.AgentMessageList.Heartbeat {
		b.Update(common.TypeAgent, k, v)
	}
	a.AgentMessageList.Heartbeat = make(map[string]*model.Agent, 0)
	a.AgentMessageList.Mutex.Unlock()

	writeBatchErr := s.WriteBatch(b)
	if writeBatchErr != nil {
		log.Println(writeBatchErr)
	}

	wg.Done()
}
