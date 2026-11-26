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
	"github.com/alibaba/ilogtail/pkg/helper/containercenter"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"strconv"
	"sync"

	"github.com/moby/moby/api/types/events"
)

type ServiceDockerEvents struct {
	IgnoreAttributes bool
	EventQueueSize   int

	innerEventQueue chan events.Message

	shutdown  chan struct{}
	waitGroup sync.WaitGroup
	context   pipeline.Context
}

func (p *ServiceDockerEvents) Init(context pipeline.Context) (int, error) {
	p.context = context
	containercenter.Init()
	return 0, nil
}

func (p *ServiceDockerEvents) Description() string {
	return "docker event input plugin for logtail"
}

func (p *ServiceDockerEvents) clampEventQueueSize() {
	if p.EventQueueSize < 4 {
		p.EventQueueSize = 4
	}
	if p.EventQueueSize > 10000 {
		p.EventQueueSize = 10000
	}
}

func (p *ServiceDockerEvents) fire(collector pipeline.PipelineCollector, event events.Message) {
	fieldCount := 4
	if !p.IgnoreAttributes {
		fieldCount += len(event.Actor.Attributes)
	}
	log := &models.Log{
		Timestamp: uint64(event.TimeNano),
		Contents:  models.NewOrderedLogContents(fieldCount),
		Tags:      models.NewTags(),
	}
	log.Contents.Add("_time_nano_", strconv.FormatInt(event.TimeNano, 10))
	log.Contents.Add("_action_", string(event.Action))
	log.Contents.Add("_type_", string(event.Type))
	log.Contents.Add("_id_", event.Actor.ID)
	if !p.IgnoreAttributes {
		for key, value := range event.Actor.Attributes {
			if key == models.BodyKey {
				log.SetBody([]byte(value))
			} else {
				log.Contents.Add(key, value)
			}
		}
	}
	collector.Collect(models.NewGroup(models.NewMetadata(), models.NewTags()), log)
}

func (p *ServiceDockerEvents) StartService(ctx pipeline.PipelineContext) error {
	collector := ctx.Collector()
	p.clampEventQueueSize()
	p.shutdown = make(chan struct{})
	p.waitGroup.Add(1)
	p.innerEventQueue = make(chan events.Message, p.EventQueueSize)
	containercenter.RegisterDockerEventListener(p.innerEventQueue)
	defer func() {
		containercenter.UnRegisterDockerEventListener(p.innerEventQueue)
		close(p.innerEventQueue)
		p.waitGroup.Done()
	}()
	for {
		select {
		case <-p.shutdown:
			return nil
		case event := <-p.innerEventQueue:
			p.fire(collector, event)
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
	pipeline.ServiceInputs["service_docker_event"] = func() pipeline.ServiceInput {
		return &ServiceDockerEvents{
			EventQueueSize: 10,
		}
	}
}
