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

import (
	"runtime"
	"strconv"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

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

func RegisterAlarmsSerializeToPb(logGroup *protocol.LogGroup) {
	regMu.Lock()
	defer regMu.Unlock()
	for _, alarm := range RegisterAlarms {
		alarm.SerializeToPb(logGroup)
	}
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
	if len(alarmType) == 0 || !level.IsValid() {
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

func (p *Alarm) SerializeToPb(logGroup *protocol.LogGroup) {
	nowTime := time.Now()
	alarmMutex.Lock()
	for _, item := range p.AlarmMap {
		if item.Count == 0 {
			continue
		}
		log := &protocol.Log{}
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "alarm_type", Value: item.AlarmType.String()})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "alarm_level", Value: item.Level.String()})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "alarm_message", Value: item.Message})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "alarm_count", Value: strconv.Itoa(item.Count)})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "ip", Value: util.GetIPAddress()})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "os", Value: runtime.GOOS})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "ver", Value: config.BaseVersion})
		if p.Project != "" {
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "project_name", Value: p.Project})
		}
		if p.Logstore != "" {
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "category", Value: p.Logstore})
		}
		if p.Config != "" {
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "config", Value: p.Config})
		}
		protocol.SetLogTime(log, uint32(nowTime.Unix()))
		logGroup.Logs = append(logGroup.Logs, log)
		// clear after serialize
		item.Count = 0
		item.Message = ""
	}
	alarmMutex.Unlock()
}

func init() {
	GlobalAlarm = new(Alarm)
	GlobalAlarm.Init("", "", "")
	RegisterAlarms = make(map[string]*Alarm)
}
