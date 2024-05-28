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
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
)

const alwaysOnlineTimeoutCheckInterval = time.Second * time.Duration(3)

var instanceAlwaysOnlineManager *AlwaysOnlineManager
var onceAlwaysOnlineManager sync.Once

type alwaysOnlineItem struct {
	config    *LogstoreConfig
	addedTime time.Time
	timeout   time.Duration
}

// AlwaysOnlineManager is used to manage the plugins that do not want to stop when config reloading
type AlwaysOnlineManager struct {
	configMap map[string]*alwaysOnlineItem
	lock      sync.Mutex
}

// GetAlwaysOnlineManager get a AlwaysOnlineManager instance
func GetAlwaysOnlineManager() *AlwaysOnlineManager {
	onceAlwaysOnlineManager.Do(
		func() {
			instanceAlwaysOnlineManager = &AlwaysOnlineManager{
				configMap: make(map[string]*alwaysOnlineItem),
			}
			go instanceAlwaysOnlineManager.run()
		},
	)
	return instanceAlwaysOnlineManager
}

// AddCachedConfig add cached config into manager, manager will stop and delete this config when timeout
func (aom *AlwaysOnlineManager) AddCachedConfig(config *LogstoreConfig, timeout time.Duration) {
	aom.lock.Lock()
	defer aom.lock.Unlock()
	alwaysOnlineItem := &alwaysOnlineItem{
		config:    config,
		addedTime: time.Now(),
		timeout:   timeout,
	}
	aom.configMap[config.ConfigNameWithSuffix] = alwaysOnlineItem
}

// GetCachedConfig get cached config from manager and delete this item, so manager will not close this config
func (aom *AlwaysOnlineManager) GetCachedConfig(configName string) (config *LogstoreConfig, ok bool) {
	aom.lock.Lock()
	defer aom.lock.Unlock()
	if item, ok := aom.configMap[configName]; ok {
		delete(aom.configMap, configName)
		return item.config, true
	}
	return nil, false
}

// GetDeletedConfigs returns cached configs not in @existConfigs.
func (aom *AlwaysOnlineManager) GetDeletedConfigs(
	existConfigs map[string]*LogstoreConfig) map[string]*LogstoreConfig {
	aom.lock.Lock()
	defer aom.lock.Unlock()
	ret := make(map[string]*LogstoreConfig)
	for name, cfg := range aom.configMap {
		if _, exists := existConfigs[name]; !exists {
			ret[name] = cfg.config
			delete(aom.configMap, name)
		}
	}
	return ret
}

func (aom *AlwaysOnlineManager) run() {
	for {
		time.Sleep(alwaysOnlineTimeoutCheckInterval)

		var toDeleteItems []*alwaysOnlineItem
		nowTime := time.Now()

		aom.lock.Lock()
		for key, item := range aom.configMap {
			if nowTime.After(item.addedTime.Add(item.timeout)) {
				toDeleteItems = append(toDeleteItems, item)
				delete(aom.configMap, key)
			}
		}
		aom.lock.Unlock()

		if len(toDeleteItems) == 0 {
			continue
		}
		for _, item := range toDeleteItems {
			go func(config *LogstoreConfig, addTime time.Time) {
				defer panicRecover(config.ConfigName)
				logger.Info(config.Context.GetRuntimeContext(), "delete timeout config, add time", addTime, "config", config.ConfigName)
				err := config.Stop(true)
				logger.Info(config.Context.GetRuntimeContext(), "delete timeout config done", config.ConfigName, "error", err)
			}(item.config, item.addedTime)
		}
	}
}
