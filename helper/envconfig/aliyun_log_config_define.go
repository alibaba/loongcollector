// Copyright 2021 iLogtail Authors
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

package envconfig

import (
	"context"
	"crypto/sha256"
	"encoding/json"
	"strconv"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/logger"

	"github.com/gogo/protobuf/proto"
)

// K8S Name : k8s_logtail_logtail-ds-rq95g_kube-system_32417d70-9085-11e8-851d-00163f008685_0
// # 存储标识：catalina
// - name: aliyun_logs_catalina
// # stdout（约定关键字）：表示采集容器标准输出日志
//   value: "/usr/local/tomcat/logs/catalina.*.log"
// # 日志标记
// - name: aliyun_logs_catalina_tags
//   value: "app=tomcat"
// ###############   扩展字段  ##################
// # project名，没有设置指向默认的project
// - name: aliyun_logs_catalina_project
//   value: "my-project"
// # logstore名，如果这个设置就存储到特定的logstore，否则存储到catalina这个logstore
// - name: aliyun_logs_catalina_logstore
//   value: "my-logstore"
// # shard 数
// - name: aliyun_logs_catalina_shard
//   value: "10"
// # 存储周期（天）
// - name: aliyun_logs_catalina_ttl
//   value: "3650"
// # 是否为JSON类型的日志文件
// - name: aliyun_logs_catalina_jsonfile
//   value: "true"
// # 是否为产品类型的配置
// - name: aliyun_logs_catalina_product
//   value: "k8s-audit"
// # 该产品类型的语言，默认为cn
// - name: aliyun_logs_catalina_language
//   value: "cn"
// # 机器组（不填使用默认的机器组）
// - name: aliyun_logs_catalina_machinegroup
//   value: "machinegroup1"
// # 详细配置信息，是一个json
// - name: aliyun_logs_catalina_detail
//   value: "{\n  \"logType\": \"delimiter_log\",\n  \"logPath\": \"/usr/local/ilogtail\",\n  \"filePattern\": \"delimiter_log.LOG\",\n  \"separator\": \"|&|\",\n  \"key\": [\n    \"time\",\n    \"level\",\n    \"method\",\n    \"file\",\n    \"line\",\n    \"message\"\n  ],\n  \"timeKey\": \"time\",\n  \"timeFormat\": \"%Y-%m-%dT%H:%M:%S\",\n  \"dockerFile\": true,\n  \"dockerIncludeEnv\": {\n    \"ALIYUN_LOGTAIL_USER_DEFINED_ID\": \"\"\n  }\n}"
// 原来的配置为
// {
// 	"logType": "delimiter_log",
// 	"logPath": "/usr/local/ilogtail",
// 	"filePattern": "delimiter_log.LOG",
// 	"separator": "|&|",
// 	"key": [
// 	  "time",
// 	  "level",
// 	  "method",
// 	  "file",
// 	  "line",
// 	  "message"
// 	],
// 	"timeKey": "time",
// 	"timeFormat": "%Y-%m-%dT%H:%M:%S",
// 	"dockerFile": true,
// 	"dockerIncludeEnv": {
// 	  "ALIYUN_LOGTAIL_USER_DEFINED_ID": ""
// 	}
// }

// AliyunLogConfigSpec logtail config struct for wrapper
type AliyunLogConfigSpec struct {
	Project       string                `json:"project"`
	Logstore      string                `json:"logstore"`
	ShardCount    *int32                `json:"shardCount"`
	LifeCycle     *int32                `json:"lifeCycle"`
	MachineGroups []string              `json:"machineGroups"`
	LogtailConfig AliyunLogConfigDetail `json:"logtailConfig"`
	ContainerName string                `json:"containerName"`
	HashCode      []byte                `json:"hashCode"`
	ErrorCount    int                   `json:"errorCount"`
	NextTryTime   int64                 `json:"nextTryTime"`
	SimpleConfig  bool                  `json:"simpleConfig"`
	LastFetchTime int64                 `json:"lastFetchTime"`
	ProductCode   string                `json:"productCode"`
	ProductLang   string                `json:"productLang"`
}

// AliyunLogConfigDetail logtail config detail
type AliyunLogConfigDetail struct {
	ConfigName    string                 `json:"configName"`
	InputType     string                 `json:"inputType"`
	LogtailConfig map[string]interface{} `json:"inputDetail"`
}

// Hash generate sha256 hash
func (alcs *AliyunLogConfigSpec) hash(hashValue string) {
	h := sha256.New()
	_, _ = h.Write([]byte(hashValue))
	alcs.HashCode = h.Sum(nil)
}

// Key return the unique key
func (alcs *AliyunLogConfigSpec) Key() string {
	return alcs.Project + "@" + alcs.LogtailConfig.ConfigName + "@" + alcs.ContainerName
}

// InitFromDockerInfo init AliyunLogConfigDetail from docker info with specific config name
func (alcs *AliyunLogConfigDetail) InitFromDockerInfo(dockerInfo *helper.DockerInfoDetail,
	config string,
	configKeys []string,
	configValues []string) error {

	return nil
}

