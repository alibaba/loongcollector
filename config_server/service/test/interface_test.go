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

	"github.com/alibaba/ilogtail/config_server/service/common"
	configserverproto "github.com/alibaba/ilogtail/config_server/service/proto"
	"github.com/alibaba/ilogtail/config_server/service/router"
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
			So(res.Code, ShouldEqual, common.BadRequest.Code)
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
			config := &configserverproto.Config{}
			config.ConfigName = "config-1"
			config.AgentType = "ilogtail"
			config.Content = "Content for test"
			config.Description = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Get config success")
			So(res.ConfigDetail.ConfigName, ShouldEqual, configName)
			So(res.ConfigDetail.AgentType, ShouldEqual, "ilogtail")
			So(res.ConfigDetail.Content, ShouldEqual, "Content for test")
			So(res.ConfigDetail.Description, ShouldEqual, "Description for test")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-1. ")
		{
			config := &configserverproto.Config{}
			config.ConfigName = "config-1"
			config.AgentType = "ilogtail"
			config.Content = "Content for test"
			config.Description = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.ConfigAlreadyExist.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.ConfigAlreadyExist.Code)
			So(res.Message, ShouldEqual, fmt.Sprintf("Config %s already exists.", config.ConfigName))

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get all configs. ")
		{
			status, res := ListConfigs(r, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Get config list success")
			So(len(res.ConfigDetails), ShouldEqual, 1)
			So(res.ConfigDetails[0].ConfigName, ShouldEqual, "config-1")
			So(res.ConfigDetails[0].AgentType, ShouldEqual, "ilogtail")
			So(res.ConfigDetails[0].Content, ShouldEqual, "Content for test")
			So(res.ConfigDetails[0].Description, ShouldEqual, "Description for test")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-2. ")
		{
			config := &configserverproto.Config{}
			config.ConfigName = "config-2"
			config.AgentType = "ilogtail"
			config.Description = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.BadRequest.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.BadRequest.Code)
			So(res.Message, ShouldEqual, fmt.Sprintf("Need parameter %s.", "Content"))

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-2. ")
		{
			config := &configserverproto.Config{}
			config.ConfigName = "config-2"
			config.AgentType = "ilogtail"
			config.Content = "Content for test"
			config.Description = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Get config success")
			So(res.ConfigDetail.ConfigName, ShouldEqual, configName)
			So(res.ConfigDetail.AgentType, ShouldEqual, "ilogtail")
			So(res.ConfigDetail.Content, ShouldEqual, "Content for test")
			So(res.ConfigDetail.Description, ShouldEqual, "Description for test")

			requestID++
		}
	})

	Convey("Test update Config.", t, func() {
		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test update config-1. ")
		{
			config := &configserverproto.Config{}
			config.ConfigName = "config-1"
			config.AgentType = "ilogtail"
			config.Content = "Content for test-updated"
			config.Description = "Description for test-updated"

			status, res := UpdateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Get config success")
			So(res.ConfigDetail.ConfigName, ShouldEqual, configName)
			So(res.ConfigDetail.AgentType, ShouldEqual, "ilogtail")
			So(res.ConfigDetail.Content, ShouldEqual, "Content for test-updated")
			So(res.ConfigDetail.Description, ShouldEqual, "Description for test-updated")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test update config-2. ")
		{
			config := &configserverproto.Config{}
			config.ConfigName = "config-2"
			config.AgentType = "ilogtail"
			config.Content = "Content for test-updated"
			config.Description = "Description for test-updated"

			status, res := UpdateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Get config success")
			So(res.ConfigDetail.ConfigName, ShouldEqual, configName)
			So(res.ConfigDetail.AgentType, ShouldEqual, "ilogtail")
			So(res.ConfigDetail.Content, ShouldEqual, "Content for test-updated")
			So(res.ConfigDetail.Description, ShouldEqual, "Description for test-updated")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test update config-3. ")
		{
			config := &configserverproto.Config{}
			config.ConfigName = "config-3"
			config.AgentType = "ilogtail"
			config.Content = "Content for test-updated"
			config.Description = "Description for test-updated"

			status, res := UpdateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.ConfigNotExist.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.ConfigNotExist.Code)
			So(res.Message, ShouldEqual, fmt.Sprintf("Config %s doesn't exist.", config.ConfigName))

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get all configs. ")
		{
			status, res := ListConfigs(r, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Get config list success")
			So(len(res.ConfigDetails), ShouldEqual, 2)
			So(res.ConfigDetails[0].ConfigName, ShouldEqual, "config-1")
			So(res.ConfigDetails[0].AgentType, ShouldEqual, "ilogtail")
			So(res.ConfigDetails[0].Content, ShouldEqual, "Content for test-updated")
			So(res.ConfigDetails[0].Description, ShouldEqual, "Description for test-updated")
			So(res.ConfigDetails[1].ConfigName, ShouldEqual, "config-2")
			So(res.ConfigDetails[1].AgentType, ShouldEqual, "ilogtail")
			So(res.ConfigDetails[1].Content, ShouldEqual, "Content for test-updated")
			So(res.ConfigDetails[1].Description, ShouldEqual, "Description for test-updated")

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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.ConfigNotExist.Code)
			So(res.Message, ShouldEqual, fmt.Sprintf("Config %s doesn't exist.", configName))

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-1. ")
		{
			config := &configserverproto.Config{}
			config.ConfigName = "config-1"
			config.AgentType = "ilogtail"
			config.Content = "Content for test"
			config.Description = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Get config success")
			So(res.ConfigDetail.ConfigName, ShouldEqual, configName)
			So(res.ConfigDetail.AgentType, ShouldEqual, "ilogtail")
			So(res.ConfigDetail.Content, ShouldEqual, "Content for test")
			So(res.ConfigDetail.Description, ShouldEqual, "Description for test")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test delete config-1. ")
		{
			configName := "config-1"

			status, res := DeleteConfig(r, configName, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.ConfigNotExist.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.ConfigNotExist.Code)
			So(res.Message, ShouldEqual, fmt.Sprintf("Config %s doesn't exist.", configName))

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get all configs. ")
		{
			status, res := ListConfigs(r, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			config := &configserverproto.Config{}
			config.ConfigName = "config-1"
			config.AgentType = "ilogtail"
			config.Content = "Content for test"
			config.Description = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Add config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-2. ")
		{
			config := &configserverproto.Config{}
			config.ConfigName = "config-2"
			config.AgentType = "ilogtail"
			config.Content = "Content for test"
			config.Description = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.ConfigNotExist.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.InternalServerError.Code)
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
			So(res.Code, ShouldEqual, common.InternalServerError.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test ilogtail-1 send Heartbeat. ")
		{
			agent := new(configserverproto.Agent)
			agent.AgentId = "ilogtail-1"
			agent.AgentType = "ilogtail"
			agent.Version = "1.1.1"
			agent.Ip = "127.0.0.1"
			agent.RunningStatus = "good"
			agent.StartupTime = 100
			agent.Tags = map[string]string{}

			status, res := HeartBeat(r, agent, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Send heartbeat success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test ilogtail-1 send RunningStatistics. ")
		{
			agentID := "ilogtail-1"
			runningStatistics := &configserverproto.RunningStatistics{Cpu: 0, Memory: 0, Extras: map[string]string{}}

			status, res := RunningStatistics(r, agentID, runningStatistics, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Send running statistics success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test ilogtail-1 send Alarm. ")
		{
			agentID := "ilogtail-1"
			alarmType := "test"
			alarmDetail := "test"

			status, res := Alarm(r, agentID, alarmType, alarmDetail, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Alarm success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Sleep 1s. ")
		{
			time.Sleep(time.Second * 1)

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test ilogtail-2 send Heartbeat. ")
		{
			agent := new(configserverproto.Agent)
			agent.AgentId = "ilogtail-2"
			agent.AgentType = "ilogtail"
			agent.Version = "1.1.0"
			agent.Ip = "127.0.0.1"
			agent.RunningStatus = "good"
			agent.StartupTime = 200
			agent.Tags = map[string]string{}

			status, res := HeartBeat(r, agent, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Send heartbeat success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test ilogtail-2 send RunningStatistics. ")
		{
			agentID := "ilogtail-2"
			runningStatistics := &configserverproto.RunningStatistics{Cpu: 100, Memory: 100, Extras: map[string]string{}}

			status, res := RunningStatistics(r, agentID, runningStatistics, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Send running statistics success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test ilogtail-2 send Alarm. ")
		{
			agentID := "ilogtail-2"
			alarmType := "test"
			alarmDetail := "test"

			status, res := Alarm(r, agentID, alarmType, alarmDetail, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Alarm success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Sleep 1s. ")
		{
			time.Sleep(time.Second * 1)

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test ilogtail-1 send Heartbeat. ")
		{
			agent := new(configserverproto.Agent)
			agent.AgentId = "ilogtail-1"
			agent.AgentType = "ilogtail"
			agent.Version = "1.1.1"
			agent.Ip = "127.0.0.1"
			agent.RunningStatus = "good"
			agent.StartupTime = 100
			agent.Tags = map[string]string{}

			status, res := HeartBeat(r, agent, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Send heartbeat success")

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
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Get agent list success")
			So(len(res.Agents), ShouldEqual, 2)

			var agent1, agent2 int
			if res.Agents[0].AgentId == "ilogtail-1" {
				agent1 = 0
				agent2 = 1
			} else {
				agent1 = 1
				agent2 = 0
			}

			So(res.Agents[agent1].LatestHeartbeatTime, ShouldBeGreaterThan, res.Agents[agent2].LatestHeartbeatTime)

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
		agent := new(configserverproto.Agent)
		agent.AgentId = "ilogtail-1"
		agent.AgentType = "ilogtail"
		agent.Version = "1.1.1"
		agent.Ip = "127.0.0.1"
		agent.RunningStatus = "good"
		agent.StartupTime = 100
		agent.Tags = map[string]string{}

		configVersions := map[string]int64{}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test ilogtail-1 send Heartbeat. ")
		{
			status, res := HeartBeat(r, agent, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Send heartbeat success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Sleep 2s, wait for writing agent info to store. ")
		{
			time.Sleep(time.Second * 2)

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-1. ")
		{
			config := &configserverproto.Config{}
			config.ConfigName = "config-1"
			config.AgentType = "ilogtail"
			config.Content = "Content for test"
			config.Description = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Add config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-2. ")
		{
			config := &configserverproto.Config{}
			config.ConfigName = "config-2"
			config.AgentType = "ilogtail"
			config.Content = "Content for test"
			config.Description = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Add config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-3. ")
		{
			config := &configserverproto.Config{}
			config.ConfigName = "config-3"
			config.AgentType = "ilogtail"
			config.Content = "Content for test"
			config.Description = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Add config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test create config-4. ")
		{
			config := &configserverproto.Config{}
			config.ConfigName = "config-4"
			config.AgentType = "ilogtail"
			config.Content = "Content for test"
			config.Description = "Description for test"

			status, res := CreateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Add config to agent group success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get ilogtail-1's configs. ")
		{
			status, res := GetConfigList(r, agent.AgentId, configVersions, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Get config update infos success")

			for _, info := range res.ConfigUpdateInfos {
				configVersions[info.ConfigName] = info.ConfigVersion
				switch info.ConfigName {
				case "config-1":
					So(info.UpdateStatus.String(), ShouldEqual, "NEW")
				case "config-2":
					So(info.UpdateStatus.String(), ShouldEqual, "NEW")
				case "config-3":
					So(info.UpdateStatus.String(), ShouldEqual, "NEW")
				}
			}

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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Remove config from agent group success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test update config-2. ")
		{
			config := &configserverproto.Config{}
			config.ConfigName = "config-2"
			config.AgentType = "ilogtail"
			config.Content = "Content for test-updated"
			config.Description = "Description for test-updated"

			status, res := UpdateConfig(r, config, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Update config success")

			requestID++
		}

		fmt.Print("\n\t" + fmt.Sprint(requestID) + ":Test get ilogtail-1's configs. ")
		{
			status, res := GetConfigList(r, agent.AgentId, configVersions, fmt.Sprint(requestID))

			// check
			So(status, ShouldEqual, common.Accept.Status)
			So(res.ResponseId, ShouldEqual, fmt.Sprint(requestID))
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Get config update infos success")

			configVersions = map[string]int64{}
			for _, info := range res.ConfigUpdateInfos {
				configVersions[info.ConfigName] = info.ConfigVersion
				switch info.ConfigName {
				case "config-1":
					So(info.UpdateStatus.String(), ShouldEqual, "DELETED")
				case "config-2":
					So(info.UpdateStatus.String(), ShouldEqual, "MODIFIED")
				case "config-3":
					So(info.UpdateStatus.String(), ShouldEqual, "SAME")
				case "config-4":
					So(info.UpdateStatus.String(), ShouldEqual, "NEW")
				}
			}

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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
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
			So(res.Code, ShouldEqual, common.Accept.Code)
			So(res.Message, ShouldEqual, "Delete config success")

			requestID++
		}
	})
}
