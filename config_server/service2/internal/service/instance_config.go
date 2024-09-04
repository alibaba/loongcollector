package service

import (
	"config-server2/internal/common"
	proto "config-server2/internal/common/protov2"
	"config-server2/internal/entity"
	"config-server2/internal/repository"
	"log"
)

func CreateInstanceConfig(req *proto.CreateConfigRequest, res *proto.CreateConfigResponse) error {
	requestId := req.RequestId
	if requestId == nil {
		log.Print("required fields requestId is null")
		return common.ValidateErrorWithMsg("required fields requestId could not be null")
	}

	configDetail := req.ConfigDetail
	if configDetail.Name == "" {
		log.Print("required field configName is null")
		return common.ValidateErrorWithMsg("required field configName could not be null")
	}

	if configDetail.Version == 0 {
		log.Print("required field version is null")
		return common.ValidateErrorWithMsg("required field configName could not be null")
	}
	res.RequestId = requestId
	instanceConfig := entity.ParseProtoInstanceConfig2InstanceConfig(configDetail)
	err := repository.CreateInstanceConfig(instanceConfig)
	return err

}

func UpdateInstanceConfig(req *proto.UpdateConfigRequest, res *proto.UpdateConfigResponse) error {
	requestId := req.RequestId
	if requestId == nil {
		log.Print("required fields requestId is null")
		return common.ValidateErrorWithMsg("required fields requestId could not be null")
	}

	configDetail := req.ConfigDetail
	if configDetail.Name == "" {
		log.Print("required field configName is null")
		return common.ValidateErrorWithMsg("required field configName could not be null")
	}

	if configDetail.Version == 0 {
		log.Print("required field version is null")
		return common.ValidateErrorWithMsg("required field configName could not be null")
	}
	res.RequestId = requestId
	instanceConfig := entity.ParseProtoInstanceConfig2InstanceConfig(configDetail)
	err := repository.UpdateInstanceConfig(instanceConfig)
	return err
}

func DeleteInstanceConfig(req *proto.DeleteConfigRequest, res *proto.DeleteConfigResponse) error {
	requestId := req.RequestId
	if requestId == nil {
		log.Print("required fields requestId is null")
		return common.ValidateErrorWithMsg("required fields requestId could not be null")
	}

	configName := req.ConfigName
	if configName == "" {
		log.Print("required field configName is null")
		return common.ValidateErrorWithMsg("required field configName could not be null")
	}
	res.RequestId = requestId
	err := repository.DeleteInstanceConfig(configName)
	return err
}

func GetInstanceConfig(req *proto.GetConfigRequest, res *proto.GetConfigResponse) error {
	requestId := req.RequestId
	if requestId == nil {
		log.Print("required fields requestId is null")
		return common.ValidateErrorWithMsg("required fields requestId could not be null")
	}

	configName := req.ConfigName
	if configName == "" {
		log.Print("required field configName is null")
		return common.ValidateErrorWithMsg("required field configName could not be null")
	}
	res.RequestId = requestId
	instanceConfig, err := repository.GetInstanceConfig(configName)
	if err != nil {
		return err
	}
	if instanceConfig != nil {
		res.ConfigDetail = instanceConfig.Parse2ProtoInstanceConfigDetail(true)
	}
	return err
}

func ListInstanceConfigs(req *proto.ListConfigsRequest, res *proto.ListConfigsResponse) error {
	requestId := req.RequestId
	if requestId == nil {
		log.Print("required fields requestId is null")
		return common.ValidateErrorWithMsg("required fields requestId could not be null")
	}
	res.RequestId = requestId
	instanceConfigs, err := repository.ListInstanceConfigs()
	if err != nil {
		return err
	}

	for _, instanceConfig := range instanceConfigs {
		res.ConfigDetails = append(res.ConfigDetails, instanceConfig.Parse2ProtoInstanceConfigDetail(true))
	}
	return nil
}

func ApplyInstanceConfigToAgentGroup(req *proto.ApplyConfigToAgentGroupRequest, res *proto.ApplyConfigToAgentGroupResponse) error {
	requestId := req.RequestId
	if requestId == nil {
		log.Print("required fields requestId is null")
		return common.ValidateErrorWithMsg("required fields requestId could not be null")
	}
	groupName := req.GroupName
	if groupName == "" {
		log.Print("required fields groupName is null")
		return common.ValidateErrorWithMsg("required fields groupName could not be null")
	}
	configName := req.ConfigName
	if configName == "" {
		log.Print("required fields configName is null")
		return common.ValidateErrorWithMsg("required fields configName could not be null")
	}
	res.RequestId = requestId
	var err error
	err = repository.CreateInstanceConfigForAgentGroup(groupName, configName)
	if err != nil {
		return err
	}

	agents, err := repository.ListAgentsByGroupName(groupName)
	if err != nil {
		return err
	}

	agentInstanceIds := make([]string, 0)
	for _, agent := range agents {
		agentInstanceIds = append(agentInstanceIds, agent.InstanceId)
	}

	return repository.CreateInstanceConfigForAgentInGroup(agentInstanceIds, configName)

}

func RemoveInstanceConfigFromAgentGroup(req *proto.RemoveConfigFromAgentGroupRequest, res *proto.RemoveConfigFromAgentGroupResponse) error {
	requestId := req.RequestId
	if requestId == nil {
		log.Print("required fields requestId is null")
		return common.ValidateErrorWithMsg("required fields requestId could not be null")
	}
	groupName := req.GroupName
	if groupName == "" {
		log.Print("required fields groupName is null")
		return common.ValidateErrorWithMsg("required fields groupName could not be null")
	}
	configName := req.ConfigName
	if configName == "" {
		log.Print("required fields configName is null")
		return common.ValidateErrorWithMsg("required fields configName could not be null")
	}
	res.RequestId = requestId

	err := repository.DeleteInstanceConfigForAgentGroup(groupName, configName)
	if err != nil {
		return err
	}

	agents, err := repository.ListAgentsByGroupName(groupName)
	if err != nil {
		return err
	}

	agentInstanceIds := make([]string, 0)
	for _, agent := range agents {
		agentInstanceIds = append(agentInstanceIds, agent.InstanceId)
	}

	return repository.DeleteInstanceConfigForAgentInGroup(agentInstanceIds, configName)
}

func GetAppliedInstanceConfigsForAgentGroup(req *proto.GetAppliedConfigsForAgentGroupRequest, res *proto.GetAppliedConfigsForAgentGroupResponse) error {
	requestId := req.RequestId
	if requestId == nil {
		log.Print("required fields requestId is null")
		return common.ValidateErrorWithMsg("required fields requestId could not be null")
	}
	groupName := req.GroupName
	if groupName == "" {
		log.Print("required fields groupName is null")
		return common.ValidateErrorWithMsg("required fields groupName could not be null")
	}
	res.RequestId = requestId

	agentGroupDetail, err := repository.GetAgentGroupDetail(groupName, true, false)
	if err != nil {
		return err
	}
	configNames := make([]string, 0)
	for _, config := range agentGroupDetail.InstanceConfigs {
		configNames = append(configNames, config.Name)
	}

	res.ConfigNames = configNames
	return nil
}