// all log config spec, must been cleared before next process
var logConfigSpecList = []*AliyunLogConfigSpec{}
var dockerEnvConfigInfoList = []*helper.DockerInfoDetail{}

// dockerInfoEnvConfigProcessFunc process all docker info, get all config spec list
func dockerInfoEnvConfigProcessFunc(dockerInfo *helper.DockerInfoDetail) {
	if len(dockerInfo.EnvConfigInfoMap) > 0 {
		dockerEnvConfigInfoList = append(dockerEnvConfigInfoList, dockerInfo)
	}
}

func isValidEnvConfig(envConfigInfo *helper.EnvConfigInfo) bool {
	if _, ok := envConfigInfo.ConfigItemMap[""]; !ok {
		return false
	}
	return true
}

func isDockerStdout(configType string) bool {
	return configType == "stdout" || configType == "stderr-only" || configType == "stdout-only"
}

// nolint: unparam
func initDockerStdoutConfig(dockerInfo *helper.DockerInfoDetail, config *AliyunLogConfigSpec, configType string) {
	config.LogtailConfig.InputType = "plugin"
	stdoutDetail := make(map[string]interface{})
	switch configType {
	case "stdout":
		stdoutDetail["Stdout"] = true
		stdoutDetail["Stderr"] = true
	case "stderr-only":
		stdoutDetail["Stdout"] = false
		stdoutDetail["Stderr"] = true
	case "stdout-only":
		stdoutDetail["Stdout"] = true
		stdoutDetail["Stderr"] = false
	}

	//  remove container name limit, this will not work when pod with a sidecar/init container which has same env
	// if len(dockerInfo.K8SInfo.Namespace) > 0 {
	// 	stdoutDetail["IncludeLabel"] = map[string]string{
	// 		"io.kubernetes.container.name": dockerInfo.K8SInfo.ContainerName,
	// 	}
	// }
	stdoutDetail["IncludeEnv"] = map[string]string{
		*LogConfigPrefix + config.LogtailConfig.ConfigName: configType,
	}
	stdoutConfig := map[string]interface{}{
		"type":   "service_docker_stdout",
		"detail": stdoutDetail,
	}
	config.LogtailConfig.LogtailConfig = map[string]interface{}{
		"plugin": map[string]interface{}{
			"inputs": []interface{}{stdoutConfig},
			"global": map[string]interface{}{
				"AlwaysOnline": true,
			},
		},
	}
}

const invalidFilePattern = "invalid_file_pattern"

// nolint: unparam
func initFileConfig(k8sInfo *helper.K8SInfo, config *AliyunLogConfigSpec, filePath string, jsonFlag, dockerFile bool) {
	config.LogtailConfig.InputType = "file"
	logPath, filePattern, err := splitLogPathAndFilePattern(filePath)
	if err != nil {
		logger.Error(context.Background(), "INVALID_DOCKER_ENV_CONFIG_ALARM", "invalid file config, you must input full file path", filePath, err)
	}

	if !jsonFlag {
		config.LogtailConfig.LogtailConfig = map[string]interface{}{
			"logType":     "common_reg_log",
			"logPath":     logPath,
			"filePattern": filePattern,
			"dockerFile":  dockerFile,
			"dockerIncludeEnv": map[string]string{
				*LogConfigPrefix + config.LogtailConfig.ConfigName: filePath,
			},
		}
	} else {
		config.LogtailConfig.LogtailConfig = map[string]interface{}{
			"logType":     "json_log",
			"logPath":     logPath,
			"filePattern": filePattern,
			"dockerFile":  dockerFile,
			"dockerIncludeEnv": map[string]string{
				*LogConfigPrefix + config.LogtailConfig.ConfigName: filePath,
			},
		}
	}

	//  remove container name limit, this will not work when pod with a sidecar/init container which has same env
	// if len(k8sInfo.Namespace) > 0 {
	// 	config.LogtailConfig.LogtailConfig["dockerIncludeLabel"] = map[string]string{
	// 		"io.kubernetes.container.name": k8sInfo.ContainerName,
	// 	}
	// }
}

func isJSONFile(envConfigInfo *helper.EnvConfigInfo, config *AliyunLogConfigSpec) bool {
	if jsonfile, ok := envConfigInfo.ConfigItemMap["jsonfile"]; ok && (jsonfile == "true" || jsonfile == "TRUE") {
		return true
	}
	logstore := config.Logstore
	// @note hardcode for k8s audit, eg audit-cfc281c9c4ca548638a1aaa765d8f220d
	if strings.HasPrefix(logstore, "audit-") && len(logstore) == 39 {
		return true
	}
	return false
}

func isDockerFile(envConfigInfo *helper.EnvConfigInfo) bool {
	// only when set false explicitly
	if dockerfile, ok := envConfigInfo.ConfigItemMap["dockerfile"]; ok && (dockerfile == "false" || dockerfile == "FALSE") {
		return false
	}
	return true
}

