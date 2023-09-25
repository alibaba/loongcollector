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

package mockd

import (
	"os"
	"strconv"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

type ServiceMock struct {
	shutdown      chan struct{}
	waitGroup     sync.WaitGroup
	Tags          map[string]string
	Fields        map[string]string
	File          string
	Index         int64
	LogsPerSecond int
	MaxLogCount   int
	nowLogCount   int
	context       pipeline.Context
}

func (p *ServiceMock) Init(context pipeline.Context) (int, error) {
	p.context = context
	if len(p.File) > 0 {
		if content, _ := os.ReadFile(p.File); len(content) > 0 {
			if p.Fields == nil {
				p.Fields = make(map[string]string)
			}
			p.Fields["content"] = string(content)
		}
	}
	return 0, nil
}

func (p *ServiceMock) Description() string {
	return "mock service input plugin for logtail"
}

// Gather takes in an accumulator and adds the metrics that the Input
// gathers. This is called every "interval"
func (p *ServiceMock) Collect(pipeline.Collector) error {
	return nil
}

// Start starts the ServiceInput's service, whatever that may be
func (p *ServiceMock) Start(c pipeline.Collector) error {
	p.shutdown = make(chan struct{})
	p.waitGroup.Add(1)
	defer p.waitGroup.Done()
	for {
		beginTime := time.Now()
		for i := 0; i < p.LogsPerSecond; i++ {
			p.MockOneLog(c)
		}
		nowTime := time.Now()
		sleepTime := time.Millisecond // make sure p.shutdown has high priority
		if nowTime.Sub(beginTime) < time.Second {
			sleepTime = time.Second - nowTime.Sub(beginTime)
		}
		logger.Info(p.context.GetRuntimeContext(), "input logs", p.LogsPerSecond, "cost", nowTime.Sub(beginTime), "sleep", sleepTime)
		p.nowLogCount += p.LogsPerSecond
		if p.MaxLogCount > 0 && p.nowLogCount >= p.MaxLogCount {
			logger.Info(p.context.GetRuntimeContext(), "input logs", p.nowLogCount, "done")
			return nil
		}
		timer := time.NewTimer(sleepTime)
		select {
		case <-p.shutdown:
			return nil
		case <-timer.C:
		}
	}
}

func (p *ServiceMock) MockOneLog(c pipeline.Collector) {
	fields := p.Fields
	p.Index++
	fields["Index"] = strconv.FormatInt(p.Index, 10)
	c.AddData(p.Tags, p.Fields)
}

// Stop stops the services and closes any necessary channels and connections
func (p *ServiceMock) Stop() error {
	close(p.shutdown)
	p.waitGroup.Wait()
	return nil
}

func init() {
	pipeline.ServiceInputs["service_mock"] = func() pipeline.ServiceInput {
		return &ServiceMock{Index: 0}
	}
}
