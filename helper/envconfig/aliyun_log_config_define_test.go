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
	"encoding/json"
	"fmt"
	"testing"

	"github.com/alibaba/ilogtail/helper"
	_ "github.com/alibaba/ilogtail/pkg/logger/test"

	docker "github.com/fsouza/go-dockerclient"
	"github.com/pingcap/check"
)

// go test -check.f "logConfigTestSuite.*" -args -debug=true -console=true
var _ = check.Suite(&logConfigTestSuite{})

func Test(t *testing.T) {
	check.TestingT(t)
}

func MockDockerInfoDetail(containerName string, envList []string) *helper.DockerInfoDetail {
	dockerInfo := &docker.Container{}
	dockerInfo.Name = containerName
	dockerInfo.Config = &docker.Config{}
	dockerInfo.Config.Env = envList
	return helper.CreateContainerInfoDetail(dockerInfo, *LogConfigPrefix, false)
}

type logConfigTestSuite struct {
}

func (s *logConfigTestSuite) SetUpSuite(c *check.C) {
}

func (s *logConfigTestSuite) TearDownSuite(c *check.C) {
}

func (s *logConfigTestSuite) SetUpTest(c *check.C) {

}

func (s *logConfigTestSuite) TearDownTest(c *check.C) {

}

func (s *logConfigTestSuite) TestNormal(c *check.C) {
	info := MockDockerInfoDetail("containerName", []string{
		"aliyun_logs_catalina=stdout",
	})
	c.Assert(info.EnvConfigInfoMap["catalina"], check.NotNil)
	c.Assert(len(info.EnvConfigInfoMap), check.Equals, 1)
	config := makeLogConfigSpec(info, info.EnvConfigInfoMap["catalina"])
	c.Assert(config.Project, check.Equals, *DefaultLogProject)
	c.Assert(len(config.MachineGroups), check.Equals, 0)
	c.Assert(config.Logstore, check.Equals, "catalina")
	c.Assert(config.ShardCount, check.IsNil)
	c.Assert(config.LifeCycle, check.IsNil)
	c.Assert(config.SimpleConfig, check.Equals, true)
	c.Assert(config.LogtailConfig.InputType, check.Equals, "plugin")
	c.Assert(config.LogtailConfig.ConfigName, check.Equals, "catalina")
	c.Assert(len(config.LogtailConfig.LogtailConfig), check.Equals, 1)
	c.Assert(config.LogtailConfig.LogtailConfig["plugin"], check.NotNil)
	jsonStr := "{\"global\":{\"AlwaysOnline\":true},\"inputs\":[{\"detail\":{\"IncludeEnv\":{\"aliyun_logs_catalina\":\"stdout\"},\"Stderr\":true,\"Stdout\":true},\"type\":\"service_docker_stdout\"}]}"
	configDetail, err := json.Marshal(config.LogtailConfig.LogtailConfig["plugin"])
	c.Assert(err, check.IsNil)
	c.Assert(string(configDetail), check.Equals, jsonStr)
}

