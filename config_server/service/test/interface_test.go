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

package test

import (
	"fmt"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	. "github.com/smartystreets/goconvey/convey"

	"config-server/common"
	proto "config-server/proto"
	"config-server/router"
)

func TestBaseAgentGroup(t *testing.T) {
	gin.SetMode(gin.TestMode)
	r := gin.New()
	router.InitUserRouter(r)

	var requestID int

	Convey("Test delete AgentGroup.", t, func() {
		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test delete default. ")
		{
			status, res := DeleteAgentGroup(r, "default", fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.BadRequest.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_INVALID_PARAMETER)
			So(res.Message, ShouldEqual, "Cannot delete agent group 'default'")

			requestID++
		}
	})
}

func TestBaseConfig(t *testing.T) {
	gin.SetMode(gin.TestMode)
	r := gin.New()
	router.InitUserRouter(r)

	var requestID int

	Convey("Test create Config.", t, func() {
		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-1. ")
		{
			config := &proto.ConfigDetail{}
			config.Name = "config-1"
			config.Type = proto.ConfigType_PIPELINE_CONFIG
			config.Detail = "Detail for test"
			config.Context = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Add config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get config-1. ")
		{
			configName := "config-1"

			status, res := GetConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Get config success")
			So(res.ConfigDetail.Name, ShouldEqual, configName)
			So(res.ConfigDetail.Type, ShouldEqual, proto.ConfigType_PIPELINE_CONFIG)
			So(res.ConfigDetail.Detail, ShouldEqual, "Detail for test")
			So(res.ConfigDetail.Context, ShouldEqual, "Description for test")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-1. ")
		{
			config := &proto.ConfigDetail{}
			config.Name = "config-1"
			config.Type = proto.ConfigType_PIPELINE_CONFIG
			config.Detail = "Detail for test"
			config.Context = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.ConfigAlreadyExist.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_INVALID_PARAMETER)
			So(res.Message, ShouldEqual, fmt.Sprintf("Config %s already exists.", config.Name))

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get all configs. ")
		{
			status, res := ListConfigs(r, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Get config list success")
			So(len(res.ConfigDetails), ShouldEqual, 1)
			So(res.ConfigDetails[0].Name, ShouldEqual, "config-1")
			So(res.ConfigDetails[0].Type, ShouldEqual, proto.ConfigType_PIPELINE_CONFIG)
			So(res.ConfigDetails[0].Detail, ShouldEqual, "Detail for test")
			So(res.ConfigDetails[0].Context, ShouldEqual, "Description for test")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-2. ")
		{
			config := &proto.ConfigDetail{}
			config.Name = "config-2"
			config.Type = proto.ConfigType_PIPELINE_CONFIG
			config.Context = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.BadRequest.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_INVALID_PARAMETER)
			So(res.Message, ShouldEqual, fmt.Sprintf("Need parameter %s.", "Detail"))

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-2. ")
		{
			config := &proto.ConfigDetail{}
			config.Name = "config-2"
			config.Type = proto.ConfigType_PIPELINE_CONFIG
			config.Detail = "Detail for test"
			config.Context = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Add config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get config-2. ")
		{
			configName := "config-2"

			status, res := GetConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Get config success")
			So(res.ConfigDetail.Name, ShouldEqual, configName)
			So(res.ConfigDetail.Type, ShouldEqual, proto.ConfigType_PIPELINE_CONFIG)
			So(res.ConfigDetail.Detail, ShouldEqual, "Detail for test")
			So(res.ConfigDetail.Context, ShouldEqual, "Description for test")

			requestID++
		}
	})

	Convey("Test update Config.", t, func() {
		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test update config-1. ")
		{
			config := &proto.ConfigDetail{}
			config.Name = "config-1"
			config.Type = proto.ConfigType_PIPELINE_CONFIG
			config.Detail = "Detail for test-updated"
			config.Context = "Description for test-updated"

			status, res := UpdateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Update config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get config-1. ")
		{
			configName := "config-1"

			status, res := GetConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Get config success")
			So(res.ConfigDetail.Name, ShouldEqual, configName)
			So(res.ConfigDetail.Type, ShouldEqual, proto.ConfigType_PIPELINE_CONFIG)
			So(res.ConfigDetail.Detail, ShouldEqual, "Detail for test-updated")
			So(res.ConfigDetail.Context, ShouldEqual, "Description for test-updated")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test update config-2. ")
		{
			config := &proto.ConfigDetail{}
			config.Name = "config-2"
			config.Type = proto.ConfigType_PIPELINE_CONFIG
			config.Detail = "Detail for test-updated"
			config.Context = "Description for test-updated"

			status, res := UpdateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Update config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get config-2. ")
		{
			configName := "config-2"

			status, res := GetConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Get config success")
			So(res.ConfigDetail.Name, ShouldEqual, configName)
			So(res.ConfigDetail.Type, ShouldEqual, proto.ConfigType_PIPELINE_CONFIG)
			So(res.ConfigDetail.Detail, ShouldEqual, "Detail for test-updated")
			So(res.ConfigDetail.Context, ShouldEqual, "Description for test-updated")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test update config-3. ")
		{
			config := &proto.ConfigDetail{}
			config.Name = "config-3"
			config.Type = proto.ConfigType_PIPELINE_CONFIG
			config.Detail = "Detail for test-updated"
			config.Context = "Description for test-updated"

			status, res := UpdateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.ConfigNotExist.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_INVALID_PARAMETER)
			So(res.Message, ShouldEqual, fmt.Sprintf("Config %s doesn't exist.", config.Name))

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get all configs. ")
		{
			status, res := ListConfigs(r, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Get config list success")
			So(len(res.ConfigDetails), ShouldEqual, 2)
			So(res.ConfigDetails[0].Name, ShouldEqual, "config-1")
			So(res.ConfigDetails[0].Type, ShouldEqual, proto.ConfigType_PIPELINE_CONFIG)
			So(res.ConfigDetails[0].Detail, ShouldEqual, "Detail for test-updated")
			So(res.ConfigDetails[0].Context, ShouldEqual, "Description for test-updated")
			So(res.ConfigDetails[1].Name, ShouldEqual, "config-2")
			So(res.ConfigDetails[1].Type, ShouldEqual, proto.ConfigType_PIPELINE_CONFIG)
			So(res.ConfigDetails[1].Detail, ShouldEqual, "Detail for test-updated")
			So(res.ConfigDetails[1].Context, ShouldEqual, "Description for test-updated")

			requestID++
		}
	})

	Convey("Test delete Config.", t, func() {
		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test delete config-1. ")
		{
			configName := "config-1"

			status, res := DeleteConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Delete config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get config-1. ")
		{
			configName := "config-1"

			status, res := GetConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.ConfigNotExist.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_INVALID_PARAMETER)
			So(res.Message, ShouldEqual, fmt.Sprintf("Config %s doesn't exist.", configName))

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-1. ")
		{
			config := &proto.ConfigDetail{}
			config.Name = "config-1"
			config.Type = proto.ConfigType_PIPELINE_CONFIG
			config.Detail = "Detail for test"
			config.Context = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Add config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get config-1. ")
		{
			configName := "config-1"

			status, res := GetConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Get config success")
			So(res.ConfigDetail.Name, ShouldEqual, configName)
			So(res.ConfigDetail.Type, ShouldEqual, proto.ConfigType_PIPELINE_CONFIG)
			So(res.ConfigDetail.Detail, ShouldEqual, "Detail for test")
			So(res.ConfigDetail.Context, ShouldEqual, "Description for test")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test delete config-1. ")
		{
			configName := "config-1"

			status, res := DeleteConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Delete config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get config-1. ")
		{
			configName := "config-1"

			status, res := GetConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.ConfigNotExist.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_INVALID_PARAMETER)
			So(res.Message, ShouldEqual, fmt.Sprintf("Config %s doesn't exist.", configName))

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test delete config-2. ")
		{
			configName := "config-2"

			status, res := DeleteConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Delete config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get config-2. ")
		{
			configName := "config-2"

			status, res := GetConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.ConfigNotExist.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_INVALID_PARAMETER)
			So(res.Message, ShouldEqual, fmt.Sprintf("Config %s doesn't exist.", configName))

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get all configs. ")
		{
			status, res := ListConfigs(r, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Get config list success")
			So(len(res.ConfigDetails), ShouldEqual, 0)

			requestID++
		}
	})
}

func TestOperationsBetweenConfigAndAgentGroup(t *testing.T) {
	gin.SetMode(gin.TestMode)
	r := gin.New()
	router.InitUserRouter(r)

	var requestID int

	Convey("Test apply.", t, func() {
		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-1. ")
		{
			config := &proto.ConfigDetail{}
			config.Name = "config-1"
			config.Type = proto.ConfigType_PIPELINE_CONFIG
			config.Detail = "Detail for test"
			config.Context = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Add config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-2. ")
		{
			config := &proto.ConfigDetail{}
			config.Name = "config-2"
			config.Type = proto.ConfigType_PIPELINE_CONFIG
			config.Detail = "Detail for test"
			config.Context = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Add config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test apply config-1 to default. ")
		{
			configName := "config-1"
			groupName := "default"

			status, res := ApplyConfigToAgentGroup(r, configName, groupName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Add config to agent group success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get default's Configs. ")
		{
			groupName := "default"

			status, res := GetAppliedConfigsForAgentGroup(r, groupName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Get agent group's applied configs success")
			So(len(res.ConfigNames), ShouldEqual, 1)
			So(res.ConfigNames[0], ShouldEqual, "config-1")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get config-1's AgentGroups. ")
		{
			configName := "config-1"

			status, res := GetAppliedAgentGroups(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Get group list success")
			So(len(res.AgentGroupNames), ShouldEqual, 1)
			So(res.AgentGroupNames[0], ShouldEqual, "default")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get config-3's AgentGroups. ")
		{
			configName := "config-3"

			status, res := GetAppliedAgentGroups(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.ConfigNotExist.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_INVALID_PARAMETER)
			So(res.Message, ShouldEqual, fmt.Sprintf("Config %s doesn't exist.", configName))

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test apply config-2 to default. ")
		{
			configName := "config-2"
			groupName := "default"

			status, res := ApplyConfigToAgentGroup(r, configName, groupName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Add config to agent group success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get default's Configs. ")
		{
			groupName := "default"

			status, res := GetAppliedConfigsForAgentGroup(r, groupName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Get agent group's applied configs success")
			So(len(res.ConfigNames), ShouldEqual, 2)
			So(res.ConfigNames, ShouldContain, "config-1")
			So(res.ConfigNames, ShouldContain, "config-2")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test delete config-1. ")
		{
			configName := "config-1"

			status, res := DeleteConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.InternalServerError.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_INTERNAL_SERVER_ERROR)
			So(res.Message, ShouldEqual, fmt.Sprintf("Config %s was applied to some agent groups, cannot be deleted.", configName))

			requestID++
		}
	})

	Convey("Test remove.", t, func() {
		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test delete config-2. ")
		{
			configName := "config-2"

			status, res := DeleteConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.InternalServerError.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_INTERNAL_SERVER_ERROR)
			So(res.Message, ShouldEqual, fmt.Sprintf("Config %s was applied to some agent groups, cannot be deleted.", configName))

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test remove config-2 from default. ")
		{
			configName := "config-2"
			groupName := "default"

			status, res := RemoveConfigFromAgentGroup(r, configName, groupName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Remove config from agent group success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get default's Configs. ")
		{
			groupName := "default"

			status, res := GetAppliedConfigsForAgentGroup(r, groupName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Get agent group's applied configs success")
			So(len(res.ConfigNames), ShouldEqual, 1)
			So(res.ConfigNames[0], ShouldEqual, "config-1")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test delete config-2. ")
		{
			configName := "config-2"

			status, res := DeleteConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Delete config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test remove config-1 from default. ")
		{
			configName := "config-1"
			groupName := "default"

			status, res := RemoveConfigFromAgentGroup(r, configName, groupName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Remove config from agent group success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get default's Configs. ")
		{
			groupName := "default"

			status, res := GetAppliedConfigsForAgentGroup(r, groupName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Get agent group's applied configs success")
			So(len(res.ConfigNames), ShouldEqual, 0)

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test delete config-1. ")
		{
			configName := "config-1"

			status, res := DeleteConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Delete config success")

			requestID++
		}
	})
}

func TestAgentSendMessage(t *testing.T) {
	gin.SetMode(gin.TestMode)
	r := gin.New()
	router.InitAgentRouter(r)
	router.InitUserRouter(r)

	var requestID int

	Convey("Test Agent send message.", t, func() {
		configInfos := make([]*proto.ConfigCheckResult, 0)

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test ilogtail-1 send Heartbeat. ")
		{
			agent := new(proto.Agent)
			agent.AgentId = "ilogtail-1"
			agent.Attributes = &proto.AgentAttributes{}
			agent.RunningStatus = "good"
			agent.StartupTime = 100

			status, res := HeartBeat(r, agent, configInfos, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.RequestId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Send heartbeat successGet config update infos success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Sleep 1s. ")
		{
			time.Sleep(time.Second * 1)

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test ilogtail-2 send Heartbeat. ")
		{
			agent := new(proto.Agent)
			agent.AgentId = "ilogtail-2"
			agent.Attributes = &proto.AgentAttributes{}
			agent.RunningStatus = "good"
			agent.StartupTime = 200

			status, res := HeartBeat(r, agent, configInfos, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.RequestId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Send heartbeat successGet config update infos success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Sleep 1s. ")
		{
			time.Sleep(time.Second * 1)

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test ilogtail-1 send Heartbeat. ")
		{
			agent := new(proto.Agent)
			agent.AgentId = "ilogtail-1"
			agent.Attributes = &proto.AgentAttributes{}
			agent.RunningStatus = "good"
			agent.StartupTime = 100

			status, res := HeartBeat(r, agent, configInfos, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.RequestId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Send heartbeat successGet config update infos success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Sleep 2s, wait for writing agent info to store. ")
		{
			time.Sleep(time.Second * 2)

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get default's Agents. ")
		{
			groupName := "default"

			status, res := ListAgents(r, groupName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Get agent list success")
			So(len(res.Agents), ShouldEqual, 2)
			requestID++
		}
	})
}

func TestAgentGetConfig(t *testing.T) {
	gin.SetMode(gin.TestMode)
	r := gin.New()
	router.InitAgentRouter(r)
	router.InitUserRouter(r)

	var requestID int

	Convey("Test Agent get config.", t, func() {
		agent := new(proto.Agent)
		agent.AgentId = "ilogtail-1"
		agent.Attributes = &proto.AgentAttributes{}
		agent.RunningStatus = "good"
		agent.StartupTime = 100

		configVersions := map[string]int64{}
		configInfos := make([]*proto.ConfigCheckResult, 0)

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-1. ")
		{
			config := &proto.ConfigDetail{}
			config.Name = "config-1"
			config.Type = proto.ConfigType_PIPELINE_CONFIG
			config.Detail = "Detail for test"
			config.Context = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Add config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-2. ")
		{
			config := &proto.ConfigDetail{}
			config.Name = "config-2"
			config.Type = proto.ConfigType_PIPELINE_CONFIG
			config.Detail = "Detail for test"
			config.Context = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Add config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-3. ")
		{
			config := &proto.ConfigDetail{}
			config.Name = "config-3"
			config.Type = proto.ConfigType_PIPELINE_CONFIG
			config.Detail = "Detail for test"
			config.Context = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Add config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-4. ")
		{
			config := &proto.ConfigDetail{}
			config.Name = "config-4"
			config.Type = proto.ConfigType_PIPELINE_CONFIG
			config.Detail = "Detail for test"
			config.Context = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Add config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test apply config-1 to default. ")
		{
			configName := "config-1"
			groupName := "default"

			status, res := ApplyConfigToAgentGroup(r, configName, groupName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Add config to agent group success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test apply config-2 to default. ")
		{
			configName := "config-2"
			groupName := "default"

			status, res := ApplyConfigToAgentGroup(r, configName, groupName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Add config to agent group success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test apply config-3 to default. ")
		{
			configName := "config-3"
			groupName := "default"

			status, res := ApplyConfigToAgentGroup(r, configName, groupName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Add config to agent group success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Sleep 3s, wait for writing config info to store. ")
		{
			time.Sleep(time.Second * 3)

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test ilogtail-1 send Heartbeat. ")
		{
			status, res := HeartBeat(r, agent, configInfos, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.RequestId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Send heartbeat successGet config update infos success")
			So(len(res.PipelineCheckResults), ShouldEqual, 3)
			for _, info := range res.PipelineCheckResults {
				configVersions[info.Name] = info.NewVersion
				switch info.Name {
				case "config-1":
					So(info.CheckStatus, ShouldEqual, proto.CheckStatus_NEW)
				case "config-2":
					So(info.CheckStatus, ShouldEqual, proto.CheckStatus_NEW)
				case "config-3":
					So(info.CheckStatus, ShouldEqual, proto.CheckStatus_NEW)
				}
			}
			configInfos = res.PipelineCheckResults

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test fetch ilogtail-1's configs. ")
		{
			status, res := FetchPipelineConfig(r, configInfos, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.RequestId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Get Agent Config details success")
			So(len(res.ConfigDetails), ShouldEqual, 3)
			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test apply config-4 to default. ")
		{
			configName := "config-4"
			groupName := "default"

			status, res := ApplyConfigToAgentGroup(r, configName, groupName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Add config to agent group success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test remove config-1 from default. ")
		{
			configName := "config-1"
			groupName := "default"

			status, res := RemoveConfigFromAgentGroup(r, configName, groupName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Remove config from agent group success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test update config-2. ")
		{
			config := &proto.ConfigDetail{}
			config.Name = "config-2"
			config.Type = proto.ConfigType_PIPELINE_CONFIG
			config.Detail = "Detail for test-updated"
			config.Context = "Description for test-updated"

			status, res := UpdateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Update config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Sleep 3s, wait for writing config info to store. ")
		{
			time.Sleep(time.Second * 3)

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test ilogtail-1 send Heartbeat. ")
		{
			status, res := HeartBeat(r, agent, configInfos, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.RequestId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Send heartbeat successGet config update infos success")
			So(len(res.PipelineCheckResults), ShouldEqual, 3)
			configVersions = map[string]int64{}
			for _, info := range res.PipelineCheckResults {
				configVersions[info.Name] = info.NewVersion
				switch info.Name {
				case "config-1":
					So(info.CheckStatus, ShouldEqual, proto.CheckStatus_DELETED)
				case "config-2":
					So(info.CheckStatus, ShouldEqual, proto.CheckStatus_MODIFIED)
				case "config-4":
					So(info.CheckStatus, ShouldEqual, proto.CheckStatus_NEW)
				}
			}
			configInfos = res.PipelineCheckResults

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test fetch ilogtail-1's configs. ")
		{
			status, res := FetchPipelineConfig(r, configInfos, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.RequestId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Get Agent Config details success")
			So(len(res.ConfigDetails), ShouldEqual, 3)
			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test remove config-2 from default. ")
		{
			configName := "config-2"
			groupName := "default"

			status, res := RemoveConfigFromAgentGroup(r, configName, groupName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Remove config from agent group success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test remove config-3 from default. ")
		{
			configName := "config-3"
			groupName := "default"

			status, res := RemoveConfigFromAgentGroup(r, configName, groupName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Remove config from agent group success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test remove config-4 from default. ")
		{
			configName := "config-4"
			groupName := "default"

			status, res := RemoveConfigFromAgentGroup(r, configName, groupName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Remove config from agent group success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test delete config-1. ")
		{
			configName := "config-1"

			status, res := DeleteConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Delete config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test delete config-2. ")
		{
			configName := "config-2"

			status, res := DeleteConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Delete config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test delete config-3. ")
		{
			configName := "config-3"

			status, res := DeleteConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Delete config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test delete config-4. ")
		{
			configName := "config-4"

			status, res := DeleteConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, proto.RespCode_ACCEPT)
			So(res.Message, ShouldEqual, "Delete config success")

			requestID++
		}
	})
}
