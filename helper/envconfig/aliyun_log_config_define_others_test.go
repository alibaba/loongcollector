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

//go:build !windows
// +build !windows

package envconfig

import (
	"github.com/alibabacloud-go/tea/tea"
	"github.com/pingcap/check"

	"github.com/alibaba/ilogtail/pkg/flags"
)

func (s *logConfigTestSuite) TestFile(c *check.C) {
	info := MockDockerInfoDetail("containerName", []string{
		"aliyun_logs_catalina=/usr/local/tomcat/logs/catalina.*.log",
	})
	c.Assert(info.EnvConfigInfoMap["catalina"], check.NotNil)
	c.Assert(len(info.EnvConfigInfoMap), check.Equals, 1)
	config := makeLogConfigSpec(info, info.EnvConfigInfoMap["catalina"])
	c.Assert(config.Project, check.Equals, *flags.DefaultLogProject)
	c.Assert(len(config.MachineGroups), check.Equals, 0)
	c.Assert(config.Logstore, check.Equals, "catalina")
	c.Assert(config.ShardCount, check.IsNil)
	c.Assert(config.LifeCycle, check.IsNil)
	c.Assert(config.SimpleConfig, check.Equals, true)
	c.Assert(tea.StringValue(config.LogtailConfig.ConfigName), check.Equals, "catalina")
	c.Assert(len(config.LogtailConfig.Inputs[0]), check.Equals, 5)
	c.Assert(config.LogtailConfig.Inputs[0]["Type"], check.Equals, "input_file")
	c.Assert(config.LogtailConfig.Inputs[0]["FilePaths"].([]string)[0], check.Equals, "/usr/local/tomcat/logs/catalina.*.log")
	c.Assert(config.LogtailConfig.Inputs[0]["EnableContainerDiscovery"].(bool), check.Equals, true)
	c.Assert(config.LogtailConfig.Inputs[0]["ContainerFilters"].(map[string]map[string]interface{})["IncludeEnv"]["aliyun_logs_catalina"], check.Equals, "/usr/local/tomcat/logs/catalina.*.log")
	c.Assert(config.LogtailConfig.Inputs[0]["MaxDirSearchDepth"].(int), check.Equals, 0)

	info = MockDockerInfoDetail("containerName", []string{
		"aliyun_logs_catalina=/usr/local/tomcat/logs/**/catalina.*.log",
	})
	c.Assert(info.EnvConfigInfoMap["catalina"], check.NotNil)
	c.Assert(len(info.EnvConfigInfoMap), check.Equals, 1)
	config = makeLogConfigSpec(info, info.EnvConfigInfoMap["catalina"])
	c.Assert(config.Project, check.Equals, *flags.DefaultLogProject)
	c.Assert(len(config.MachineGroups), check.Equals, 0)
	c.Assert(config.Logstore, check.Equals, "catalina")
	c.Assert(config.ShardCount, check.IsNil)
	c.Assert(config.LifeCycle, check.IsNil)
	c.Assert(config.SimpleConfig, check.Equals, true)
	c.Assert(tea.StringValue(config.LogtailConfig.ConfigName), check.Equals, "catalina")
	c.Assert(len(config.LogtailConfig.Inputs[0]), check.Equals, 5)
	c.Assert(config.LogtailConfig.Inputs[0]["Type"], check.Equals, "input_file")
	c.Assert(config.LogtailConfig.Inputs[0]["FilePaths"].([]string)[0], check.Equals, "/usr/local/tomcat/logs/**/catalina.*.log")
	c.Assert(config.LogtailConfig.Inputs[0]["EnableContainerDiscovery"].(bool), check.Equals, true)
	c.Assert(config.LogtailConfig.Inputs[0]["ContainerFilters"].(map[string]map[string]interface{})["IncludeEnv"]["aliyun_logs_catalina"], check.Equals, "/usr/local/tomcat/logs/**/catalina.*.log")
	c.Assert(config.LogtailConfig.Inputs[0]["MaxDirSearchDepth"].(int), check.Equals, 10)
}

func (s *logConfigTestSuite) TestJsonFile(c *check.C) {
	info := MockDockerInfoDetail("containerName", []string{
		"aliyun_logs_catalina=/usr/local/tomcat/logs/catalina.*.log",
		"aliyun_logs_catalina_jsonfile=true",
	})
	c.Assert(info.EnvConfigInfoMap["catalina"], check.NotNil)
	c.Assert(len(info.EnvConfigInfoMap), check.Equals, 1)
	config := makeLogConfigSpec(info, info.EnvConfigInfoMap["catalina"])
	c.Assert(config.Project, check.Equals, *flags.DefaultLogProject)
	c.Assert(len(config.MachineGroups), check.Equals, 0)
	c.Assert(config.Logstore, check.Equals, "catalina")
	c.Assert(config.ShardCount, check.IsNil)
	c.Assert(config.LifeCycle, check.IsNil)
	c.Assert(config.SimpleConfig, check.Equals, true)
	c.Assert(tea.StringValue(config.LogtailConfig.ConfigName), check.Equals, "catalina")
	c.Assert(len(config.LogtailConfig.Inputs[0]), check.Equals, 5)
	c.Assert(config.LogtailConfig.Inputs[0]["Type"], check.Equals, "input_file")
	c.Assert(config.LogtailConfig.Inputs[0]["FilePaths"].([]string)[0], check.Equals, "/usr/local/tomcat/logs/catalina.*.log")
	c.Assert(config.LogtailConfig.Inputs[0]["EnableContainerDiscovery"].(bool), check.Equals, true)
	c.Assert(config.LogtailConfig.Inputs[0]["ContainerFilters"].(map[string]map[string]interface{})["IncludeEnv"]["aliyun_logs_catalina"], check.Equals, "/usr/local/tomcat/logs/catalina.*.log")
	c.Assert(config.LogtailConfig.Inputs[0]["MaxDirSearchDepth"].(int), check.Equals, 0)

	c.Assert(len(config.LogtailConfig.Processors[0]), check.Equals, 2)
	c.Assert(config.LogtailConfig.Processors[0]["SourceKey"], check.Equals, "content")
	c.Assert(config.LogtailConfig.Processors[0]["Type"].(string), check.Equals, "processor_parse_json_native")
}
