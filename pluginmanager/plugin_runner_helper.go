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
	"fmt"
	"math/rand"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
)

type timerRunner struct {
	initialMaxDelay time.Duration
	interval        time.Duration
	context         pipeline.Context
	state           interface{}
}

func (p *timerRunner) Run(task func(state interface{}) error, cc *pipeline.AsyncControl) {
	logger.Info(p.context.GetRuntimeContext(), "task run", "start", "interval", p.interval, "max delay", p.initialMaxDelay, "state", fmt.Sprintf("%T", p.state))
	defer panicRecover(fmt.Sprint(p.state))

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
		p.execTask(task) // execute task at least once.
		if exitFlag {
			logger.Info(p.context.GetRuntimeContext(), "task run", "exit", "state", fmt.Sprintf("%T", p.state))
			return
		}
		exitFlag = util.RandomSleep(p.interval, 0, cc.CancelToken())
	}
}

func (p *timerRunner) execTask(task func(state interface{}) error) {
	if err := task(p.state); err != nil {
		logger.Error(p.context.GetRuntimeContext(), "PLUGIN_RUN_ALARM", "task run", "error", err, "plugin", "state", fmt.Sprintf("%T", p.state))
	}
}

func flushOutStore[T FlushData, F FlusherWrapperInterface](lc *LogstoreConfig, store *FlushOutStore[T], flushers []F, flushFunc func(*LogstoreConfig, F, *FlushOutStore[T]) error) bool {
	for _, flusher := range flushers {
		for waitCount := 0; !flusher.IsReady(lc.ProjectName, lc.LogstoreName, lc.LogstoreKey); waitCount++ {
			if waitCount > maxFlushOutTime*100 {
				logger.Error(lc.Context.GetRuntimeContext(), "DROP_DATA_ALARM", "flush out data timeout, drop data", store.Len())
				return false
			}
			time.Sleep(time.Duration(10) * time.Millisecond)
		}
		err := flushFunc(lc, flusher, store)
		if err != nil {
			logger.Error(lc.Context.GetRuntimeContext(), "FLUSH_DATA_ALARM", "flush data error", lc.ProjectName, lc.LogstoreName, err)
		}
	}
	store.Reset()
	return true
}

func GetFlushStoreLen(runner PluginRunner) int {
	if r, ok := runner.(*pluginv1Runner); ok {
		return r.FlushOutStore.Len()
	}
	if r, ok := runner.(*pluginv2Runner); ok {
		return r.FlushOutStore.Len()
	}
	return 0
}

func GetFlushCancelToken(runner PluginRunner) <-chan struct{} {
	if r, ok := runner.(*pluginv1Runner); ok {
		return r.FlushControl.CancelToken()
	}
	if r, ok := runner.(*pluginv2Runner); ok {
		return r.FlushControl.CancelToken()
	}
	return make(<-chan struct{})
}

func GetConfigInputs(runner PluginRunner) []pipeline.ServiceInput {
	inputs := make([]pipeline.ServiceInput, 0)
	if r, ok := runner.(*pluginv1Runner); ok {
		for _, i := range r.ServicePlugins {
			inputs = append(inputs, i.Input)
		}
	} else if r, ok := runner.(*pluginv2Runner); ok {
		for _, i := range r.ServicePlugins {
			inputs = append(inputs, i.Input)
		}
	}
	return inputs
}

func GetConfigFlushers(runner PluginRunner) []pipeline.Flusher {
	flushers := make([]pipeline.Flusher, 0)
	if r, ok := runner.(*pluginv1Runner); ok {
		for _, f := range r.FlusherPlugins {
			flushers = append(flushers, f.Flusher)
		}
	} else if r, ok := runner.(*pluginv2Runner); ok {
		for _, f := range r.FlusherPlugins {
			flushers = append(flushers, f.Flusher)
		}
	}
	return flushers
}

func pluginUnImplementError(category pluginCategory, version ConfigVersion, pluginType string) error {
	return fmt.Errorf("plugin does not implement %s%s. pluginType: %s", category, strings.ToUpper(string(version)), pluginType)
}

func pluginCategoryUndefinedError(category pluginCategory) error {
	return fmt.Errorf("undefined plugin category : %s", category)
}
