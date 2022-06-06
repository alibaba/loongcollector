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
	"strconv"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/protocol"
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

func RegisterAlarmsSerializeToPb(logGroup *protocol.LogGroup) {
	regMu.Lock()
	defer regMu.Unlock()
	for _, alarm := range RegisterAlarms {
		alarm.SerializeToPb(logGroup)
	}
}

type AlarmItem struct {
	Message string
	Count   int
}

type Alarm struct {
	AlarmMap map[string]*AlarmItem
	Project  string
	Logstore string
}

func (p *Alarm) Init(project, logstore string) {
	mu.Lock()
	p.AlarmMap = make(map[string]*AlarmItem)
	p.Project = project
	p.Logstore = logstore
	mu.Unlock()
}

func (p *Alarm) Update(project, logstore string) {
	mu.Lock()
	defer mu.Unlock()
	p.Project = project
	p.Logstore = logstore
}

func (p *Alarm) Record(alarmType, message string) {
	// donot record empty alarmType
	if len(alarmType) == 0 {
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

func (p *Alarm) SerializeToPb(logGroup *protocol.LogGroup) {
	nowTime := (uint32)(time.Now().Unix())
	mu.Lock()
	for alarmType, item := range p.AlarmMap {
		if item.Count == 0 {
			continue
		}
		log := &protocol.Log{}
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "project_name", Value: p.Project})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "category", Value: p.Logstore})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "alarm_type", Value: alarmType})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "alarm_count", Value: strconv.Itoa(item.Count)})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "alarm_message", Value: item.Message})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "ip", Value: GetIPAddress()})
		log.Time = nowTime
		logGroup.Logs = append(logGroup.Logs, log)
		// clear after serialize
		item.Count = 0
		item.Message = ""
	}
	mu.Unlock()
}

func init() {
	GlobalAlarm = new(Alarm)
	GlobalAlarm.Init("", "")
	RegisterAlarms = make(map[string]*Alarm)
}
