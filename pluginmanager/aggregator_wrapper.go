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

package pluginmanager

import (
	"errors"
	"time"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

var errAggAdd = errors.New("loggroup queue is full")

// AggregatorWrapper wrappers Aggregator.
// It implements LogGroupQueue interface, and is passed to associated Aggregator.
// Aggregator uses Add function to pass log groups to wrapper, and then wrapper
// passes log groups to associated LogstoreConfig through channel LogGroupsChan.
// In fact, LogGroupsChan == (associated) LogstoreConfig.LogGroupsChan.
type AggregatorWrapper struct {
	Aggregator    pipeline.AggregatorV1
	Config        *LogstoreConfig
	LogGroupsChan chan *protocol.LogGroup
	Interval      time.Duration
}

// Add inserts @loggroup to LogGroupsChan if @loggroup is not empty.
// It is called by associated Aggregator.
// It returns errAggAdd when queue is full.
func (p *AggregatorWrapper) Add(loggroup *protocol.LogGroup) error {
	if len(loggroup.Logs) == 0 {
		return nil
	}
	select {
	case p.LogGroupsChan <- loggroup:
		return nil
	default:
		return errAggAdd
	}
}

// AddWithWait inserts @loggroup to LogGroupsChan, and it waits at most @duration.
// It works like Add but adds a timeout policy when log group queue is full.
// It returns errAggAdd when queue is full and timeout.
// NOTE: no body calls it now.
func (p *AggregatorWrapper) AddWithWait(loggroup *protocol.LogGroup, duration time.Duration) error {
	if len(loggroup.Logs) == 0 {
		return nil
	}
	timer := time.NewTimer(duration)
	select {
	case p.LogGroupsChan <- loggroup:
		return nil
	case <-timer.C:
		return errAggAdd
	}
}

// Run calls periodically Aggregator.Flush to get log groups from associated aggregator and
// pass them to LogstoreConfig through LogGroupsChan.
func (p *AggregatorWrapper) Run(control *pipeline.AsyncControl) {
	defer panicRecover(p.Aggregator.Description())
	for {
		exitFlag := util.RandomSleep(p.Interval, 0.1, control.CancelToken())
		logGroups := p.Aggregator.Flush()
		for _, logGroup := range logGroups {
			if len(logGroup.Logs) == 0 {
				continue
			}
			p.LogGroupsChan <- logGroup
		}
		if exitFlag {
			return
		}
	}
}
