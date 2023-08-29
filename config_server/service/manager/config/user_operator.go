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
	"time"

	"config-server/common"
	"config-server/model"
	proto "config-server/proto"
	"config-server/store"
)

func (c *ConfigManager) CreateConfig(req *proto.CreateConfigRequest, res *proto.CreateConfigResponse) (int, *proto.CreateConfigResponse) {
	s := store.GetStore()

	ok, hasErr := s.Has(common.TypeConfigDetail, req.ConfigDetail.Name)
	if hasErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = hasErr.Error()
		return common.InternalServerError.Status, res
	}
	if ok {
		value, getErr := s.Get(common.TypeConfigDetail, req.ConfigDetail.Name)
		if getErr != nil {
			res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
			res.Message = getErr.Error()
			return common.InternalServerError.Status, res
		}
		config := value.(*model.ConfigDetail)

		if !config.DelTag { // exsit
			res.Code = proto.RespCode_INVALID_PARAMETER
			res.Message = fmt.Sprintf("Config %s already exists.", req.ConfigDetail.Name)
			return common.ConfigAlreadyExist.Status, res
		}
		// exist but has delete tag
		verison := config.Version
		config.ParseProto(req.ConfigDetail)
		config.Version = verison + 1
		config.DelTag = false

		updateErr := s.Update(common.TypeConfigDetail, config.Name, config)
		if updateErr != nil {
			res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
			res.Message = updateErr.Error()
			return common.InternalServerError.Status, res
		}

		res.Code = proto.RespCode_ACCEPT
		res.Message = "Add config success"
		return common.Accept.Status, res
	}
	// doesn't exist
	config := new(model.ConfigDetail)
	config.ParseProto(req.ConfigDetail)
	config.Version = 0
	config.DelTag = false

	addErr := s.Add(common.TypeConfigDetail, config.Name, config)
	if addErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = addErr.Error()
		return common.InternalServerError.Status, res
	}

	res.Code = proto.RespCode_ACCEPT
	res.Message = "Add config success"
	return common.Accept.Status, res
}

func (c *ConfigManager) UpdateConfig(req *proto.UpdateConfigRequest, res *proto.UpdateConfigResponse) (int, *proto.UpdateConfigResponse) {
	s := store.GetStore()
	ok, hasErr := s.Has(common.TypeConfigDetail, req.ConfigDetail.Name)

	if hasErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = hasErr.Error()
		return common.InternalServerError.Status, res
	}
	if !ok {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Config %s doesn't exist.", req.ConfigDetail.Name)
		return common.ConfigNotExist.Status, res
	}

	value, getErr := s.Get(common.TypeConfigDetail, req.ConfigDetail.Name)
	if getErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = getErr.Error()
		return common.InternalServerError.Status, res
	}
	config := value.(*model.ConfigDetail)

	if config.DelTag {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Config %s doesn't exist.", req.ConfigDetail.Name)
		return common.ConfigNotExist.Status, res
	}
	version := config.Version
	config.ParseProto(req.ConfigDetail)
	config.Version = version + 1

	updateErr := s.Update(common.TypeConfigDetail, config.Name, config)
	if updateErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = updateErr.Error()
		return common.InternalServerError.Status, res
	}

	res.Code = proto.RespCode_ACCEPT
	res.Message = "Update config success"
	return common.Accept.Status, res
}

func (c *ConfigManager) DeleteConfig(req *proto.DeleteConfigRequest, res *proto.DeleteConfigResponse) (int, *proto.DeleteConfigResponse) {
	s := store.GetStore()
	ok, hasErr := s.Has(common.TypeConfigDetail, req.ConfigName)

	if hasErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = hasErr.Error()
		return common.InternalServerError.Status, res
	}
	if !ok {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Config %s doesn't exist.", req.ConfigName)
		return common.ConfigNotExist.Status, res
	}
	value, getErr := s.Get(common.TypeConfigDetail, req.ConfigName)
	if getErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = getErr.Error()
		return common.InternalServerError.Status, res
	}
	config := value.(*model.ConfigDetail)

	if config.DelTag {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Config %s doesn't exist.", req.ConfigName)
		return common.ConfigNotExist.Status, res
	}
	// Check if this config bind with agent groups
	checkReq := proto.GetAppliedAgentGroupsRequest{}
	checkRes := &proto.GetAppliedAgentGroupsResponse{}
	checkReq.ConfigName = req.ConfigName
	status, checkRes := c.GetAppliedAgentGroups(&checkReq, checkRes)
	if status != 200 {
		res.Code = checkRes.Code
		res.Message = checkRes.Message
		return status, res
	}
	if len(checkRes.AgentGroupNames) > 0 {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = fmt.Sprintf("Config %s was applied to some agent groups, cannot be deleted.", req.ConfigName)
		return common.InternalServerError.Status, res
	}

	config.DelTag = true
	config.Version++

	updateErr := s.Update(common.TypeConfigDetail, req.ConfigName, config)
	if updateErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = updateErr.Error()
		return common.InternalServerError.Status, res
	}

	res.Code = proto.RespCode_ACCEPT
	res.Message = "Delete config success"
	return common.Accept.Status, res
}

