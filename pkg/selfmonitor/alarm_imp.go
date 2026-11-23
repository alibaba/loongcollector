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

package selfmonitor

import "sync"

var GlobalAlarm *Alarm
var alarmMutex sync.Mutex

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

func RegisterAlarmsSerializeToMessages(results *[]AlarmExportMessage) {
	regMu.Lock()
	defer regMu.Unlock()
	for _, alarm := range RegisterAlarms {
		alarm.SerializeToMessages(results)
	}
}

type AlarmExportMessage struct {
	AlarmType    string
	AlarmLevel   string
	AlarmMessage string
	ProjectName  string
	Category     string
	Config       string
	Count        int
}

type AlarmItem struct {
	AlarmType AlarmType
	Level     AlarmLevel
	Message   string
	Count     int
}

type Alarm struct {
	AlarmMap map[string]*AlarmItem
	Project  string
	Logstore string
	Config   string
}

func (p *Alarm) Init(project, logstore, config string) {
	alarmMutex.Lock()
	p.AlarmMap = make(map[string]*AlarmItem)
	p.Project = project
	p.Logstore = logstore
	p.Config = config
	alarmMutex.Unlock()
}

func (p *Alarm) Update(project, logstore, config string) {
	alarmMutex.Lock()
	defer alarmMutex.Unlock()
	p.Project = project
	p.Logstore = logstore
	p.Config = config
}

func (p *Alarm) Record(alarmType AlarmType, level AlarmLevel, message string) {
	// donot record empty alarmType
	if alarmType.String() == "" || !level.IsValid() {
		return
	}
	alarmMutex.Lock()

	alarmKey := alarmType.String() + "_" + level.String()
	alarmItem, existFlag := p.AlarmMap[alarmKey]
	if !existFlag {
		alarmItem = &AlarmItem{
			AlarmType: alarmType,
			Level:     level,
		}
		p.AlarmMap[alarmKey] = alarmItem
	}
	alarmItem.Message = message
	alarmItem.Count++
	alarmMutex.Unlock()
}

func (p *Alarm) SerializeToMessages(results *[]AlarmExportMessage) {
	alarmMutex.Lock()
	defer alarmMutex.Unlock()
	for _, item := range p.AlarmMap {
		if item.Count == 0 {
			continue
		}
		*results = append(*results, AlarmExportMessage{
			AlarmType:    item.AlarmType.String(),
			AlarmLevel:   item.Level.String(),
			AlarmMessage: item.Message,
			ProjectName:  p.Project,
			Category:     p.Logstore,
			Config:       p.Config,
			Count:        item.Count,
		})
		item.Count = 0
		item.Message = ""
	}
}

func init() {
	GlobalAlarm = new(Alarm)
	GlobalAlarm.Init("", "", "")
	RegisterAlarms = make(map[string]*Alarm)
}
