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
	"bytes"
	"io"
	"net/http"
	"net/http/httptest"

	"github.com/gin-gonic/gin"
	"google.golang.org/protobuf/proto"

	configserverproto "config-server/proto"
)

func DeleteAgentGroup(r *gin.Engine, groupName string, requestID string) (int, *configserverproto.DeleteAgentGroupResponse) {
	// data
	reqBody := configserverproto.DeleteAgentGroupRequest{}
	reqBody.RequestId = requestID
	reqBody.GroupName = groupName
	reqBodyByte, _ := proto.Marshal(&reqBody)

	// request
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("DELETE", "/User/DeleteAgentGroup", bytes.NewBuffer(reqBodyByte))
	r.ServeHTTP(w, req)

	// response
	res := w.Result()
	resBodyByte, _ := io.ReadAll(res.Body)
	resBody := new(configserverproto.DeleteAgentGroupResponse)
	_ = proto.Unmarshal(resBodyByte, resBody)

	return res.StatusCode, resBody
}

func CreateConfig(r *gin.Engine, config *configserverproto.ConfigDetail, requestID string) (int, *configserverproto.CreateConfigResponse) {
	// data
	reqBody := configserverproto.CreateConfigRequest{}
	reqBody.RequestId = requestID
	reqBody.ConfigDetail = config
	reqBodyByte, _ := proto.Marshal(&reqBody)

	// request
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("POST", "/User/CreateConfig", bytes.NewBuffer(reqBodyByte))
	r.ServeHTTP(w, req)

	// response
	res := w.Result()
	resBodyByte, _ := io.ReadAll(res.Body)
	resBody := new(configserverproto.CreateConfigResponse)
	_ = proto.Unmarshal(resBodyByte, resBody)

	return res.StatusCode, resBody
}

func GetConfig(r *gin.Engine, configName string, requestID string) (int, *configserverproto.GetConfigResponse) {
	// data
	reqBody := configserverproto.GetConfigRequest{}
	reqBody.RequestId = requestID
	reqBody.ConfigName = configName
	reqBodyByte, _ := proto.Marshal(&reqBody)

	// request
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("POST", "/User/GetConfig", bytes.NewBuffer(reqBodyByte))
	r.ServeHTTP(w, req)

	// response
	res := w.Result()
	resBodyByte, _ := io.ReadAll(res.Body)
	resBody := new(configserverproto.GetConfigResponse)
	_ = proto.Unmarshal(resBodyByte, resBody)

	return res.StatusCode, resBody
}

func UpdateConfig(r *gin.Engine, config *configserverproto.ConfigDetail, requestID string) (int, *configserverproto.UpdateConfigResponse) {
	// data
	reqBody := configserverproto.UpdateConfigRequest{}
	reqBody.RequestId = requestID
	reqBody.ConfigDetail = config
	reqBodyByte, _ := proto.Marshal(&reqBody)

	// request
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("PUT", "/User/UpdateConfig", bytes.NewBuffer(reqBodyByte))
	r.ServeHTTP(w, req)

	// response
	res := w.Result()
	resBodyByte, _ := io.ReadAll(res.Body)
	resBody := new(configserverproto.UpdateConfigResponse)
	_ = proto.Unmarshal(resBodyByte, resBody)

	return res.StatusCode, resBody
}

func DeleteConfig(r *gin.Engine, configName string, requestID string) (int, *configserverproto.DeleteConfigResponse) {
	// data
	reqBody := configserverproto.DeleteConfigRequest{}
	reqBody.RequestId = requestID
	reqBody.ConfigName = configName
	reqBodyByte, _ := proto.Marshal(&reqBody)

	// request
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("DELETE", "/User/DeleteConfig", bytes.NewBuffer(reqBodyByte))
	r.ServeHTTP(w, req)

	// response
	res := w.Result()
	resBodyByte, _ := io.ReadAll(res.Body)
	resBody := new(configserverproto.DeleteConfigResponse)
	_ = proto.Unmarshal(resBodyByte, resBody)

	return res.StatusCode, resBody
}

