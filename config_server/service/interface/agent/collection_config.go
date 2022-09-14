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
	"fmt"

	"github.com/gin-gonic/gin"
	"github.com/gin-gonic/gin/binding"

	"github.com/alibaba/ilogtail/config_server/service/common"
	"github.com/alibaba/ilogtail/config_server/service/manager"
	proto "github.com/alibaba/ilogtail/config_server/service/proto"
)

func GetConfigList(c *gin.Context) {
	req := proto.AgentGetConfigListRequest{}
	res := &proto.AgentGetConfigListResponse{}

	err := c.ShouldBindBodyWith(&req, binding.ProtoBuf)
	if err != nil {
		res.Code = common.BadRequest.Code
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	res.ResponseId = req.RequestId

	if req.AgentId == "" {
		res.Code = common.BadRequest.Code
		res.Message = fmt.Sprintf("Need parameter %s.", "AgentID")
		c.ProtoBuf(common.BadRequest.Status, res)
		return
	}
	if req.ConfigVersions == nil {
		req.ConfigVersions = make(map[string]int64)
	}

	c.ProtoBuf(manager.ConfigManager().GetConfigList(&req, res))
}
