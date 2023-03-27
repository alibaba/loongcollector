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
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const labelName = "__labels__"

// SharedUDPServer used for multi configs, such as using in JMX fetcher
type SharedUDPServer struct {
	dispatchKey string
	lastLog     time.Time
	udp         *UDPServer
	collectors  map[string]pipeline.Collector
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
		collectors:  make(map[string]pipeline.Collector),
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

func (s *SharedUDPServer) RegisterCollectors(key string, collector pipeline.Collector) {
	s.lock.Lock()
	defer s.lock.Unlock()
	s.collectors[key] = collector
}

func (s *SharedUDPServer) dispatcher(logs []*protocol.Log) {
	s.lock.RLock()
	defer s.lock.RUnlock()
	for _, log := range logs {
		if logger.DebugFlag() {
			logger.Debug(context.Background(), "log", log.String())
		}
		tag := s.cutDispatchTag(log)
		if tag == "" {
			s.logErr("dispatcher tag is empty")
			continue
		}
		collector, ok := s.collectors[tag]
		if !ok {
			s.logErr("collector not exist for " + tag)
			continue
		}
		collector.AddRawLog(log)
	}
}

func (s *SharedUDPServer) logErr(errStr string) {
	if time.Since(s.lastLog).Seconds() > 10 {
		logger.Error(s.udp.context.GetRuntimeContext(), "MULTI_HTTP_SERVER_ALARM", "not exist dispatch key", errStr)
		s.lastLog = time.Now()
	}
}

func (s *SharedUDPServer) cutDispatchTag(log *protocol.Log) (tag string) {
	for _, content := range log.Contents {
		if content.Key == "__labels__" {
			startIdx := strings.Index(content.Value, s.dispatchKey)
			if startIdx == -1 {
				s.logErr("dispatch key not exist")
				break
			}
			endIdx := startIdx + len(s.dispatchKey) + len("#$#")
			if endIdx >= len(content.Value) {
				s.logErr("dispatch key over range")
				break
			}
			str := content.Value[endIdx:]
			offset := strings.Index(str, "|")
			if offset == -1 {
				tag = str
				content.Value = content.Value[:startIdx-1]
			} else {
				tag = str[:offset]
				content.Value = content.Value[:startIdx] + content.Value[endIdx+offset+1:]
			}
			break
		}
	}
	if tag == "" {
		s.logErr("not find any dispatch key" + log.String())
	}
	return
}