func ListConfigs(r *gin.Engine, requestID string) (int, *configserverproto.ListConfigsResponse) {
	// data
	reqBody := configserverproto.ListConfigsRequest{}
	reqBody.RequestId = requestID
	reqBodyByte, _ := proto.Marshal(&reqBody)

	// request
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("POST", "/User/ListConfigs", bytes.NewBuffer(reqBodyByte))
	r.ServeHTTP(w, req)

	// response
	res := w.Result()
	resBodyByte, _ := io.ReadAll(res.Body)
	resBody := new(configserverproto.ListConfigsResponse)
	_ = proto.Unmarshal(resBodyByte, resBody)

	return res.StatusCode, resBody
}

func ApplyConfigToAgentGroup(r *gin.Engine, configName string, groupName string, requestID string) (int, *configserverproto.ApplyConfigToAgentGroupResponse) {
	// data
	reqBody := configserverproto.ApplyConfigToAgentGroupRequest{}
	reqBody.RequestId = requestID
	reqBody.ConfigName = configName
	reqBody.GroupName = groupName
	reqBodyByte, _ := proto.Marshal(&reqBody)

	// request
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("PUT", "/User/ApplyConfigToAgentGroup", bytes.NewBuffer(reqBodyByte))
	r.ServeHTTP(w, req)

	// response
	res := w.Result()
	resBodyByte, _ := io.ReadAll(res.Body)
	resBody := new(configserverproto.ApplyConfigToAgentGroupResponse)
	_ = proto.Unmarshal(resBodyByte, resBody)

	return res.StatusCode, resBody
}

func RemoveConfigFromAgentGroup(r *gin.Engine, configName string, groupName string, requestID string) (int, *configserverproto.RemoveConfigFromAgentGroupResponse) {
	// data
	reqBody := configserverproto.RemoveConfigFromAgentGroupRequest{}
	reqBody.RequestId = requestID
	reqBody.ConfigName = configName
	reqBody.GroupName = groupName
	reqBodyByte, _ := proto.Marshal(&reqBody)

	// request
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("DELETE", "/User/RemoveConfigFromAgentGroup", bytes.NewBuffer(reqBodyByte))
	r.ServeHTTP(w, req)

	// response
	res := w.Result()
	resBodyByte, _ := io.ReadAll(res.Body)
	resBody := new(configserverproto.RemoveConfigFromAgentGroupResponse)
	_ = proto.Unmarshal(resBodyByte, resBody)

	return res.StatusCode, resBody
}

func GetAppliedConfigsForAgentGroup(r *gin.Engine, groupName string, requestID string) (int, *configserverproto.GetAppliedConfigsForAgentGroupResponse) {
	// data
	reqBody := configserverproto.GetAppliedConfigsForAgentGroupRequest{}
	reqBody.RequestId = requestID
	reqBody.GroupName = groupName
	reqBodyByte, _ := proto.Marshal(&reqBody)

	// request
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("POST", "/User/GetAppliedConfigsForAgentGroup", bytes.NewBuffer(reqBodyByte))
	r.ServeHTTP(w, req)

	// response
	res := w.Result()
	resBodyByte, _ := io.ReadAll(res.Body)
	resBody := new(configserverproto.GetAppliedConfigsForAgentGroupResponse)
	_ = proto.Unmarshal(resBodyByte, resBody)

	return res.StatusCode, resBody
}