func (c *ConfigManager) GetConfig(req *proto.GetConfigRequest, res *proto.GetConfigResponse) (int, *proto.GetConfigResponse) {
	s := store.GetStore()
	ok, hasErr := s.Has(common.TypeConfigDetail, req.ConfigName)

	if hasErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = hasErr.Error()
		return common.InternalServerError.Status, res
	}
	if !ok {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Config %s doesn't exist.", req.ConfigName)
		return common.ConfigNotExist.Status, res
	}
	value, getErr := s.Get(common.TypeConfigDetail, req.ConfigName)
	if getErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = getErr.Error()
		return common.InternalServerError.Status, res
	}
	config := value.(*model.ConfigDetail)

	if config.DelTag {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Config %s doesn't exist.", req.ConfigName)
		return common.ConfigNotExist.Status, res
	}

	res.Code = proto.RespCode_ACCEPT
	res.Message = "Get config success"
	res.ConfigDetail = config.ToProto()
	return common.Accept.Status, res

}

func (c *ConfigManager) ListConfigs(req *proto.ListConfigsRequest, res *proto.ListConfigsResponse) (int, *proto.ListConfigsResponse) {
	s := store.GetStore()
	configs, getAllErr := s.GetAll(common.TypeConfigDetail)

	if getAllErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = getAllErr.Error()
		return common.InternalServerError.Status, res
	}
	ans := make([]model.ConfigDetail, 0)
	for _, value := range configs {
		config := value.(*model.ConfigDetail)
		if !config.DelTag {
			ans = append(ans, *config)
		}
	}

	res.Code = proto.RespCode_ACCEPT
	res.Message = "Get config list success"
	res.ConfigDetails = make([]*proto.ConfigDetail, 0)
	for _, v := range ans {
		res.ConfigDetails = append(res.ConfigDetails, v.ToProto())
	}
	return common.Accept.Status, res
}

func (c *ConfigManager) GetAppliedAgentGroups(req *proto.GetAppliedAgentGroupsRequest, res *proto.GetAppliedAgentGroupsResponse) (int, *proto.GetAppliedAgentGroupsResponse) {
	ans := make([]string, 0)
	s := store.GetStore()

	// Check config exist
	ok, hasErr := s.Has(common.TypeConfigDetail, req.ConfigName)
	if hasErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = hasErr.Error()
		return common.InternalServerError.Status, res
	}
	if !ok {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Config %s doesn't exist.", req.ConfigName)
		return common.ConfigNotExist.Status, res
	}
	value, getErr := s.Get(common.TypeConfigDetail, req.ConfigName)
	if getErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = getErr.Error()
		return common.InternalServerError.Status, res
	}
	config := value.(*model.ConfigDetail)

	if config.DelTag {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Config %s doesn't exist.", req.ConfigName)
		return common.ConfigNotExist.Status, res
	}

	// Get agent group names
	agentGroupList, getAllErr := s.GetAll(common.TypeAgentGROUP)
	if getAllErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = getAllErr.Error()
		return common.InternalServerError.Status, res
	}
	for _, g := range agentGroupList {
		if _, ok := g.(*model.AgentGroup).AppliedConfigs[req.ConfigName]; ok {
			ans = append(ans, g.(*model.AgentGroup).Name)
		}
	}

	res.Code = proto.RespCode_ACCEPT
	res.Message = "Get group list success"
	res.AgentGroupNames = ans
	return common.Accept.Status, res
}

