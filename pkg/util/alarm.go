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

package util

import (
	"sync"
)

var GlobalAlarm *Alarm
var mu sync.Mutex

var RegisterAlarms map[string]*Alarm
var regMu sync.Mutex

func RegisterAlarm(key string, alarm *Alarm) {
	regMu.Lock()
	defer regMu.Unlock()
	RegisterAlarms[key] = alarm
}

func DeleteAlarm(key string) {
	regMu.Lock()
	defer regMu.Unlock()
	delete(RegisterAlarms, key)
}

func RegisterAlarmsSerializeToMessage() []InternalAlarmMessage {
	regMu.Lock()
	defer regMu.Unlock()

	alarmMessages := []InternalAlarmMessage{}
	for _, alarm := range RegisterAlarms {
		alarmMessages = append(alarmMessages, alarm.SerializeToMessage()...)
	}
	return alarmMessages
}

type InternalAlarmMessage struct {
	AlarmType int
	Project   string
	Logstore  string
	Config    string
	Message   string
	Count     int
}

type AlarmItem struct {
	Message string
	Count   int
}

type Alarm struct {
	AlarmMap map[AlarmType]*AlarmItem
	Project  string
	Logstore string
	Config   string
}

func (p *Alarm) Init(project, logstore, config string) {
	mu.Lock()
	p.AlarmMap = make(map[AlarmType]*AlarmItem)
	p.Project = project
	p.Logstore = logstore
	p.Config = config
	mu.Unlock()
}

func (p *Alarm) Update(project, logstore, config string) {
	mu.Lock()
	defer mu.Unlock()
	p.Project = project
	p.Logstore = logstore
	p.Config = config
}

func (p *Alarm) Record(alarmType AlarmType, message string) {
	// donot record empty alarmType
	if alarmType < 0 || alarmType >= ALL_LOGTAIL_ALARM_NUM {
		return
	}
	mu.Lock()
	alarmItem, existFlag := p.AlarmMap[alarmType]
	if !existFlag {
		alarmItem = &AlarmItem{}
		p.AlarmMap[alarmType] = alarmItem
	}
	alarmItem.Message = message
	alarmItem.Count++
	mu.Unlock()
}

func (p *Alarm) SerializeToMessage() []InternalAlarmMessage {
	alarmMessages := []InternalAlarmMessage{}
	mu.Lock()
	for alarmType, item := range p.AlarmMap {
		if item.Count == 0 {
			continue
		}
		alarmMessages = append(alarmMessages, InternalAlarmMessage{
			AlarmType: int(alarmType),
			Project:   p.Project,
			Logstore:  p.Logstore,
			Config:    p.Config,
			Message:   item.Message,
			Count:     item.Count,
		})
		// clear after serialize
		item.Count = 0
		item.Message = ""
	}
	mu.Unlock()
	return alarmMessages
}

func init() {
	GlobalAlarm = new(Alarm)
	GlobalAlarm.Init("", "", "")
	RegisterAlarms = make(map[string]*Alarm)
}