func GetAppliedAgentGroups(r *gin.Engine, configName string, requestID string) (int, *configserverproto.GetAppliedAgentGroupsResponse) {
	// data
	reqBody := configserverproto.GetAppliedAgentGroupsRequest{}
	reqBody.RequestId = requestID
	reqBody.ConfigName = configName
	reqBodyByte, _ := proto.Marshal(&reqBody)

	// request
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("POST", "/User/GetAppliedAgentGroups", bytes.NewBuffer(reqBodyByte))
	r.ServeHTTP(w, req)

	// response
	res := w.Result()
	resBodyByte, _ := io.ReadAll(res.Body)
	resBody := new(configserverproto.GetAppliedAgentGroupsResponse)
	_ = proto.Unmarshal(resBodyByte, resBody)

	return res.StatusCode, resBody
}

func ListAgents(r *gin.Engine, groupName string, requestID string) (int, *configserverproto.ListAgentsResponse) {
	// data
	reqBody := configserverproto.ListAgentsRequest{}
	reqBody.RequestId = requestID
	reqBody.GroupName = groupName
	reqBodyByte, _ := proto.Marshal(&reqBody)

	// request
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("POST", "/User/ListAgents", bytes.NewBuffer(reqBodyByte))
	r.ServeHTTP(w, req)

	// response
	res := w.Result()
	resBodyByte, _ := io.ReadAll(res.Body)
	resBody := new(configserverproto.ListAgentsResponse)
	_ = proto.Unmarshal(resBodyByte, resBody)

	return res.StatusCode, resBody
}

func HeartBeat(r *gin.Engine, agent *configserverproto.Agent, configInfos []*configserverproto.ConfigCheckResult, requestID string) (int, *configserverproto.HeartBeatResponse) {
	// data
	reqBody := configserverproto.HeartBeatRequest{}
	reqBody.RequestId = requestID
	reqBody.AgentId = agent.AgentId
	reqBody.AgentType = agent.AgentType
	reqBody.Attributes = agent.Attributes
	reqBody.RunningStatus = agent.RunningStatus
	reqBody.StartupTime = agent.StartupTime
	reqBody.Tags = agent.Tags
	pipelineConfigs := make([]*configserverproto.ConfigInfo, 0)
	for _, c := range configInfos {
		conf := new(configserverproto.ConfigInfo)
		conf.Type = c.Type
		conf.Name = c.Name
		conf.Version = c.OldVersion
		pipelineConfigs = append(pipelineConfigs, conf)
	}
	reqBody.PipelineConfigs = pipelineConfigs
	reqBodyByte, _ := proto.Marshal(&reqBody)

	// request
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("POST", "/Agent/HeartBeat", bytes.NewBuffer(reqBodyByte))
	r.ServeHTTP(w, req)

	// response
	res := w.Result()
	resBodyByte, _ := io.ReadAll(res.Body)
	resBody := new(configserverproto.HeartBeatResponse)
	_ = proto.Unmarshal(resBodyByte, resBody)

	return res.StatusCode, resBody
}

func FetchPipelineConfig(r *gin.Engine, configInfos []*configserverproto.ConfigCheckResult, requestID string) (int, *configserverproto.FetchPipelineConfigResponse) {
	// data
	reqBody := configserverproto.FetchPipelineConfigRequest{}
	reqBody.RequestId = requestID
	reqBody.ReqConfigs = make([]*configserverproto.ConfigInfo, 0)
	for _, c := range configInfos {
		conf := new(configserverproto.ConfigInfo)
		conf.Name = c.Name
		conf.Context = c.Context
		conf.Type = c.Type
		conf.Version = c.NewVersion
		reqBody.ReqConfigs = append(reqBody.ReqConfigs, conf)
	}
	reqBodyByte, _ := proto.Marshal(&reqBody)

	// request
	w := httptest.NewRecorder()
	req, _ := http.NewRequest("POST", "/Agent/FetchPipelineConfig", bytes.NewBuffer(reqBodyByte))
	r.ServeHTTP(w, req)

	// response
	res := w.Result()
	resBodyByte, _ := io.ReadAll(res.Body)
	resBody := new(configserverproto.FetchPipelineConfigResponse)
	_ = proto.Unmarshal(resBodyByte, resBody)

	return res.StatusCode, resBody
}