func (c *ConfigManager) CreateAgentGroup(req *proto.CreateAgentGroupRequest, res *proto.CreateAgentGroupResponse) (int, *proto.CreateAgentGroupResponse) {
	s := store.GetStore()
	ok, hasErr := s.Has(common.TypeAgentGROUP, req.AgentGroup.GroupName)
	if hasErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = hasErr.Error()
		return common.InternalServerError.Status, res
	}
	if ok {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Agent group %s already exists.", req.AgentGroup.GroupName)
		return common.AgentGroupAlreadyExist.Status, res
	}
	agentGroup := new(model.AgentGroup)
	agentGroup.ParseProto(req.AgentGroup)
	agentGroup.AppliedConfigs = make(map[string]int64, 0)

	addErr := s.Add(common.TypeAgentGROUP, agentGroup.Name, agentGroup)
	if addErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = addErr.Error()
		return common.InternalServerError.Status, res
	}

	res.Code = proto.RespCode_ACCEPT
	res.Message = "Add agent group success"
	return common.Accept.Status, res
}

func (c *ConfigManager) UpdateAgentGroup(req *proto.UpdateAgentGroupRequest, res *proto.UpdateAgentGroupResponse) (int, *proto.UpdateAgentGroupResponse) {
	s := store.GetStore()
	ok, hasErr := s.Has(common.TypeAgentGROUP, req.AgentGroup.GroupName)
	if hasErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = hasErr.Error()
		return common.InternalServerError.Status, res
	}
	if !ok {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Agent group %s doesn't exist.", req.AgentGroup.GroupName)
		return common.AgentGroupNotExist.Status, res
	}
	value, getErr := s.Get(common.TypeAgentGROUP, req.AgentGroup.GroupName)
	if getErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = getErr.Error()
		return common.InternalServerError.Status, res
	}
	agentGroup := value.(*model.AgentGroup)

	agentGroup.ParseProto(req.AgentGroup)

	updateErr := s.Update(common.TypeAgentGROUP, req.AgentGroup.GroupName, agentGroup)
	if updateErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = updateErr.Error()
		return common.InternalServerError.Status, res
	}

	res.Code = proto.RespCode_ACCEPT
	res.Message = "Update agent group success"
	return common.Accept.Status, res
}

func (c *ConfigManager) DeleteAgentGroup(req *proto.DeleteAgentGroupRequest, res *proto.DeleteAgentGroupResponse) (int, *proto.DeleteAgentGroupResponse) {
	s := store.GetStore()

	if req.GroupName == "default" {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = "Cannot delete agent group 'default'"
		return common.BadRequest.Status, res
	}

	ok, hasErr := s.Has(common.TypeAgentGROUP, req.GroupName)
	if hasErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = hasErr.Error()
		return common.InternalServerError.Status, res
	}
	if !ok {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Agent group %s doesn't exist.", req.GroupName)
		return common.AgentGroupNotExist.Status, res
	}
	// Check if this config bind with agent groups
	checkReq := proto.GetAppliedConfigsForAgentGroupRequest{}
	checkRes := &proto.GetAppliedConfigsForAgentGroupResponse{}
	checkReq.GroupName = req.GroupName
	status, checkRes := c.GetAppliedConfigsForAgentGroup(&checkReq, checkRes)
	if status != 200 {
		res.Code = checkRes.Code
		res.Message = checkRes.Message
		return status, res
	}
	if len(checkRes.ConfigNames) > 0 {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = fmt.Sprintf("Agent group %s was applied to some configs, cannot be deleted.", req.GroupName)
		return common.InternalServerError.Status, res
	}

	deleteErr := s.Delete(common.TypeAgentGROUP, req.GroupName)
	if deleteErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = deleteErr.Error()
		return common.InternalServerError.Status, res
	}

	res.Code = proto.RespCode_ACCEPT
	res.Message = "Delete agent group success"
	return common.Accept.Status, res
}

