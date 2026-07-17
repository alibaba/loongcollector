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

package checker

import (
	"encoding/json"
	"fmt"
	"regexp"
	"sync"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type FlusherChecker struct {
	context pipeline.Context
	// LogGroup records data received via the v1 Flush entry point.
	LogGroup protocol.LogGroup
	// GroupEvents records data received via the v2 Export entry point. It is kept
	// as native PipelineGroupEvents rather than being converted to a v1 LogGroup.
	GroupEvents []*models.PipelineGroupEvents
	Lock        sync.RWMutex
	Ready       bool
}

func (p *FlusherChecker) Init(context pipeline.Context) error {
	p.context = context
	p.Ready = true
	return nil
}

func (*FlusherChecker) Description() string {
	return "checking flusher for logtail"
}

func (p *FlusherChecker) GetLogCount() int {
	p.Lock.Lock()
	defer p.Lock.Unlock()
	count := len(p.LogGroup.Logs)
	for _, groupEvents := range p.GroupEvents {
		count += len(groupEvents.Events)
	}
	return count
}

func (p *FlusherChecker) SetUrgent(flag bool) {
}

func (p *FlusherChecker) CheckKeyValue(key, value string) error {
	p.Lock.RLock()
	defer p.Lock.RUnlock()
	for _, log := range p.LogGroup.Logs {
		for _, content := range log.Contents {
			if key == content.GetKey() {
				if value == content.GetValue() {
					return nil
				}
				return fmt.Errorf("key : %s, expect : %s, real : %s", key, value, content.GetValue())
			}
		}
	}
	return fmt.Errorf("cannot find this key :" + key)
}

func (p *FlusherChecker) CheckKeyValueAny(key, value string) error {
	p.Lock.RLock()
	defer p.Lock.RUnlock()
	for _, log := range p.LogGroup.Logs {
		for _, content := range log.Contents {
			if key == content.GetKey() {
				if value == content.GetValue() {
					return nil
				}
			}
		}
	}
	return fmt.Errorf("cannot find this key :" + key + ", value :" + value)
}

func (p *FlusherChecker) CheckKeyValueRegex(key, valueRegex string) error {
	p.Lock.RLock()
	defer p.Lock.RUnlock()
	for _, log := range p.LogGroup.Logs {
		for _, content := range log.Contents {
			if key == content.GetKey() {
				if match, err := regexp.MatchString(valueRegex, content.GetValue()); err != nil || !match {
					return fmt.Errorf("key : %s, regex : %s not match value : %s", key, valueRegex, content.GetValue())
				}
				return nil
			}
		}
	}
	return fmt.Errorf("cannot find this key :" + key)
}

func (p *FlusherChecker) CheckEveryLog(checker func(*protocol.Log) error) error {
	p.Lock.RLock()
	defer p.Lock.RUnlock()
	for _, log := range p.LogGroup.Logs {
		if err := checker(log); err != nil {
			return err
		}
	}
	return nil
}

func (p *FlusherChecker) CheckEveryKeyValue(checker func(string, string) error) error {
	p.Lock.RLock()
	defer p.Lock.RUnlock()
	for _, log := range p.LogGroup.Logs {
		for _, content := range log.Contents {
			if err := checker(content.GetKey(), content.GetValue()); err != nil {
				return err
			}
		}
	}
	return nil
}

func (p *FlusherChecker) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	p.Lock.Lock()
	defer p.Lock.Unlock()
	for _, logGroup := range logGroupList {
		p.LogGroup.Topic = logGroup.Topic
		p.LogGroup.LogTags = logGroup.LogTags
		for _, log := range logGroup.Logs {
			buf, _ := json.Marshal(log)
			logger.Debug(p.context.GetRuntimeContext(), string(buf))
			newLog := log
			// ilogtail.CopySLSLog(log)
			p.LogGroup.Logs = append(p.LogGroup.Logs, newLog)
		}
	}
	return nil
}

// Export is the v2 pipeline entry point. It records PipelineGroupEvents directly
// for later assertion, without converting to protocol.LogGroup or going through
// Flush, both of which belong to the v1 pipeline and are planned for removal.
func (p *FlusherChecker) Export(groups []*models.PipelineGroupEvents, _ pipeline.PipelineContext) error {
	p.Lock.Lock()
	defer p.Lock.Unlock()
	for _, groupEvents := range groups {
		if groupEvents == nil || len(groupEvents.Events) == 0 {
			continue
		}
		p.GroupEvents = append(p.GroupEvents, groupEvents)
	}
	return nil
}

// IsReady is ready to flush
func (p *FlusherChecker) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return p.Ready
}

// Stop ...
func (p *FlusherChecker) Stop() error {
	return nil
}

func init() {
	pipeline.Flushers["flusher_checker"] = func() pipeline.Flusher {
		return &FlusherChecker{}
	}
}
