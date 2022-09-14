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

	"github.com/alibaba/ilogtail/config_server/service/common"
	"github.com/alibaba/ilogtail/config_server/service/manager"
	proto "github.com/alibaba/ilogtail/config_server/service/proto"
)

func CreateConfig(c *gin.Context) {
	req := proto.CreateConfigRequest{}
	res := &proto.CreateConfigResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = common.BadRequest.Code
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.ResponseId = req.RequestId

	if req.ConfigDetail.ConfigName == "" {
		res.Code = common.BadRequest.Code
		res.Message = fmt.Sprintf("Need parameter %s.", "ConfigName")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}

	if req.ConfigDetail.Content == "" {
		res.Code = common.BadRequest.Code
		res.Message = fmt.Sprintf("Need parameter %s.", "Content")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}

	c.ProtoBuf(manager.ConfigManager().CreateConfig(&req, res))
}

func UpdateConfig(c *gin.Context) {
	req := proto.UpdateConfigRequest{}
	res := &proto.UpdateConfigResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = common.BadRequest.Code
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.ResponseId = req.RequestId

	if req.ConfigDetail.ConfigName == "" {
		res.Code = common.BadRequest.Code
		res.Message = fmt.Sprintf("Need parameter %s.", "ConfigName")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}

	if req.ConfigDetail.Content == "" {
		res.Code = common.BadRequest.Code
		res.Message = fmt.Sprintf("Need parameter %s.", "Content")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}

	c.ProtoBuf(manager.ConfigManager().UpdateConfig(&req, res))
}

func DeleteConfig(c *gin.Context) {
	req := proto.DeleteConfigRequest{}
	res := &proto.DeleteConfigResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = common.BadRequest.Code
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.ResponseId = req.RequestId

	if req.ConfigName == "" {
		res.Code = common.BadRequest.Code
		res.Message = fmt.Sprintf("Need parameter %s.", "ConfigName")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}

	c.ProtoBuf(manager.ConfigManager().DeleteConfig(&req, res))
}

func GetConfig(c *gin.Context) {
	req := proto.GetConfigRequest{}
	res := &proto.GetConfigResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = common.BadRequest.Code
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.ResponseId = req.RequestId

	if req.ConfigName == "" {
		res.Code = common.BadRequest.Code
		res.Message = fmt.Sprintf("Need parameter %s.", "ConfigName")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}

	c.ProtoBuf(manager.ConfigManager().GetConfig(&req, res))
}

func ListConfigs(c *gin.Context) {
	req := proto.ListConfigsRequest{}
	res := &proto.ListConfigsResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = common.BadRequest.Code
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.ResponseId = req.RequestId

	c.ProtoBuf(manager.ConfigManager().ListConfigs(&req, res))
}

func GetAppliedAgentGroups(c *gin.Context) {
	req := proto.GetAppliedAgentGroupsRequest{}
	res := &proto.GetAppliedAgentGroupsResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = common.BadRequest.Code
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.ResponseId = req.RequestId

	if req.ConfigName == "" {
		res.Code = common.BadRequest.Code
		res.Message = fmt.Sprintf("Need parameter %s.", "ConfigName")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}

	c.ProtoBuf(manager.ConfigManager().GetAppliedAgentGroups(&req, res))
}
