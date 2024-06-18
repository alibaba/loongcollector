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

package validator

import (
	"fmt"
	"strconv"
	"sync"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

var logValidatorFactory = make(map[string]LogValidatorCreator)
var tagValidatorFactory = make(map[string]TagValidatorCreator)
var sysValidatorFactory = make(map[string]SystemValidatorCreator)
var counterChan chan *protocol.LogGroup

var alarmChan chan *protocol.LogGroup

var containerChan chan *protocol.LogGroup

var mu sync.Mutex

var (
	RawLogCounter        int
	ProcessedLogCounter  int
	FlushLogCounter      int
	FlushLogGroupCounter int
)

// AlarmLogs spec: map[project_{project}:logstore:{logstore}][{alarm_type}][{alarm_message}][{alarm_count}]
var AlarmLogs = make(map[string]map[string]map[string]int)

type (
	LogValidatorCreator    func(spec map[string]interface{}) (LogValidator, error)
	TagValidatorCreator    func(spec map[string]interface{}) (TagValidator, error)
	SystemValidatorCreator func(spec map[string]interface{}) (SystemValidator, error)
)

type (
	// LogValidator check each loggroup and returns Report slice when having illegal logs.
	LogValidator interface {
		doc.Doc
		// Valid the given log group and returns the reports when valid failed.
		Valid(group *protocol.LogGroup) (reports []*Report)
		// Name of LogValidator.
		Name() string
	}
	// TagValidator check each tag and returns Report slice when having illegal tag.
	TagValidator interface {
		doc.Doc
		// Valid the given group and returns the reports when valid failed.
		Valid(group *protocol.LogGroup) (reports []*Report)
		// Name of LogValidator.
		Name() string
	}
	// SystemValidator verify the total status of the system from the testing start to the end, such as compare logs count.
	SystemValidator interface {
		doc.Doc
		// Start the SystemValidator.
		Start() error
		// Valid the given group.
		Valid(group *protocol.LogGroup)
		// FetchResult return reports when find failure in the whole testing.
		FetchResult() (reports []*Report)
		// Name of SystemValidator
		Name() string
	}
)

type Report struct {
	Success   bool // used to judge the final status.
	Validator string
	Name      string
	Want      string
	Got       string
	NotFound  string
}

func (a *Report) String() string {
	msg := "VALIDATOR:" + a.Validator + " name:" + a.Name + " WANT:" + a.Want
	if a.Got != "" {
		msg += " GOT:" + a.Got
	}
	if a.NotFound != "" {
		msg += " NOTFOUND:" + a.NotFound
	}
	return msg
}

// RegisterLogValidatorCreator register a new log validator creator to the factory .
func RegisterLogValidatorCreator(name string, creator LogValidatorCreator) {
	mu.Lock()
	defer mu.Unlock()
	logValidatorFactory[name] = creator
}

// RegisterTagValidatorCreator register a new tag validator creator to the factory .
func RegisterTagValidatorCreator(name string, creator TagValidatorCreator) {
	mu.Lock()
	defer mu.Unlock()
	tagValidatorFactory[name] = creator
}

// RegisterSystemValidatorCreator register a new system validator creator to the factory.
func RegisterSystemValidatorCreator(name string, creator SystemValidatorCreator) {
	mu.Lock()
	defer mu.Unlock()
	sysValidatorFactory[name] = creator
}

// NewLogValidator create a new log validator from the factory.
func NewLogValidator(name string, cfg map[string]interface{}) (LogValidator, error) {
	mu.Lock()
	defer mu.Unlock()
	creator, ok := logValidatorFactory[name]
	if !ok {
		return nil, fmt.Errorf("cannot find %s validator", name)
	}
	return creator(cfg)
}

// NewTagValidators reate a new tag validator from the factory.
func NewTagValidators(name string, cfg map[string]interface{}) (TagValidator, error) {
	mu.Lock()
	defer mu.Unlock()
	creator, ok := tagValidatorFactory[name]
	if !ok {
		return nil, fmt.Errorf("cannot find %s validator", name)
	}
	return creator(cfg)
}

// NewSystemValidator create a new system validator from the factory.
func NewSystemValidator(name string, cfg map[string]interface{}) (SystemValidator, error) {
	mu.Lock()
	defer mu.Unlock()
	creator, ok := sysValidatorFactory[name]
	if !ok {
		return nil, fmt.Errorf("cannot find %s validator", name)
	}
	return creator(cfg)
}

func ClearCounter() {
	RawLogCounter = 0
	ProcessedLogCounter = 0
	FlushLogCounter = 0
	FlushLogGroupCounter = 0
	AlarmLogs = make(map[string]map[string]map[string]int)
}

func GetCounterChan() chan<- *protocol.LogGroup {
	return counterChan
}

func GetAlarmLogChan() chan<- *protocol.LogGroup {
	return alarmChan
}

func GetContainerLogChan() chan<- *protocol.LogGroup {
	return containerChan
}

func InitCounter() {
	counterChan = make(chan *protocol.LogGroup, 5)
	alarmChan = make(chan *protocol.LogGroup, 5)
	containerChan = make(chan *protocol.LogGroup, 5)
	go func() {
		for group := range alarmChan {
			for _, log := range group.Logs {
				var alarmType, project, category, alarmMsg string
				var num int
				for _, content := range log.Contents {
					switch content.Key {
					case "project_name":
						project = content.Value
					case "category":
						category = content.Value
					case "alarm_type":
						alarmType = content.Value
					case "alarm_count":
						num, _ = strconv.Atoi(content.Value)
					case "alarm_message":
						alarmMsg = content.Value
					}
				}
				key := fmt.Sprintf("project_%s:logstore_%s", project, category)
				if _, ok := AlarmLogs[key]; !ok {
					AlarmLogs[key] = make(map[string]map[string]int)
				}
				if _, ok := AlarmLogs[key][alarmType]; !ok {
					AlarmLogs[key][alarmType] = make(map[string]int)
				}
				AlarmLogs[key][alarmType][alarmMsg] += num
			}
		}
	}()
	go func() {
		for group := range counterChan {
			for _, log := range group.Logs {
				for _, content := range log.Contents {
					switch content.Key {
					case "raw_log":
						RawLogCounter += getValue(content.Value)
					case "processed_log":
						ProcessedLogCounter += getValue(content.Value)
					case "flush_log":
						FlushLogCounter += getValue(content.Value)
					case "flush_loggroup":
						FlushLogGroupCounter += getValue(content.Value)
					}
				}
			}
		}
	}()
}

func CloseCounter() {
	close(counterChan)
}

func getValue(sValue string) int {
	num := 0
	if sValue == "" {
		return num
	}
	if value, err := strconv.ParseInt(sValue, 10, 64); err == nil {
		num = int(value)
	} else if valueF, err := strconv.ParseFloat(sValue, 64); err == nil {
		num = int(valueF)
	}
	return num
}