func (s *logConfigTestSuite) TestK8S(c *check.C) {
	info := MockDockerInfoDetail("k8s_logtail_logtail-ds-rq95g_kube-system_32417d70-9085-11e8-851d-00163f008685_0", []string{
		"aliyun_logs_catalina=stdout",
	})
	c.Assert(info.K8SInfo.ContainerName, check.Equals, "logtail")
	c.Assert(info.K8SInfo.Namespace, check.Equals, "kube-system")
	c.Assert(info.K8SInfo.Pod, check.Equals, "logtail-ds-rq95g")
	c.Assert(info.ContainerNameTag["_container_name_"], check.Equals, "logtail")
	c.Assert(info.ContainerNameTag["_namespace_"], check.Equals, "kube-system")
	c.Assert(info.ContainerNameTag["_pod_name_"], check.Equals, "logtail-ds-rq95g")
	c.Assert(info.EnvConfigInfoMap["catalina"], check.NotNil)
	c.Assert(len(info.EnvConfigInfoMap), check.Equals, 1)
	config := makeLogConfigSpec(info, info.EnvConfigInfoMap["catalina"])
	c.Assert(config.Project, check.Equals, *DefaultLogProject)
	c.Assert(len(config.MachineGroups), check.Equals, 0)
	c.Assert(config.Logstore, check.Equals, "catalina")
	c.Assert(config.ShardCount, check.IsNil)
	c.Assert(config.LifeCycle, check.IsNil)
	c.Assert(config.SimpleConfig, check.Equals, true)
	c.Assert(config.LogtailConfig.InputType, check.Equals, "plugin")
	c.Assert(config.LogtailConfig.ConfigName, check.Equals, "catalina")
	c.Assert(len(config.LogtailConfig.LogtailConfig), check.Equals, 1)
	c.Assert(config.LogtailConfig.LogtailConfig["plugin"], check.NotNil)
	jsonStr := "{\"global\":{\"AlwaysOnline\":true},\"inputs\":[{\"detail\":{\"IncludeEnv\":{\"aliyun_logs_catalina\":\"stdout\"},\"Stderr\":true,\"Stdout\":true},\"type\":\"service_docker_stdout\"}]}"
	configDetail, err := json.Marshal(config.LogtailConfig.LogtailConfig["plugin"])
	c.Assert(err, check.IsNil)
	c.Assert(string(configDetail), check.Equals, jsonStr)
}

func (s *logConfigTestSuite) TestTags(c *check.C) {
	info := MockDockerInfoDetail("containerName", []string{
		"aliyun_logs_catalina=stdout",
		"aliyun_logs_catalina_tags=app=tomcat",
	})
	c.Assert(info.ContainerNameTag["app"], check.Equals, "tomcat")

}

func (s *logConfigTestSuite) TestMulti(c *check.C) {
	info := MockDockerInfoDetail("containerName", []string{
		"aliyun_logs_catalina=stdout",
		"aliyun_logs_filelogs=invalid-file-path",
	})
	{
		c.Assert(info.EnvConfigInfoMap["catalina"], check.NotNil)
		c.Assert(len(info.EnvConfigInfoMap), check.Equals, 2)
		config := makeLogConfigSpec(info, info.EnvConfigInfoMap["catalina"])
		c.Assert(config.Project, check.Equals, *DefaultLogProject)
		c.Assert(len(config.MachineGroups), check.Equals, 0)
		c.Assert(config.Logstore, check.Equals, "catalina")
		c.Assert(config.ShardCount, check.IsNil)
		c.Assert(config.LifeCycle, check.IsNil)
		c.Assert(config.SimpleConfig, check.Equals, true)
		c.Assert(config.LogtailConfig.InputType, check.Equals, "plugin")
		c.Assert(config.LogtailConfig.ConfigName, check.Equals, "catalina")
		c.Assert(len(config.LogtailConfig.LogtailConfig), check.Equals, 1)
		c.Assert(config.LogtailConfig.LogtailConfig["plugin"], check.NotNil)
		jsonStr := "{\"global\":{\"AlwaysOnline\":true},\"inputs\":[{\"detail\":{\"IncludeEnv\":{\"aliyun_logs_catalina\":\"stdout\"},\"Stderr\":true,\"Stdout\":true},\"type\":\"service_docker_stdout\"}]}"
		configDetail, err := json.Marshal(config.LogtailConfig.LogtailConfig["plugin"])
		c.Assert(err, check.IsNil)
		c.Assert(string(configDetail), check.Equals, jsonStr)
	}

	{
		c.Assert(info.EnvConfigInfoMap["filelogs"], check.NotNil)
		c.Assert(len(info.EnvConfigInfoMap), check.Equals, 2)
		config := makeLogConfigSpec(info, info.EnvConfigInfoMap["filelogs"])
		c.Assert(config.Project, check.Equals, *DefaultLogProject)
		c.Assert(len(config.MachineGroups), check.Equals, 0)
		c.Assert(config.Logstore, check.Equals, "filelogs")
		c.Assert(config.ShardCount, check.IsNil)
		c.Assert(config.LifeCycle, check.IsNil)
		c.Assert(config.SimpleConfig, check.Equals, true)
		c.Assert(config.LogtailConfig.InputType, check.Equals, "file")
		c.Assert(config.LogtailConfig.ConfigName, check.Equals, "filelogs")
		c.Assert(len(config.LogtailConfig.LogtailConfig), check.Equals, 5)
		c.Assert(config.LogtailConfig.LogtailConfig["logType"].(string), check.Equals, "common_reg_log")
		c.Assert(config.LogtailConfig.LogtailConfig["logPath"].(string), check.Equals, invalidLogPath)
		c.Assert(config.LogtailConfig.LogtailConfig["filePattern"].(string), check.Equals, invalidFilePattern)
		c.Assert(config.LogtailConfig.LogtailConfig["dockerFile"].(bool), check.Equals, true)
		c.Assert(config.LogtailConfig.LogtailConfig["dockerIncludeEnv"].(map[string]string)["aliyun_logs_filelogs"], check.Equals, "invalid-file-path")
	}
}