func (c *ConfigManager) GetAgentGroup(req *proto.GetAgentGroupRequest, res *proto.GetAgentGroupResponse) (int, *proto.GetAgentGroupResponse) {
	s := store.GetStore()
	ok, hasErr := s.Has(common.TypeAgentGROUP, req.GroupName)
	if hasErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = hasErr.Error()
		return common.InternalServerError.Status, res
	}
	if !ok {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Agent group %s doesn't exist.", req.GroupName)
		return common.AgentGroupNotExist.Status, res
	}
	agentGroup, getErr := s.Get(common.TypeAgentGROUP, req.GroupName)
	if getErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = getErr.Error()
		return common.InternalServerError.Status, res
	}

	res.Code = proto.RespCode_ACCEPT
	res.Message = "Get agent group success"
	res.AgentGroup = agentGroup.(*model.AgentGroup).ToProto()
	return common.Accept.Status, res
}

func (c *ConfigManager) ListAgentGroups(req *proto.ListAgentGroupsRequest, res *proto.ListAgentGroupsResponse) (int, *proto.ListAgentGroupsResponse) {
	s := store.GetStore()
	agentGroupList, getAllErr := s.GetAll(common.TypeAgentGROUP)
	if getAllErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		return common.InternalServerError.Status, res
	}
	res.Code = proto.RespCode_ACCEPT
	res.Message = "Get agent group list success"
	res.AgentGroups = []*proto.AgentGroup{}
	for _, v := range agentGroupList {
		res.AgentGroups = append(res.AgentGroups, v.(*model.AgentGroup).ToProto())
	}
	return common.Accept.Status, res
}

func (c *ConfigManager) ListAgents(req *proto.ListAgentsRequest, res *proto.ListAgentsResponse) (int, *proto.ListAgentsResponse) {
	ans := make([]*proto.Agent, 0)
	s := store.GetStore()

	var agentGroup *model.AgentGroup

	// get agent group
	ok, hasErr := s.Has(common.TypeAgentGROUP, req.GroupName)
	if hasErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = hasErr.Error()
		return common.InternalServerError.Status, res
	}
	if !ok {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Agent group %s doesn't exist.", req.GroupName)
		return common.AgentGroupNotExist.Status, res
	}
	value, getErr := s.Get(common.TypeAgentGROUP, req.GroupName)
	if getErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = getErr.Error()
		return common.InternalServerError.Status, res
	}

	agentGroup = value.(*model.AgentGroup)

	agentList, getAllErr := s.GetAll(common.TypeAgent)
	if getAllErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = getAllErr.Error()
		return common.InternalServerError.Status, res
	}

	for _, v := range agentList {
		agent := v.(*model.Agent)
		match := func() bool {
			for _, v := range agentGroup.Tags {
				for _, tag := range agent.Tags {
					if v.Value == tag {
						return true
					}
				}
			}
			return false
		}()
		if match || agentGroup.Name == "default" {
			ans = append(ans, agent.ToProto())
		}
	}

	res.Code = proto.RespCode_ACCEPT
	res.Message = "Get agent list success"
	res.Agents = ans
	return common.Accept.Status, res
}

func (c *ConfigManager) GetAppliedConfigsForAgentGroup(req *proto.GetAppliedConfigsForAgentGroupRequest, res *proto.GetAppliedConfigsForAgentGroupResponse) (int, *proto.GetAppliedConfigsForAgentGroupResponse) {
	ans := make([]string, 0)
	s := store.GetStore()
	var agentGroup *model.AgentGroup

	// Get agent group
	ok, hasErr := s.Has(common.TypeAgentGROUP, req.GroupName)
	if hasErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = hasErr.Error()
		return common.InternalServerError.Status, res
	}
	if !ok {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Agent group %s doesn't exist.", req.GroupName)
		return common.AgentGroupNotExist.Status, res
	}
	value, getErr := s.Get(common.TypeAgentGROUP, req.GroupName)
	if getErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = getErr.Error()
		return common.InternalServerError.Status, res
	}

	agentGroup = value.(*model.AgentGroup)

	for k := range agentGroup.AppliedConfigs {
		ans = append(ans, k)
	}

	res.Code = proto.RespCode_ACCEPT
	res.Message = "Get agent group's applied configs success"
	res.ConfigNames = ans
	return common.Accept.Status, res
}

