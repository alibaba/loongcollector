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

// TEMPORARY AUTOGENERATED FILE: easyjson stub code to make the package
// compilable during generation.

package protocol

import (
	"sync"
)

type Pool struct {
	logPool       *sync.Pool
	logGroupPool  *sync.Pool
	logConentPool *sync.Pool
}

func NewPools() *Pool {
	p := new(Pool)
	p.logPool = &sync.Pool{New: func() interface{} { return new(Log) }}
	p.logGroupPool = &sync.Pool{New: func() interface{} { return new(LogGroup) }}
	p.logConentPool = &sync.Pool{New: func() interface{} { return new(Log_Content) }}
	return p
}

func (p *Pool) PutLogGroup(logGroup *LogGroup) {
	for _, l := range logGroup.Logs {
		p.PutLog(l)
	}
	logGroup.Logs = nil
	logGroup.LogTags = nil
	logGroup.Category = ""
	logGroup.MachineUUID = ""
	logGroup.Source = ""
	logGroup.Topic = ""
	p.logGroupPool.Put(logGroup)
}

func (p *Pool) PutLog(log *Log) {
	for _, c := range log.Contents {
		p.PutLogContent(c)
	}
	log.Contents = nil
	log.Time = 0
	p.logPool.Put(log)
}

func (p *Pool) PutLogContent(context *Log_Content) {
	context.Key = ""
	context.Value = ""
	p.logConentPool.Put(context)
}

func (p *Pool) GetLogContent() (context *Log_Content) {
	return p.logConentPool.Get().(*Log_Content)
}

func (p *Pool) GetLog(contentNum int) (log *Log) {
	l := p.logPool.Get().(*Log)
	l.Contents = make([]*Log_Content, 0, contentNum)
	return l
}

func (p *Pool) GetLogGroup() (group *LogGroup) {
	return p.logGroupPool.Get().(*LogGroup)
}
