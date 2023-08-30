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

	"config-server/common"
	"config-server/model"
	proto "config-server/proto"
	"config-server/store"
)

func (c *ConfigManager) updateConfigList(interval int) {
	ticker := time.NewTicker(time.Duration(interval) * time.Second)
	defer ticker.Stop()

	for range ticker.C {
		s := store.GetStore()
		configList, getAllErr := s.GetAll(common.TypeConfigDetail)
		if getAllErr != nil {
			log.Println(getAllErr)
			continue
		} else {
			c.ConfigListMutex.Lock()
			c.ConfigList = make(map[string]*model.ConfigDetail, 0)
			for _, config := range configList {
				c.ConfigList[config.(*model.ConfigDetail).Name] = config.(*model.ConfigDetail)
			}
			c.ConfigListMutex.Unlock()
		}
	}
}

func (c *ConfigManager) CheckConfigUpdatesWhenHeartbeat(req *proto.HeartBeatRequest, res *proto.HeartBeatResponse) (int, *proto.HeartBeatResponse) {
	pipelineConfigs := make([]*proto.ConfigCheckResult, 0)
	agentConfigs := make([]*proto.ConfigCheckResult, 0)
	s := store.GetStore()

	// get all configs connected to agent group whose tag is same as agent's
	SelectedConfigs := make(map[string]*model.ConfigDetail)
	agentGroupList, getAllErr := s.GetAll(common.TypeAgentGROUP)
	if getAllErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = getAllErr.Error()
		return common.InternalServerError.Status, res
	}

	for _, agentGroup := range agentGroupList {
		match := func() bool {
			for _, v := range agentGroup.(*model.AgentGroup).Tags {
				for _, tag := range req.Tags {
					if v.Value == tag {
						return true
					}
				}
			}
			return false
		}()
		if match || agentGroup.(*model.AgentGroup).Name == "default" {
			for k := range agentGroup.(*model.AgentGroup).AppliedConfigs {
				// Check if config k exist
				c.ConfigListMutex.RLock()
				config, ok := c.ConfigList[k]
				c.ConfigListMutex.RUnlock()
				if !ok {
					res.Code = proto.RespCode_INVALID_PARAMETER
					res.Message = fmt.Sprintf("Config %s doesn't exist.", k)
					return common.ConfigNotExist.Status, res
				}

				if config.DelTag {
					res.Code = proto.RespCode_INVALID_PARAMETER
					res.Message = fmt.Sprintf("Config %s doesn't exist.", k)
					return common.ConfigNotExist.Status, res
				}

				SelectedConfigs[k] = config
			}
		}
	}

	// deleted or modified configs, and delete same configs in SelectedConfigs
	for _, k := range req.PipelineConfigs {
		config, ok := SelectedConfigs[k.Name]
		if !ok {
			result := new(proto.ConfigCheckResult)
			result.Type = proto.ConfigType_PIPELINE_CONFIG
			result.Name = k.Name
			result.OldVersion = k.Version
			result.NewVersion = k.Version
			result.Context = k.Context
			result.CheckStatus = proto.CheckStatus_DELETED
			pipelineConfigs = append(pipelineConfigs, result)
		} else if ok {
			if config.Version > k.Version {
				result := new(proto.ConfigCheckResult)
				result.Type = proto.ConfigType_PIPELINE_CONFIG
				result.Name = config.Name
				result.OldVersion = k.Version
				result.NewVersion = config.Version
				result.Context = config.Context
				result.CheckStatus = proto.CheckStatus_MODIFIED
				pipelineConfigs = append(pipelineConfigs, result)
			}
			delete(SelectedConfigs, k.Name)
		}
	}

	for _, k := range req.AgentConfigs {
		config, ok := SelectedConfigs[k.Name]
		if !ok {
			result := new(proto.ConfigCheckResult)
			result.Type = proto.ConfigType_AGENT_CONFIG
			result.Name = k.Name
			result.OldVersion = k.Version
			result.NewVersion = k.Version
			result.Context = k.Context
			result.CheckStatus = proto.CheckStatus_DELETED
			agentConfigs = append(agentConfigs, result)
		} else if ok {
			if config.Version > k.Version {
				result := new(proto.ConfigCheckResult)
				result.Type = proto.ConfigType_AGENT_CONFIG
				result.Name = config.Name
				result.OldVersion = k.Version
				result.NewVersion = config.Version
				result.Context = config.Context
				result.CheckStatus = proto.CheckStatus_MODIFIED
				agentConfigs = append(agentConfigs, result)
			}
			delete(SelectedConfigs, k.Name)
		}
	}

	// new configs
	for _, config := range SelectedConfigs {
		result := new(proto.ConfigCheckResult)
		result.Type = model.ConfigType[config.Type]
		result.Name = config.Name
		result.OldVersion = 0
		result.NewVersion = config.Version
		result.Context = config.Context
		result.CheckStatus = proto.CheckStatus_NEW
		switch result.Type {
		case proto.ConfigType_AGENT_CONFIG:
			agentConfigs = append(agentConfigs, result)
		case proto.ConfigType_PIPELINE_CONFIG:
			pipelineConfigs = append(pipelineConfigs, result)
		}
	}

	res.Code = proto.RespCode_ACCEPT
	res.Message += "Get config update infos success"
	res.AgentCheckResults = agentConfigs
	res.PipelineCheckResults = pipelineConfigs
	return common.Accept.Status, res
}

