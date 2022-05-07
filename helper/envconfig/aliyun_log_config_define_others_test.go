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
	"github.com/pingcap/check"
)

func (s *logConfigTestSuite) TestFile(c *check.C) {
	info := MockDockerInfoDetail("containerName", []string{
		"aliyun_logs_catalina=/usr/local/tomcat/logs/catalina.*.log",
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
	c.Assert(config.LogtailConfig.InputType, check.Equals, "file")
	c.Assert(config.LogtailConfig.ConfigName, check.Equals, "catalina")
	c.Assert(len(config.LogtailConfig.LogtailConfig), check.Equals, 5)
	c.Assert(config.LogtailConfig.LogtailConfig["logType"].(string), check.Equals, "common_reg_log")
	c.Assert(config.LogtailConfig.LogtailConfig["logPath"].(string), check.Equals, "/usr/local/tomcat/logs/")
	c.Assert(config.LogtailConfig.LogtailConfig["filePattern"].(string), check.Equals, "catalina.*.log")
	c.Assert(config.LogtailConfig.LogtailConfig["dockerFile"].(bool), check.Equals, true)
	c.Assert(config.LogtailConfig.LogtailConfig["dockerIncludeEnv"].(map[string]string)["aliyun_logs_catalina"], check.Equals, "/usr/local/tomcat/logs/catalina.*.log")
}

func (s *logConfigTestSuite) TestJsonFile(c *check.C) {
	info := MockDockerInfoDetail("containerName", []string{
		"aliyun_logs_catalina=/usr/local/tomcat/logs/catalina.*.log",
		"aliyun_logs_catalina_jsonfile=true",
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
	c.Assert(config.LogtailConfig.InputType, check.Equals, "file")
	c.Assert(config.LogtailConfig.ConfigName, check.Equals, "catalina")
	c.Assert(len(config.LogtailConfig.LogtailConfig), check.Equals, 5)
	c.Assert(config.LogtailConfig.LogtailConfig["logType"].(string), check.Equals, "json_log")
	c.Assert(config.LogtailConfig.LogtailConfig["logPath"].(string), check.Equals, "/usr/local/tomcat/logs/")
	c.Assert(config.LogtailConfig.LogtailConfig["filePattern"].(string), check.Equals, "catalina.*.log")
	c.Assert(config.LogtailConfig.LogtailConfig["dockerFile"].(bool), check.Equals, true)
	c.Assert(config.LogtailConfig.LogtailConfig["dockerIncludeEnv"].(map[string]string)["aliyun_logs_catalina"], check.Equals, "/usr/local/tomcat/logs/catalina.*.log")
}
