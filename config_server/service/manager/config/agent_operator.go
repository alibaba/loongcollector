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
	"fmt"
	"log"
	"time"

	"github.com/alibaba/ilogtail/config_server/service/common"
	"github.com/alibaba/ilogtail/config_server/service/model"
	proto "github.com/alibaba/ilogtail/config_server/service/proto"
	"github.com/alibaba/ilogtail/config_server/service/store"
)

func (c *ConfigManager) updateConfigList(interval int) {
	ticker := time.NewTicker(time.Duration(interval) * time.Second)
	defer ticker.Stop()

	for range ticker.C {
		s := store.GetStore()
		configList, getAllErr := s.GetAll(common.TypeCollectionConfig)
		if getAllErr != nil {
			log.Println(getAllErr)
			continue
		} else {
			c.ConfigListMutex.Lock()
			c.ConfigList = make(map[string]*model.Config, 0)
			for _, config := range configList {
				c.ConfigList[config.(*model.Config).Name] = config.(*model.Config)
			}
			c.ConfigListMutex.Unlock()
		}
	}
}

func (c *ConfigManager) GetConfigList(req *proto.AgentGetConfigListRequest, res *proto.AgentGetConfigListResponse) (int, *proto.AgentGetConfigListResponse) {
	ans := make([]*proto.ConfigUpdateInfo, 0)
	s := store.GetStore()

	// get agent's tag
	ok, hasErr := s.Has(common.TypeAgent, req.AgentId)
	if hasErr != nil {
		res.Code = common.InternalServerError.Code
		res.Message = hasErr.Error()
		return common.InternalServerError.Status, res
	} else if !ok {
		res.Code = common.AgentNotExist.Code
		res.Message = fmt.Sprintf("Agent %s doesn't exist.", req.AgentId)
		return common.AgentNotExist.Status, res
	}

	value, getErr := s.Get(common.TypeAgent, req.AgentId)
	if getErr != nil {
		res.Code = common.InternalServerError.Code
		res.Message = getErr.Error()
		return common.InternalServerError.Status, res
	}
	agent := value.(*model.Agent)

	// get all configs connected to agent group whose tag is same as agent's
	configList := make(map[string]*model.Config)
	agentGroupList, getAllErr := s.GetAll(common.TypeAgentGROUP)
	if getAllErr != nil {
		res.Code = common.InternalServerError.Code
		res.Message = getAllErr.Error()
		return common.InternalServerError.Status, res
	}

	for _, agentGroup := range agentGroupList {
		match := func() bool {
			for _, v := range agentGroup.(*model.AgentGroup).Tags {
				_, ok := agent.Tags[v.Name]
				if ok && agent.Tags[v.Name] == v.Value {
					return true
				}
			}
			return false
		}()
		if match || agentGroup.(*model.AgentGroup).Name == "default" {
			for k := range agentGroup.(*model.AgentGroup).AppliedConfigs {
				// Check if config k exist
				ok, hasErr := s.Has(common.TypeCollectionConfig, k)
				if hasErr != nil {
					res.Code = common.InternalServerError.Code
					res.Message = hasErr.Error()
					return common.InternalServerError.Status, res
				}
				if !ok {
					res.Code = common.ConfigNotExist.Code
					res.Message = fmt.Sprintf("Config %s doesn't exist.", k)
					return common.ConfigNotExist.Status, res
				}

				value, getErr := s.Get(common.TypeCollectionConfig, k)
				if getErr != nil {
					res.Code = common.InternalServerError.Code
					res.Message = getErr.Error()
					return common.InternalServerError.Status, res
				}
				config := value.(*model.Config)

				if config.DelTag {
					res.Code = common.ConfigNotExist.Code
					res.Message = fmt.Sprintf("Config %s doesn't exist.", k)
					return common.ConfigNotExist.Status, res
				}

				configList[k] = config
			}
		}
	}

	// comp config info
	for k := range req.ConfigVersions {
		if _, ok := configList[k]; !ok {
			result := new(proto.ConfigUpdateInfo)
			result.ConfigName = k
			result.UpdateStatus = proto.ConfigUpdateInfo_DELETED
			ans = append(ans, result)
		}
	}

	for k, config := range configList {
		result := new(proto.ConfigUpdateInfo)

		if _, ok := req.ConfigVersions[k]; !ok {
			result.ConfigName = k
			result.UpdateStatus = proto.ConfigUpdateInfo_NEW
			result.ConfigVersion = config.Version
			result.Content = config.Content
		} else {
			ver := req.ConfigVersions[k]

			if ver < config.Version {
				result.ConfigName = k
				result.UpdateStatus = proto.ConfigUpdateInfo_MODIFIED
				result.ConfigVersion = config.Version
				result.Content = config.Content
			} else {
				result.ConfigName = k
				result.UpdateStatus = proto.ConfigUpdateInfo_SAME
			}
		}
		ans = append(ans, result)
	}

	res.Code = common.Accept.Code
	res.Message = "Get config update infos success"
	res.ConfigUpdateInfos = ans
	return common.Accept.Status, res
}
