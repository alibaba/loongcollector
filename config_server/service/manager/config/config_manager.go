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

package configmanager

import (
	"sync"

	"config-server/common"
	"config-server/model"
	"config-server/setting"
	"config-server/store"
)

type ConfigManager struct {
	ConfigList      map[string]*model.ConfigDetail
	ConfigListMutex *sync.RWMutex
}

func (c *ConfigManager) Init() {
	c.ConfigList = make(map[string]*model.ConfigDetail)
	c.ConfigListMutex = new(sync.RWMutex)
	go c.updateConfigList(setting.GetSetting().ConfigSyncInterval)

	s := store.GetStore()
	ok, hasErr := s.Has(common.TypeAgentGROUP, "default")
	if hasErr != nil {
		panic(hasErr)
	}
	if ok {
		return
	}
	agentGroup := new(model.AgentGroup)
	agentGroup.Name = "default"
	agentGroup.Tags = make([]model.AgentGroupTag, 0)
	agentGroup.AppliedConfigs = map[string]int64{}
	agentGroup.Description = "Default agent group, include all Agents."

	addErr := s.Add(common.TypeAgentGROUP, agentGroup.Name, agentGroup)
	if addErr != nil {
		panic(addErr)
	}
}
