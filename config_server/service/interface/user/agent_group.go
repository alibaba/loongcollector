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

package user

import (
	"fmt"

	"github.com/gin-gonic/gin"
	"github.com/gin-gonic/gin/binding"

	"config-server/common"
	"config-server/manager"
	proto "config-server/proto"
)

func CreateAgentGroup(c *gin.Context) {
	req := proto.CreateAgentGroupRequest{}
	res := &proto.CreateAgentGroupResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.ResponseId = req.RequestId

	if req.AgentGroup.GroupName == "" {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Agent group need parameter %s.", "GroupName")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}

	c.ProtoBuf(manager.ConfigManager().CreateAgentGroup(&req, res))
}

func UpdateAgentGroup(c *gin.Context) {
	req := proto.UpdateAgentGroupRequest{}
	res := &proto.UpdateAgentGroupResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.ResponseId = req.RequestId

	if req.AgentGroup.GroupName == "" {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Agent group need parameter %s.", "groupName")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}

	c.ProtoBuf(manager.ConfigManager().UpdateAgentGroup(&req, res))
}

func DeleteAgentGroup(c *gin.Context) {
	req := proto.DeleteAgentGroupRequest{}
	res := &proto.DeleteAgentGroupResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.ResponseId = req.RequestId

	if req.GroupName == "" {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Agent group need parameter %s.", "groupName")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}

	c.ProtoBuf(manager.ConfigManager().DeleteAgentGroup(&req, res))
}

func GetAgentGroup(c *gin.Context) {
	req := proto.GetAgentGroupRequest{}
	res := &proto.GetAgentGroupResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.ResponseId = req.RequestId

	if req.GroupName == "" {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Agent group need parameter %s.", "groupName")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}

	c.ProtoBuf(manager.ConfigManager().GetAgentGroup(&req, res))
}

func ListAgentGroups(c *gin.Context) {
	req := proto.ListAgentGroupsRequest{}
	res := &proto.ListAgentGroupsResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.ResponseId = req.RequestId

	c.ProtoBuf(manager.ConfigManager().ListAgentGroups(&req, res))
}

func ListAgents(c *gin.Context) {
	req := proto.ListAgentsRequest{}
	res := &proto.ListAgentsResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.ResponseId = req.RequestId

	if req.GroupName == "" {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Need parameter %s.", "groupName")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}

	c.ProtoBuf(manager.ConfigManager().ListAgents(&req, res))
}

func GetAppliedConfigsForAgentGroup(c *gin.Context) {
	req := proto.GetAppliedConfigsForAgentGroupRequest{}
	res := &proto.GetAppliedConfigsForAgentGroupResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.ResponseId = req.RequestId

	if req.GroupName == "" {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Need parameter %s.", "GroupName")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}

	c.ProtoBuf(manager.ConfigManager().GetAppliedConfigsForAgentGroup(&req, res))
}

func ApplyConfigToAgentGroup(c *gin.Context) {
	req := proto.ApplyConfigToAgentGroupRequest{}
	res := &proto.ApplyConfigToAgentGroupResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.ResponseId = req.RequestId

	if req.GroupName == "" {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Need parameter %s.", "groupName")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	if req.ConfigName == "" {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Need parameter %s.", "configName")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}

	c.ProtoBuf(manager.ConfigManager().ApplyConfigToAgentGroup(&req, res))
}

func RemoveConfigFromAgentGroup(c *gin.Context) {
	req := proto.RemoveConfigFromAgentGroupRequest{}
	res := &proto.RemoveConfigFromAgentGroupResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.ResponseId = req.RequestId

	if req.GroupName == "" {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Need parameter %s.", "groupName")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	if req.ConfigName == "" {
		res.Code = proto.RespCode_INVALID_PARAMETER
		res.Message = fmt.Sprintf("Need parameter %s.", "configName")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}

	c.ProtoBuf(manager.ConfigManager().RemoveConfigFromAgentGroup(&req, res))
}