func (c *ConfigManager) ApplyConfigToAgentGroup(req *proto.ApplyConfigToAgentGroupRequest, res *proto.ApplyConfigToAgentGroupResponse) (int, *proto.ApplyConfigToAgentGroupResponse) {
	s := store.GetStore()
	var agentGroup *model.AgentGroup
	var config *model.ConfigDetail

	// Get agent group
	ok, hasErr := s.Has(common.TypeAgentGROUP, req.GroupName)
	if hasErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = hasErr.Error()
		return common.InternalServerError.Status, res
	}
	if !ok {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Agent group %s doesn't exist.", req.GroupName)
		return common.AgentGroupNotExist.Status, res
	}
	value, getErr := s.Get(common.TypeAgentGROUP, req.GroupName)
	if getErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = getErr.Error()
		return common.InternalServerError.Status, res
	}

	agentGroup = value.(*model.AgentGroup)

	// Get config
	ok, hasErr = s.Has(common.TypeConfigDetail, req.ConfigName)
	if hasErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = hasErr.Error()
		return common.InternalServerError.Status, res
	}
	if !ok {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Config %s doesn't exist.", req.ConfigName)
		return common.ConfigNotExist.Status, res
	}
	value, getErr = s.Get(common.TypeConfigDetail, req.ConfigName)
	if getErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = getErr.Error()
		return common.InternalServerError.Status, res
	}
	config = value.(*model.ConfigDetail)

	if config.DelTag {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Config %s doesn't exist.", req.ConfigName)
		return common.ConfigNotExist.Status, res
	}

	// Check if agent group already has config
	if agentGroup.AppliedConfigs == nil {
		agentGroup.AppliedConfigs = make(map[string]int64)
	}
	if _, ok := agentGroup.AppliedConfigs[config.Name]; ok {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Agent group %s already has config %s.", req.GroupName, req.ConfigName)
		return common.ConfigAlreadyExist.Status, res
	}
	agentGroup.AppliedConfigs[config.Name] = time.Now().Unix()

	updateErr := store.GetStore().Update(common.TypeAgentGROUP, req.GroupName, agentGroup)
	if updateErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = updateErr.Error()
		return common.InternalServerError.Status, res
	}

	res.Code = proto.RespCode_ACCEPT
	res.Message = "Add config to agent group success"
	return common.Accept.Status, res
}

func (c *ConfigManager) RemoveConfigFromAgentGroup(req *proto.RemoveConfigFromAgentGroupRequest, res *proto.RemoveConfigFromAgentGroupResponse) (int, *proto.RemoveConfigFromAgentGroupResponse) {
	s := store.GetStore()
	var agentGroup *model.AgentGroup
	var config *model.ConfigDetail

	// Get agent group
	ok, hasErr := s.Has(common.TypeAgentGROUP, req.GroupName)
	if hasErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = hasErr.Error()
		return common.InternalServerError.Status, res
	}
	if !ok {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Agent group %s doesn't exist.", req.GroupName)
		return common.AgentGroupNotExist.Status, res
	}
	value, getErr := s.Get(common.TypeAgentGROUP, req.GroupName)
	if getErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = getErr.Error()
		return common.InternalServerError.Status, res
	}

	agentGroup = value.(*model.AgentGroup)

	// Get config
	ok, hasErr = s.Has(common.TypeConfigDetail, req.ConfigName)
	if hasErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = hasErr.Error()
		return common.InternalServerError.Status, res
	}
	if !ok {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Config %s doesn't exist.", req.ConfigName)
		return common.ConfigNotExist.Status, res
	}
	value, getErr = s.Get(common.TypeConfigDetail, req.ConfigName)
	if getErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = getErr.Error()
		return common.InternalServerError.Status, res
	}
	config = value.(*model.ConfigDetail)

	if config.DelTag {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Config %s doesn't exist.", req.ConfigName)
		return common.ConfigNotExist.Status, res
	}

	if agentGroup.AppliedConfigs == nil {
		agentGroup.AppliedConfigs = make(map[string]int64)
	}
	if _, ok := agentGroup.AppliedConfigs[config.Name]; !ok {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Agent group %s doesn't have config %s.", req.GroupName, req.ConfigName)
		return common.ConfigNotExist.Status, res
	}
	delete(agentGroup.AppliedConfigs, config.Name)

	updateErr := store.GetStore().Update(common.TypeAgentGROUP, req.GroupName, agentGroup)
	if updateErr != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		res.Message = updateErr.Error()
		return common.InternalServerError.Status, res
	}

	res.Code = proto.RespCode_ACCEPT
	res.Message = "Remove config from agent group success"
	return common.Accept.Status, res
}
