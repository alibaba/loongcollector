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
	"strconv"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/config_server/service/common"
	"github.com/alibaba/ilogtail/config_server/service/model"
	proto "github.com/alibaba/ilogtail/config_server/service/proto"
	"github.com/alibaba/ilogtail/config_server/service/store"
)

func (a *AgentManager) HeartBeat(req *proto.HeartBeatRequest, res *proto.HeartBeatResponse) (int, *proto.HeartBeatResponse) {
	queryTime := time.Now().Unix()
	agent := new(model.Agent)
	agent.AgentID = req.AgentId
	agent.AgentType = req.AgentType
	agent.Version = req.AgentVersion
	agent.IP = req.Ip
	agent.Tags = req.Tags
	agent.RunningStatus = req.RunningStatus
	agent.StartupTime = req.StartupTime
	agent.LatestHeartbeatTime = queryTime

	a.AgentMessageList.Push(optHeartbeat, agent)

	res.Code = common.Accept.Code
	res.Message = "Send heartbeat success"
	return common.Accept.Status, res
}

func (a *AgentManager) RunningStatistics(req *proto.RunningStatisticsRequest, res *proto.RunningStatisticsResponse) (int, *proto.RunningStatisticsResponse) {
	agentStatus := new(model.RunningStatistics)
	agentStatus.ParseProto(req.RunningDetails)
	a.AgentMessageList.Push(optStatistics, agentStatus)

	res.Code = common.Accept.Code
	res.Message = "Send running statistics success"
	return common.Accept.Status, res
}

func (a *AgentManager) Alarm(req *proto.AlarmRequest, res *proto.AlarmResponse) (int, *proto.AlarmResponse) {
	queryTime := strconv.FormatInt(time.Now().Unix(), 10)
	alarm := new(model.AgentAlarm)
	alarm.AlarmKey = generateAlarmKey(queryTime, req.AgentId)
	alarm.AlarmTime = queryTime
	alarm.AlarmType = req.Type
	alarm.AlarmMessage = req.Detail
	a.AgentMessageList.Push(optAlarm, alarm)

	res.Code = common.Accept.Code
	res.Message = "Alarm success"
	return common.Accept.Status, res
}

var wg sync.WaitGroup

func (a *AgentManager) updateAgentMessage(interval int) {
	ticker := time.NewTicker(time.Duration(interval) * time.Second)
	defer ticker.Stop()

	for range ticker.C {
		wg.Add(3)
		go a.batchAddAlarm()
		go a.batchUpdateAgentMessage()
		go a.releaseAlarm()
		wg.Wait()
	}
}

func (a *AgentManager) batchAddAlarm() {
	s := store.GetStore()
	b := store.CreateBacth()

	a.AgentMessageList.Mutex.Lock()
	for k, v := range a.AgentMessageList.Alarm {
		b.Add(common.TypeAgentAlarm, k, v)
	}
	a.AgentMessageList.Alarm = make(map[string]*model.AgentAlarm, 0)
	a.AgentMessageList.Mutex.Unlock()

	writeBatchErr := s.WriteBatch(b)
	if writeBatchErr != nil {
		log.Println(writeBatchErr)
		return
	}

	wg.Done()
}

func (a *AgentManager) releaseAlarm() {
	s := store.GetStore()
	b := store.CreateBacth()

	alarmCount, countErr := s.Count(common.TypeAgentAlarm)
	if countErr != nil {
		log.Println(countErr)
		return
	}
	if alarmCount > 10000 {
		alarmList, getAllErr := s.GetAll(common.TypeAgentAlarm)
		if getAllErr != nil {
			log.Println(getAllErr)
			return
		}
		for i, v := range alarmList {
			if i > 5000 {
				break
			}
			b.Delete(common.TypeAgentAlarm, generateAlarmKey(v.(*model.AgentAlarm).AlarmTime, v.(*model.AgentAlarm).AlarmKey))
		}
	}

	writeBatchErr := s.WriteBatch(b)
	if writeBatchErr != nil {
		log.Println(writeBatchErr)
		return
	}

	wg.Done()
}

func (a *AgentManager) batchUpdateAgentMessage() {
	s := store.GetStore()
	b := store.CreateBacth()

	a.AgentMessageList.Mutex.Lock()
	for k, v := range a.AgentMessageList.Heartbeat {
		b.Update(common.TypeAgent, k, v)
	}
	a.AgentMessageList.Heartbeat = make(map[string]*model.Agent, 0)
	for k, v := range a.AgentMessageList.Statistics {
		b.Update(common.TypeRunningStatistics, k, v)
	}
	a.AgentMessageList.Statistics = make(map[string]*model.RunningStatistics, 0)
	a.AgentMessageList.Mutex.Unlock()

	writeBatchErr := s.WriteBatch(b)
	if writeBatchErr != nil {
		log.Println(writeBatchErr)
	}

	wg.Done()
}

func generateAlarmKey(queryTime string, agentID string) string {
	return queryTime + ":" + agentID
}
