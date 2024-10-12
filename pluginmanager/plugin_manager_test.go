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

//go:build linux || windows
// +build linux windows

package pluginmanager

import (
	"context"
	"os"
	"testing"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	_ "github.com/alibaba/ilogtail/pkg/logger/test"

	// dependency packages
	_ "github.com/alibaba/ilogtail/plugins/aggregator"
	"github.com/alibaba/ilogtail/plugins/flusher/checker"
	_ "github.com/alibaba/ilogtail/plugins/flusher/sls"
	_ "github.com/alibaba/ilogtail/plugins/flusher/statistics"
	_ "github.com/alibaba/ilogtail/plugins/flusher/stdout"
	_ "github.com/alibaba/ilogtail/plugins/input/canal"
	_ "github.com/alibaba/ilogtail/plugins/input/http"
	_ "github.com/alibaba/ilogtail/plugins/input/mockd"
	_ "github.com/alibaba/ilogtail/plugins/processor/anchor"
	_ "github.com/alibaba/ilogtail/plugins/processor/regex"

	"github.com/stretchr/testify/suite"
)

func TestPluginManager(t *testing.T) {
	suite.Run(t, new(managerTestSuite))
}

type managerTestSuite struct {
	suite.Suite
}

func (s *managerTestSuite) BeforeTest(suiteName, testName string) {
	logger.Infof(context.Background(), "========== %s %s test start ========================", suiteName, testName)
	s.NoError(Init(), "got error when init")
}

func (s *managerTestSuite) AfterTest(suiteName, testName string) {
	logger.Infof(context.Background(), "========== %s %s test end =======================", suiteName, testName)

}

/*
func (s *managerTestSuite) TestResumeHoldOn() {
	for i := 0; i < 10; i++ {
		s.NoError(LoadAndStartMockConfig(), "got err when logad config")
		s.NoError(Resume(), "got err when resume")
		time.Sleep(time.Millisecond * time.Duration(10))
		s.NoError(HoldOn(false), "got err when hold on")
	}
}
*/

func (s *managerTestSuite) TestPluginManager() {
	s.NoError(LoadAndStartMockConfig(), "got err when logad config")
	time.Sleep(time.Millisecond * time.Duration(10))
	s.NoError(Stop("test_config", false), "got err when hold on")
	for i := 0; i < 5; i++ {
		s.NoError(LoadAndStartMockConfig(), "got err when logad config")
		time.Sleep(time.Millisecond * time.Duration(1500))
		config, ok := LogtailConfig["test_config"]
		s.True(ok)
		s.Equal(2, len(GetConfigFlushers(config.PluginRunner)))
		c, ok := GetConfigFlushers(config.PluginRunner)[1].(*checker.FlusherChecker)
		s.True(ok)
		s.NoError(Stop("test_config", false), "got err when hold on")
		s.Equal(200, c.GetLogCount())
	}
}

func GetTestConfig(configName string) string {
	fileName := "./test_config/" + configName + ".json"
	byteStr, err := os.ReadFile(fileName)
	if err != nil {
		logger.Warning(context.Background(), "read", fileName, "error", err)
	}
	return string(byteStr)
}

// project, logstore, config, configJsonStr
func LoadAndStartMockConfig(args ...string) error {
	project := "test_prj"
	if len(args) > 0 {
		project = args[0]
	}
	logstore := "test_logstore"
	if len(args) > 1 {
		logstore = args[1]
	}
	configName := "test_config"
	if len(args) > 2 {
		configName = args[2]
	}

	configStr := `
	{
		"inputs": [
			{
				"type": "service_mock",
				"detail": {
					"LogsPerSecond": 100,
					"Fields": {
						"content": "Active connections: 1\nserver accepts handled requests\n 6079 6079 11596\n Reading: 0 Writing: 1 Waiting: 0"
					}
				}
			}
		],
		"processors": [
			{
				"type": "processor_regex",
				"detail": {
					"SourceKey": "content",
					"Regex": "Active connections: (\\d+)\\s+server accepts handled requests\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+Reading: (\\d+) Writing: (\\d+) Waiting: (\\d+).*",
					"Keys": [
						"connection",
						"accepts",
						"handled",
						"requests",
						"reading",
						"writing",
						"waiting"
					],
					"FullMatch": true,
					"NoKeyError": true,
					"NoMatchError": true,
					"KeepSource": true
				}
			}
		],
		"aggregators": [
			{
				"type": "aggregator_default"
			}
		],
		"flushers": [
			{
				"type": "flusher_statistics",
				"detail": {
					"GeneratePB": true
				}
			},
			{
				"type": "flusher_checker"
			}
		]
	}`

	if len(args) > 3 {
		configStr = args[3]
	}

	err := LoadLogstoreConfig(project, logstore, configName, 666, configStr)
	if err != nil {
		return err
	}
	return Start(configName)
}