func makeLogConfigSpec(dockerInfo *helper.DockerInfoDetail, envConfigInfo *helper.EnvConfigInfo) *AliyunLogConfigSpec {
	config := &AliyunLogConfigSpec{}
	config.LastFetchTime = time.Now().Unix()
	// save all config info, and keep sequence
	var totalConfig string
	config.LogtailConfig.ConfigName = envConfigInfo.ConfigName
	// @note add image name to prevent key conflict
	if len(dockerInfo.K8SInfo.ContainerName) > 0 {
		config.ContainerName = dockerInfo.K8SInfo.ContainerName
	} else {
		config.ContainerName = dockerInfo.ContainerInfo.Name
	}
	if val, ok := envConfigInfo.ConfigItemMap["project"]; ok {
		config.Project = val
	} else {
		config.Project = *DefaultLogProject
	}
	totalConfig += config.Project

	if val, ok := envConfigInfo.ConfigItemMap["logstore"]; ok {
		config.Logstore = val
	} else {
		config.Logstore = envConfigInfo.ConfigName
	}
	totalConfig += config.Logstore

	if val, ok := envConfigInfo.ConfigItemMap["product"]; ok {
		config.ProductCode = val
		totalConfig += val
		if lang, ok := envConfigInfo.ConfigItemMap["language"]; ok {
			config.ProductLang = lang
			totalConfig += lang
		} else {
			config.ProductLang = "cn"
		}
	}

	if val, ok := envConfigInfo.ConfigItemMap["machinegroup"]; ok {
		groups := strings.Split(val, ",")
		totalConfig += val
		config.MachineGroups = append(config.MachineGroups, groups...)
	}

	if val, ok := envConfigInfo.ConfigItemMap["shard"]; ok {
		totalConfig += val
		shardCount, _ := strconv.Atoi(val)
		if shardCount <= 0 {
			shardCount = 2
		}
		if shardCount > 10 {
			shardCount = 10
		}
		config.ShardCount = proto.Int32((int32)(shardCount))
	}

	if val, ok := envConfigInfo.ConfigItemMap["ttl"]; ok {
		totalConfig += val
		ttl, _ := strconv.Atoi(val)
		if ttl <= 0 {
			ttl = 90
		}
		if ttl > 3650 {
			ttl = 3650
		}
		config.LifeCycle = proto.Int32((int32)(ttl))
	}

	// config
	// makesure exist
	configType := envConfigInfo.ConfigItemMap[""]
	config.LogtailConfig.LogtailConfig = make(map[string]interface{})
	if configDetail, ok := envConfigInfo.ConfigItemMap["detail"]; ok {
		totalConfig += configDetail
		if err := json.Unmarshal([]byte(configDetail), &config.LogtailConfig.LogtailConfig); err != nil {
			logger.Error(context.Background(), "INVALID_ENV_CONFIG_DETAIL", "unmarshal error", err, "detail", configDetail)
		}
		config.LogtailConfig.InputType = configType
		config.SimpleConfig = false
	} else {
		totalConfig += configType
		config.SimpleConfig = true
		logger.Debug(context.Background(), "container", dockerInfo.ContainerInfo.Name, "docker k8s info", dockerInfo.K8SInfo)
		// stdout or filepath
		if isDockerStdout(configType) {
			initDockerStdoutConfig(dockerInfo, config, configType)
		} else {
			jsonFlag := isJSONFile(envConfigInfo, config)
			dockerFileFlag := isDockerFile(envConfigInfo)
			initFileConfig(dockerInfo.K8SInfo, config, configType, jsonFlag, dockerFileFlag)
		}

		if len(config.ProductCode) > 0 {
			logger.Debug(context.Background(), "init product", config.ProductCode, "project", config.Project, "logstore", config.Logstore)
			initLogtailConfigForProduct(dockerInfo.K8SInfo, config, configType)
		}
	}
	config.hash(totalConfig)
	return config
}

func fetchAllEnvConfig() {
	dockerEnvConfigInfoList = make([]*helper.DockerInfoDetail, 0, len(dockerEnvConfigInfoList))
	helper.ProcessContainerAllInfo(dockerInfoEnvConfigProcessFunc)
	logConfigSpecList = make([]*AliyunLogConfigSpec, 0, len(dockerEnvConfigInfoList))
	for _, info := range dockerEnvConfigInfoList {
		for _, envConfigInfo := range info.EnvConfigInfoMap {
			if !isValidEnvConfig(envConfigInfo) {
				continue
			}
			logConfigSpecList = append(logConfigSpecList, makeLogConfigSpec(info, envConfigInfo))
		}
	}
	// add self env config
	if selfEnvConfig != nil {
		for _, envConfigInfo := range selfEnvConfig.EnvConfigInfoMap {
			logger.Debug(context.Background(), "load self envConfigInfo", *envConfigInfo)
			if !isValidEnvConfig(envConfigInfo) {
				continue
			}
			logConfigSpecList = append(logConfigSpecList, makeLogConfigSpec(selfEnvConfig, envConfigInfo))
		}
	}
}
