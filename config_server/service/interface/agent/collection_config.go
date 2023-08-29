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

package agent

import (
	"github.com/gin-gonic/gin"
	"github.com/gin-gonic/gin/binding"

	"config-server/common"
	"config-server/manager"
	proto "config-server/proto"
)

func FetchPipelineConfig(c *gin.Context) {
	req := proto.FetchPipelineConfigRequest{}
	res := &proto.FetchPipelineConfigResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.RequestId = req.RequestId

	if req.ReqConfigs == nil {
		req.ReqConfigs = []*proto.ConfigInfo{}
	}

	c.ProtoBuf(manager.ConfigManager().FetchPipelineConfig(&req, res))
}

func FetchAgentConfig(c *gin.Context) {
	req := proto.FetchAgentConfigRequest{}
	res := &proto.FetchAgentConfigResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = proto.RespCode_INTERNAL_SERVER_ERROR
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.RequestId = req.RequestId

	if req.ReqConfigs == nil {
		req.ReqConfigs = []*proto.ConfigInfo{}
	}

	c.ProtoBuf(manager.ConfigManager().FetchAgentConfig(&req, res))
}