func (s *logConfigTestSuite) TestAllConfigs(c *check.C) {
	info := MockDockerInfoDetail("containerName", []string{
		"aliyun_logs_catalina=file",
		"aliyun_logs_catalina_project=my-project",
		"aliyun_logs_catalina_logstore=my-logstore",
		"aliyun_logs_catalina_shard=10",
		"aliyun_logs_catalina_ttl=3650",
		"aliyun_logs_catalina_machinegroup=my-group",
		"aliyun_logs_catalina_detail={\n  \"logType\": \"delimiter_log\",\n  \"logPath\": \"/usr/local/ilogtail\",\n  \"filePattern\": \"delimiter_log.LOG\",\n  \"separator\": \"|&|\",\n  \"key\": [\n    \"time\",\n    \"level\",\n    \"method\",\n    \"file\",\n    \"line\",\n    \"message\"\n  ],\n  \"timeKey\": \"time\",\n  \"timeFormat\": \"%Y-%m-%dT%H:%M:%S\",\n  \"dockerFile\": true,\n  \"dockerIncludeEnv\": {\n    \"ALIYUN_LOGTAIL_USER_DEFINED_ID\": \"\"\n  }\n}",
	})
	c.Assert(info.EnvConfigInfoMap["catalina"], check.NotNil)
	c.Assert(len(info.EnvConfigInfoMap), check.Equals, 1)
	config := makeLogConfigSpec(info, info.EnvConfigInfoMap["catalina"])
	c.Assert(config.Project, check.Equals, "my-project")
	c.Assert(len(config.MachineGroups), check.Equals, 1)
	c.Assert(config.MachineGroups[0], check.Equals, "my-group")
	c.Assert(config.Logstore, check.Equals, "my-logstore")
	c.Assert(*config.ShardCount, check.Equals, int32(10))
	c.Assert(*config.LifeCycle, check.Equals, int32(3650))
	c.Assert(config.SimpleConfig, check.Equals, false)
	c.Assert(config.LogtailConfig.InputType, check.Equals, "file")
	c.Assert(config.LogtailConfig.ConfigName, check.Equals, "catalina")
	c.Assert(len(config.LogtailConfig.LogtailConfig), check.Equals, 9)
	c.Assert(config.LogtailConfig.LogtailConfig["logType"], check.Equals, "delimiter_log")
}

