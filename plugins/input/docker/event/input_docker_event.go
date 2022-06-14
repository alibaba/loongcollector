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

package event

import (
	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/helper"

	"strconv"
	"sync"
	"time"

	docker "github.com/fsouza/go-dockerclient"
)

type ServiceDockerEvents struct {
	IgnoreAttributes bool
	EventQueueSize   int

	innerEventQueue chan *docker.APIEvents

	shutdown  chan struct{}
	waitGroup sync.WaitGroup
	context   ilogtail.Context
}

func (p *ServiceDockerEvents) Init(context ilogtail.Context) (int, error) {
	p.context = context
	helper.ContainerCenterInit()
	return 0, nil
}

func (p *ServiceDockerEvents) Description() string {
	return "docker event input plugin for logtail"
}

// Collect takes in an accumulator and adds the metrics that the Input
// gathers. This is called every "interval"
func (p *ServiceDockerEvents) Collect(ilogtail.Collector) error {
	if p.EventQueueSize < 4 {
		p.EventQueueSize = 4
	}
	if p.EventQueueSize > 10000 {
		p.EventQueueSize = 10000
	}
	return nil
}

func (p *ServiceDockerEvents) fire(c ilogtail.Collector, event *docker.APIEvents) {
	key := make([]string, len(event.Actor.Attributes)+4)
	value := make([]string, len(key))
	value[0] = strconv.FormatInt(event.TimeNano, 10)
	value[1] = event.Action
	value[2] = event.Type
	value[3] = event.Actor.ID
	key[0] = "_time_nano_"
	key[1] = "_action_"
	key[2] = "_type_"
	key[3] = "_id_"

	if !p.IgnoreAttributes {
		index := 4
		for k, v := range event.Actor.Attributes {
			key[index] = k
			value[index] = v
			index++
		}
	}

	c.AddDataArray(nil, key, value, time.Unix(0, event.TimeNano))
}

// Start starts the ServiceInput's service, whatever that may be
func (p *ServiceDockerEvents) Start(c ilogtail.Collector) error {
	p.shutdown = make(chan struct{})
	p.waitGroup.Add(1)
	p.innerEventQueue = make(chan *docker.APIEvents, p.EventQueueSize)
	helper.RegisterDockerEventListener(p.innerEventQueue)
	defer func() {
		helper.UnRegisterDockerEventListener(p.innerEventQueue)
		close(p.innerEventQueue)
		p.waitGroup.Done()
	}()
	for {
		select {
		case <-p.shutdown:
			return nil
		case event := <-p.innerEventQueue:
			if event != nil {
				p.fire(c, event)
			}
		}
	}
}

// Stop stops the services and closes any necessary channels and connections
func (p *ServiceDockerEvents) Stop() error {
	close(p.shutdown)
	p.waitGroup.Wait()
	return nil
}

func init() {
	ilogtail.ServiceInputs["service_docker_event"] = func() ilogtail.ServiceInput {
		return &ServiceDockerEvents{
			EventQueueSize: 10,
		}
	}
}
