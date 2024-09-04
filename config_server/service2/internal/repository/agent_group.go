package repository

import (
	"config-server2/internal/common"
	"config-server2/internal/entity"
)

func CreateAgentGroup(group *entity.AgentGroup) error {
	row := s.DB.Create(group).RowsAffected
	if row != 1 {
		return common.ServerErrorWithMsg("create agentGroup(%s) error", group.Name)
	}
	return nil
}

func UpdateAgentGroup(group *entity.AgentGroup) error {
	row := s.DB.Model(&group).Updates(group).RowsAffected
	if row != 1 {
		return common.ServerErrorWithMsg("update agentGroup(%s) error", group.Name)
	}
	return nil
}

func DeleteAgentGroup(name string) error {
	row := s.DB.Where("name=?", name).Delete(&entity.AgentGroup{}).RowsAffected
	if row != 1 {
		return common.ServerErrorWithMsg("delete agentGroup(name=%s) error", name)
	}
	return nil
}

func GetAgentGroupDetail(name string, containPipelineConfigs bool, containInstanceConfigs bool) (*entity.AgentGroup, error) {
	agentGroup := &entity.AgentGroup{}
	tx := s.DB.Where("name=?", name)
	if containPipelineConfigs {
		tx.Preload("PipelineConfigs")
	}
	if containInstanceConfigs {
		tx.Preload("InstanceConfigs")
	}
	row := tx.Find(agentGroup).RowsAffected
	if row != 1 {
		return nil, common.ServerErrorWithMsg("get agentGroup(name=%s) error", name)
	}
	return agentGroup, nil
}

func GetAllAgentGroup() ([]*entity.AgentGroup, error) {
	agentGroups := make([]*entity.AgentGroup, 0)
	err := s.DB.Find(&agentGroups).Error
	return agentGroups, err
}

func GetAppliedAgentGroupForPipelineConfigName(configName string) ([]string, error) {
	pipelineConfig := &entity.PipelineConfig{}
	row := s.DB.Preload("AgentGroups").Where("name=?", configName).Find(&pipelineConfig).RowsAffected
	if row != 1 {
		return nil, common.ServerErrorWithMsg("can not find name=%s pipelineConfig")
	}
	groupNames := make([]string, 0)
	for _, group := range pipelineConfig.AgentGroups {
		groupNames = append(groupNames, group.Name)
	}
	return groupNames, nil
}
func GetAppliedAgentGroupForInstanceConfigName(configName string) ([]string, error) {
	instanceConfig := &entity.InstanceConfig{}
	row := s.DB.Preload("AgentGroups").Where("name=?", configName).Find(&instanceConfig).RowsAffected
	if row != 1 {
		return nil, common.ServerErrorWithMsg("can not find name=%s pipelineConfig")
	}
	groupNames := make([]string, 0)
	for _, group := range instanceConfig.AgentGroups {
		groupNames = append(groupNames, group.Name)
	}
	return groupNames, nil
}
