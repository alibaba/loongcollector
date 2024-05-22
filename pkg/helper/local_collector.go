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

package helper

import (
	"github.com/alibaba/ilogtail/pkg/protocol"

	"time"
)

// LocalCollector for unit test
type LocalCollector struct {
	Logs []*protocol.Log
}

func (p *LocalCollector) AddData(tags map[string]string, fields map[string]string, t ...time.Time) {
	p.AddDataWithContext(tags, fields, nil, t...)
}

func (p *LocalCollector) AddDataArray(tags map[string]string,
	columns []string,
	values []string,
	t ...time.Time) {
	p.AddDataArrayWithContext(tags, columns, values, nil, t...)
}

func (p *LocalCollector) AddRawLog(log *protocol.Log) {
	p.AddRawLogWithContext(log, nil)
}

func (p *LocalCollector) AddDataWithContext(tags map[string]string, fields map[string]string, ctx map[string]interface{}, t ...time.Time) {
	// log.Printf("Begin add %v %v", tags, fields)
	var logTime time.Time
	if len(t) == 0 {
		logTime = time.Now()
	} else {
		logTime = t[0]
	}
	slsLog, _ := CreateLog(logTime, len(t) != 0, nil, tags, fields)
	p.Logs = append(p.Logs, slsLog)
}

func (p *LocalCollector) AddDataArrayWithContext(tags map[string]string,
	columns []string,
	values []string,
	ctx map[string]interface{},
	t ...time.Time) {
	var logTime time.Time
	if len(t) == 0 {
		logTime = time.Now()
	} else {
		logTime = t[0]
	}
	slsLog, _ := CreateLogByArray(logTime, len(t) != 0, nil, tags, columns, values)
	p.Logs = append(p.Logs, slsLog)
}

func (p *LocalCollector) AddRawLogWithContext(log *protocol.Log, ctx map[string]interface{}) {
	p.Logs = append(p.Logs, log)
}
