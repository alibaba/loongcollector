// Copyright 2022 iLogtail Authors
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

package udpserver

import (
	"context"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

// SharedUDPServer used for multi configs, such as using in JMX fetcher
type SharedUDPServer struct {
	dispatchKey string
	lastLog     time.Time
	udp         *UDPServer
	collectors  map[string]pipeline.PipelineCollector
	lock        sync.RWMutex // mutex for register collector
}

func NewSharedUDPServer(context pipeline.Context, decoder, format, addr, dispatchKey string, maxBufferSize int) (*SharedUDPServer, error) {
	server := pipeline.ServiceInputs["service_udp_server"]().(*UDPServer)
	server.Decoder = decoder
	server.Format = format
	server.Address = addr
	server.MaxBufferSize = maxBufferSize
	if _, err := server.Init(context); err != nil {
		return nil, err
	}
	return &SharedUDPServer{
		dispatchKey: dispatchKey,
		lastLog:     time.Now(),
		udp:         server,
		collectors:  make(map[string]pipeline.PipelineCollector),
	}, nil
}

func (s *SharedUDPServer) IsRunning() bool {
	return s.udp.conn != nil
}

func (s *SharedUDPServer) Start() error {
	logger.Infof(s.udp.context.GetRuntimeContext(), "start shared udp server")
	return s.udp.doStart(s.dispatcher)
}

func (s *SharedUDPServer) Stop() error {
	logger.Infof(s.udp.context.GetRuntimeContext(), "stop shared udp server")
	return s.udp.Stop()
}

func (s *SharedUDPServer) UnregisterCollectors(key string) {
	s.lock.Lock()
	defer s.lock.Unlock()
	delete(s.collectors, key)
}

func (s *SharedUDPServer) RegisterCollectors(key string, collector pipeline.PipelineCollector) {
	s.lock.Lock()
	defer s.lock.Unlock()
	s.collectors[key] = collector
}

func (s *SharedUDPServer) dispatcher(groups ...*models.PipelineGroupEvents) {
	s.lock.RLock()
	defer s.lock.RUnlock()
	for _, group := range groups {
		for _, event := range group.Events {
			if logger.DebugFlag() {
				logger.Debug(context.Background(), "metric", event.GetName())
			}
			tag := s.cutDispatchTag(event)
			if tag == "" {
				s.logErr("dispatcher tag is empty")
				continue
			}
			collector, ok := s.collectors[tag]
			if !ok {
				s.logErr("collector not exist for " + tag)
				continue
			}
			info := models.NewGroup(models.NewMetadata(), models.NewTags())
			info.Metadata.Merge(group.Group.GetMetadata())
			info.Tags.Merge(group.Group.GetTags())
			info.DestinationLogstore = group.Group.GetDestinationLogstore()
			collector.Collect(info, event)
		}
	}
}

func (s *SharedUDPServer) logErr(errStr string) {
	if time.Since(s.lastLog).Seconds() > 10 {
		logger.Warning(s.udp.context.GetRuntimeContext(), selfmonitor.MultiHTTPServerAlarm, "not exist dispatch key", errStr)
		s.lastLog = time.Now()
	}
}

func (s *SharedUDPServer) cutDispatchTag(event models.PipelineEvent) string {
	if log, ok := event.(*models.Log); ok {
		// StatsD logs carry the packed metric labels in their contents. Remove
		// the routing label by exact key so similarly named labels survive.
		const labelsKey = "__labels__"
		contents := log.GetIndices()
		labels, ok := contents.Get(labelsKey).(string)
		if ok {
			remaining := strings.Split(labels, "|")
			for i, part := range remaining {
				key, value, found := strings.Cut(part, "#$#")
				if found && key == s.dispatchKey && value != "" {
					remaining = append(remaining[:i], remaining[i+1:]...)
					contents.Add(labelsKey, strings.Join(remaining, "|"))
					return value
				}
			}
			return ""
		}
	}
	tags := event.GetTags()
	if tags == nil {
		return ""
	}
	value := tags.Get(s.dispatchKey)
	tags.Delete(s.dispatchKey)
	return value
}