func (s *logConfigTestSuite) TestNginxIngress(c *check.C) {
	info := MockDockerInfoDetail("containerName", []string{
		"aliyun_logs_ingress-access-abc123_product=k8s-ingress-nginx",
		"aliyun_logs_ingress-access-abc123=stdout-only",
		"aliyun_logs_ingress-error-abc123=stderr-only",
	})
	{
		c.Assert(info.EnvConfigInfoMap["ingress-access-abc123"], check.NotNil)
		c.Assert(len(info.EnvConfigInfoMap), check.Equals, 2)
		config := makeLogConfigSpec(info, info.EnvConfigInfoMap["ingress-access-abc123"])
		fmt.Println("ingress access config : ", *(info.EnvConfigInfoMap["ingress-access-abc123"]))
		c.Assert(config.Project, check.Equals, *DefaultLogProject)
		c.Assert(len(config.MachineGroups), check.Equals, 0)
		c.Assert(config.Logstore, check.Equals, "ingress-access-abc123")
		c.Assert(config.ShardCount, check.IsNil)
		c.Assert(config.LifeCycle, check.IsNil)
		c.Assert(config.SimpleConfig, check.Equals, true)
		c.Assert(config.LogtailConfig.InputType, check.Equals, "plugin")
		c.Assert(config.LogtailConfig.ConfigName, check.Equals, "ingress-access-abc123")
		c.Assert(len(config.LogtailConfig.LogtailConfig), check.Equals, 1)
		c.Assert(config.LogtailConfig.LogtailConfig["plugin"], check.NotNil)
		c.Assert(config.LogtailConfig.LogtailConfig["plugin"].(map[string]interface{})["processors"], check.NotNil)
		jsonStr := "{\"global\":{\"AlwaysOnline\":true},\"inputs\":[{\"detail\":{\"IncludeEnv\":{\"aliyun_logs_ingress-access-abc123\":\"stdout-only\"},\"Stderr\":false,\"Stdout\":true},\"type\":\"service_docker_stdout\"}],\"processors\":[{\"detail\":{\"KeepSource\":true,\"Keys\":[\"client_ip\",\"x_forward_for\",\"remote_user\",\"time\",\"method\",\"url\",\"version\",\"status\",\"body_bytes_sent\",\"http_referer\",\"http_user_agent\",\"request_length\",\"request_time\",\"proxy_upstream_name\",\"upstream_addr\",\"upstream_response_length\",\"upstream_response_time\",\"upstream_status\",\"req_id\"],\"NoKeyError\":true,\"NoMatchError\":true,\"Regex\":\"^(\\\\S+)\\\\s-\\\\s\\\\[([^]]+)]\\\\s-\\\\s(\\\\S+)\\\\s\\\\[(\\\\S+)\\\\s\\\\S+\\\\s\\\"(\\\\w+)\\\\s(\\\\S+)\\\\s([^\\\"]+)\\\"\\\\s(\\\\d+)\\\\s(\\\\d+)\\\\s\\\"([^\\\"]*)\\\"\\\\s\\\"([^\\\"]*)\\\"\\\\s(\\\\S+)\\\\s(\\\\S+)+\\\\s\\\\[([^]]*)]\\\\s(\\\\S+)\\\\s(\\\\S+)\\\\s(\\\\S+)\\\\s(\\\\S+)\\\\s(\\\\S+).*\",\"SourceKey\":\"content\"},\"type\":\"processor_regex\"}]}"
		configDetail, err := json.Marshal(config.LogtailConfig.LogtailConfig["plugin"])
		c.Assert(err, check.IsNil)
		c.Assert(string(configDetail), check.Equals, jsonStr)
	}
	{
		c.Assert(info.EnvConfigInfoMap["ingress-error-abc123"], check.NotNil)
		c.Assert(len(info.EnvConfigInfoMap), check.Equals, 2)
		config := makeLogConfigSpec(info, info.EnvConfigInfoMap["ingress-error-abc123"])
		c.Assert(config.Project, check.Equals, *DefaultLogProject)
		c.Assert(len(config.MachineGroups), check.Equals, 0)
		c.Assert(config.Logstore, check.Equals, "ingress-error-abc123")
		c.Assert(config.ShardCount, check.IsNil)
		c.Assert(config.LifeCycle, check.IsNil)
		c.Assert(config.SimpleConfig, check.Equals, true)
		c.Assert(config.LogtailConfig.InputType, check.Equals, "plugin")
		c.Assert(config.LogtailConfig.ConfigName, check.Equals, "ingress-error-abc123")
		c.Assert(len(config.LogtailConfig.LogtailConfig), check.Equals, 1)
		c.Assert(config.LogtailConfig.LogtailConfig["plugin"], check.NotNil)
		jsonStr := "{\"global\":{\"AlwaysOnline\":true},\"inputs\":[{\"detail\":{\"IncludeEnv\":{\"aliyun_logs_ingress-error-abc123\":\"stderr-only\"},\"Stderr\":true,\"Stdout\":false},\"type\":\"service_docker_stdout\"}]}"
		configDetail, err := json.Marshal(config.LogtailConfig.LogtailConfig["plugin"])
		c.Assert(err, check.IsNil)
		c.Assert(string(configDetail), check.Equals, jsonStr)
	}
}