func (c *ConfigManager) FetchAgentConfig(req *proto.FetchAgentConfigRequest, res *proto.FetchAgentConfigResponse) (int, *proto.FetchAgentConfigResponse) {
	ans := make([]*proto.ConfigDetail, 0)

	// do something about attributes

	for _, configInfo := range req.ReqConfigs {
		c.ConfigListMutex.RLock()
		config, ok := c.ConfigList[configInfo.Name]
		c.ConfigListMutex.RUnlock()
		if !ok {
			res.Code = proto.RespCode_INVALID_PARAMETER
			res.Message += fmt.Sprintf("Config %s doesn't exist.\n", configInfo.Name)
			continue
		}

		if config.DelTag {
			res.Code = proto.RespCode_INVALID_PARAMETER
			res.Message = fmt.Sprintf("Config %s doesn't exist.\n", configInfo.Name)
			continue
		}
		if config.Type == configInfo.Type.String() {
			ans = append(ans, config.ToProto())
		}
	}

	res.Code = proto.RespCode_ACCEPT
	res.Message = "Get Agent Config details success"
	res.ConfigDetails = ans
	return common.Accept.Status, res
}

func (c *ConfigManager) FetchPipelineConfig(req *proto.FetchPipelineConfigRequest, res *proto.FetchPipelineConfigResponse) (int, *proto.FetchPipelineConfigResponse) {
	ans := make([]*proto.ConfigDetail, 0)

	for _, configInfo := range req.ReqConfigs {
		c.ConfigListMutex.RLock()
		config, ok := c.ConfigList[configInfo.Name]
		c.ConfigListMutex.RUnlock()
		if !ok {
			res.Code = proto.RespCode_INVALID_PARAMETER
			res.Message += fmt.Sprintf("Config %s doesn't exist.\n", configInfo.Name)
			continue
		}

		if config.DelTag {
			res.Code = proto.RespCode_INVALID_PARAMETER
			res.Message = fmt.Sprintf("Config %s doesn't exist.\n", configInfo.Name)
			continue
		}
		if config.Type == configInfo.Type.String() {
			ans = append(ans, config.ToProto())
		}
	}

	res.Code = proto.RespCode_ACCEPT
	res.Message = "Get Agent Config details success"
	res.ConfigDetails = ans
	return common.Accept.Status, res
}
