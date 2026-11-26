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

package pluginmanager

import (
	"context"
	"fmt"
	"math/rand"
	"sync"
	"time"

	"golang.org/x/sync/semaphore"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/pkg/util"
)

type timerRunner struct {
	initialMaxDelay time.Duration
	interval        time.Duration
	context         pipeline.Context
	state           interface{}
}

func (p *timerRunner) Run(task func(state interface{}) error, cc *pipeline.AsyncControl) {
	pluginType := fmt.Sprintf("%T", p.state)
	logger.Info(p.context.GetRuntimeContext(), "task run", "start", "interval", p.interval, "max delay", p.initialMaxDelay, "state", pluginType)
	defer panicRecover(pluginType)

	exitFlag := false
	if p.initialMaxDelay > 0 {
		if p.initialMaxDelay > p.interval {
			logger.Infof(p.context.GetRuntimeContext(), "initial collect delay is larger than than interval, use interval %v instead", p.interval)
			p.initialMaxDelay = p.interval
		}
		/* #nosec G404 */
		exitFlag = util.RandomSleep(time.Duration(rand.Int63n(int64(p.initialMaxDelay))), 0, cc.CancelToken())
	}

	for {
		startTime := time.Now()
		p.execTask(task) // execute task at least once.
		executeTime := time.Since(startTime)
		if exitFlag {
			logger.Info(p.context.GetRuntimeContext(), "task run", "exit", "state", fmt.Sprintf("%T", p.state))
			return
		}
		timeToSleep := GetNextDelay(p.interval, executeTime)
		if timeToSleep == 0 {
			logger.Warningf(p.context.GetRuntimeContext(), selfmonitor.TaskExecuteSlowAlarm, "task execute time %v is larger than interval %v, skip sleep", executeTime, p.interval)
		}
		exitFlag = util.RandomSleep(timeToSleep, 0, cc.CancelToken())
	}
}

func GetNextDelay(expectedInterval time.Duration, executeTime time.Duration) time.Duration {
	if executeTime > expectedInterval {
		return 0
	}
	return expectedInterval - executeTime
}

func (p *timerRunner) execTask(task func(state interface{}) error) {
	if err := task(p.state); err != nil {
		logger.Error(p.context.GetRuntimeContext(), selfmonitor.PluginRunAlarm, "task run", "error", err, "plugin", "state", fmt.Sprintf("%T", p.state))
	}
}

func flushOutStore[T FlushData, F FlusherWrapperInterface](lc *LogstoreConfig, store *FlushOutStore[T], flushers []F, flushFunc func(*LogstoreConfig, F, *FlushOutStore[T]) error) bool {
	for _, flusher := range flushers {
		for waitCount := 0; !flusher.IsReady(lc.ProjectName, lc.LogstoreName, lc.LogstoreKey); waitCount++ {
			if waitCount > maxFlushOutTime*100 {
				logger.Error(lc.Context.GetRuntimeContext(), selfmonitor.DropDataAlarm, "flush out data timeout, drop data", store.Len())
				return false
			}
			time.Sleep(time.Duration(10) * time.Millisecond)
		}
		err := flushFunc(lc, flusher, store)
		if err != nil {
			logger.Error(lc.Context.GetRuntimeContext(), selfmonitor.FlushDataAlarm, "flush data error", lc.ProjectName, lc.LogstoreName, err)
		}
	}
	store.Reset()
	return true
}

func GetFlushStoreLen(runner PluginRunner) int {
	if r, ok := runner.(*pluginRunner); ok {
		return r.FlushOutStore.Len()
	}
	return 0
}

func GetFlushCancelToken(runner PluginRunner) <-chan struct{} {
	if r, ok := runner.(*pluginRunner); ok {
		return r.FlushControl.CancelToken()
	}
	return make(<-chan struct{})
}

func GetConfigInputs(runner PluginRunner) []pipeline.ServiceInput {
	inputs := make([]pipeline.ServiceInput, 0)
	if r, ok := runner.(*pluginRunner); ok {
		for _, input := range r.ServicePlugins {
			inputs = append(inputs, input.Input)
		}
	}
	return inputs
}

func GetConfigFlushers(runner PluginRunner) []pipeline.Flusher {
	flushers := make([]pipeline.Flusher, 0)
	if r, ok := runner.(*pluginRunner); ok {
		for _, f := range r.FlusherPlugins {
			flushers = append(flushers, f.Flusher)
		}
	}
	return flushers
}

func pluginUnImplementError(category pluginCategory, pluginType string) error {
	return fmt.Errorf("plugin does not implement %s event interface. pluginType: %s", category, pluginType)
}

func pluginCategoryUndefinedError(category pluginCategory) error {
	return fmt.Errorf("undefined plugin category : %s", category)
}

const fileInputWireBudget = 2 << 20

// reservedInputQueue counts file groups against the existing queue capacity
// before decoding. Native workers waiting for capacity retain only their
// borrowed wire data until the processor takes another group.
type reservedInputQueue struct {
	pipeline.PipelineCollector
	slots      chan struct{}
	wireBudget *semaphore.Weighted
	weights    sync.Map // *models.PipelineGroupEvents -> int64
}

func newReservedInputQueue(input pipeline.PipelineContext, capacity int) *reservedInputQueue {
	return &reservedInputQueue{
		PipelineCollector: input.Collector(),
		slots:             make(chan struct{}, capacity),
		wireBudget:        semaphore.NewWeighted(fileInputWireBudget),
	}
}

func (q *reservedInputQueue) Collector() pipeline.PipelineCollector { return q }

func (q *reservedInputQueue) reserve() { q.slots <- struct{}{} }

func (q *reservedInputQueue) release() { <-q.slots }

// A queue slot is one group, regardless of its size. Reserve wire bytes
// before decoding as well, so a burst of large groups cannot inflate the Go
// heap. An oversized group gets the entire budget and can still make progress.
func (q *reservedInputQueue) reserveWire(size int) int64 {
	q.reserve()
	weight := int64(min(size, fileInputWireBudget))
	_ = q.wireBudget.Acquire(context.Background(), weight)
	return weight
}

func (q *reservedInputQueue) cancelWireReservation(weight int64) {
	q.wireBudget.Release(weight)
	q.release()
}

func (q *reservedInputQueue) collectReserved(group *models.PipelineGroupEvents, weight int64) {
	q.weights.Store(group, weight)
	q.PipelineCollector.CollectList(group)
}

func (q *reservedInputQueue) releaseGroup(group *models.PipelineGroupEvents) {
	if weight, ok := q.weights.LoadAndDelete(group); ok {
		q.wireBudget.Release(weight.(int64))
	}
	q.release()
}

func (q *reservedInputQueue) Collect(group *models.GroupInfo, events ...models.PipelineEvent) {
	if len(events) != 0 {
		q.CollectList(&models.PipelineGroupEvents{Group: group, Events: events})
	}
}

func (q *reservedInputQueue) CollectList(groups ...*models.PipelineGroupEvents) {
	for _, group := range groups {
		q.reserve()
		q.PipelineCollector.CollectList(group)
	}
}

func (q *reservedInputQueue) ToArray() []*models.PipelineGroupEvents {
	groups := q.PipelineCollector.ToArray()
	for _, group := range groups {
		q.releaseGroup(group)
	}
	return groups
}
