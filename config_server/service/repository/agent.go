package repository

import (
	"config-server/common"
	"config-server/entity"
	"config-server/store"
)

var s = store.S

func GetAgentByiId(instanceId string) *entity.Agent {
	var agentInfo = new(entity.Agent)
	row := s.Db.Where("instance_id=?", instanceId).Find(agentInfo).RowsAffected
	if row == 1 {
		return agentInfo
	}
	return nil
}

func GetAllAgents(containPipelineConfigs bool, containInstanceConfigs bool) []entity.Agent {
	var agentInfoList []entity.Agent
	tx := s.Db
	if containPipelineConfigs {
		tx.Preload("PipelineConfigs")
	}
	if containInstanceConfigs {
		tx.Preload("InstanceConfigs")
	}
	tx.Find(&agentInfoList)
	return agentInfoList
}

func RemoveAgentById(instanceId string) error {
	tx := s.Db.Where("instance_id=?", instanceId).Delete(&entity.Agent{})
	if tx.RowsAffected != 1 {
		return common.ServerErrorWithMsg("Agent failed to delete record %s", instanceId)
	}
	return nil
}

func UpdateAgentById(agent *entity.Agent, filed ...string) error {
	if filed == nil {
		row := s.Db.Model(agent).Updates(*agent).RowsAffected
		if row != 1 {
			return common.ServerErrorWithMsg("update agent error")
		}
	}
	row := s.Db.Model(agent).Select(filed).Updates(*agent).RowsAffected
	if row != 1 {
		return common.ServerErrorWithMsg("update agent filed error")
	}
	return nil
}

func CreateOrUpdateAgentBasicInfo(conflictColumnNames []string, agent ...*entity.Agent) error {
	return createOrUpdateEntities(conflictColumnNames, nil, agent...)
}

func ListAgentsByGroupName(groupName string) ([]*entity.Agent, error) {
	agentGroup := entity.AgentGroup{}
	err := s.Db.Preload("Agents").Where("name=?", groupName).Find(&agentGroup).Error

	agent := new(entity.Agent)
	s.Db.First(agent)

	if err != nil {
		return nil, err
	}
	return agentGroup.Agents, nil
}
