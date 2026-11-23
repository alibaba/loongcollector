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
	"testing"
	"time"

	"github.com/moby/moby/api/types/container"
	"github.com/stretchr/testify/suite"

	"github.com/alibaba/ilogtail/pkg/flags"
	"github.com/alibaba/ilogtail/pkg/helper/containercenter"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/aggregator/baseagg"
	"github.com/alibaba/ilogtail/plugins/flusher/checker"
	_ "github.com/alibaba/ilogtail/plugins/flusher/sls"
	_ "github.com/alibaba/ilogtail/plugins/input/docker/stdout"
)

func TestContainerConfig(t *testing.T) {
	suite.Run(t, new(containerConfigTestSuite))
}

func (s *containerConfigTestSuite) TestRefreshEnvAndLabel() {
	s.NoError(loadMockConfig(), "got err when logad config")
	refreshEnvAndLabel()
	s.Equal(1, len(LogtailConfig))
	s.Equal(1, len(envSet))
	s.Equal(1, len(containerLabelSet))
}

func (s *containerConfigTestSuite) TestCompareEnvAndLabel() {
	envSet = make(map[string]struct{})
	containerLabelSet = make(map[string]struct{})
	k8sLabelSet = make(map[string]struct{})

	s.NoError(loadMockConfig(), "got err when logad config")

	envSet["testEnv1"] = struct{}{}
	containerLabelSet["testLabel1"] = struct{}{}
	k8sLabelSet["testK8sLabel1"] = struct{}{}

	diffEnvSet, diffLabelSet, diffK8sLabelSet := compareEnvAndLabel()
	s.Equal(1, len(diffEnvSet))
	s.Equal(1, len(diffLabelSet))
	s.Equal(2, len(envSet))
	s.Equal(2, len(containerLabelSet))
	s.Equal(0, len(diffK8sLabelSet))
}

func (s *containerConfigTestSuite) TestCompareEnvAndLabelAndRecordContainer() {
	envSet = make(map[string]struct{})
	containerLabelSet = make(map[string]struct{})
	k8sLabelSet = make(map[string]struct{})

	s.NoError(loadMockConfig(), "got err when logad config")

	envSet["testEnv1"] = struct{}{}
	containerLabelSet["testLabel1"] = struct{}{}

	envList := []string{0: "test=111"}
	info := mockDockerInfoDetail("testConfig", envList)
	cMap := containercenter.GetContainerMap()
	cMap["test"] = info

	containers := compareEnvAndLabelAndRecordContainer()
	s.Equal(1, len(containers))
}

func (s *containerConfigTestSuite) TestRecordContainers() {
	info := mockDockerInfoDetail("testConfig", []string{0: "test=111"})
	cMap := containercenter.GetContainerMap()
	cMap["test"] = info

	containerIDs := make(map[string]struct{})
	containerIDs["test"] = struct{}{}
	recordedIds, _ := getContainersToRecord(containerIDs)
	s.Equal(1, len(recordedIds))
}

type containerConfigTestSuite struct {
	suite.Suite
}

func (s *containerConfigTestSuite) BeforeTest(suiteName, testName string) {
	logger.Infof(context.Background(), "========== %s %s test start ========================", suiteName, testName)
}

func (s *containerConfigTestSuite) AfterTest(suiteName, testName string) {
	logger.Infof(context.Background(), "========== %s %s test end =======================", suiteName, testName)

}

func mockDockerInfoDetail(containerName string, envList []string) *containercenter.DockerInfoDetail {
	dockerInfo := container.InspectResponse{
		Name:    containerName,
		ID:      "test",
		LogPath: "/var/lib/docker/containers/test/test-json.log",
		HostConfig: &container.HostConfig{
			LogConfig: container.LogConfig{
				Type: "json-file",
			},
		},
	}
	dockerInfo.Config = &container.Config{}
	dockerInfo.Config.Env = envList
	return containercenter.CreateContainerInfoDetail(dockerInfo, *flags.LogConfigPrefix, false)
}

// project, logstore, config, configJsonStr
func loadMockConfig() error {
	project := "test_prj"
	logstore := "test_logstore"
	configName := "test_config"

	configStr := `
	{
		"inputs": [{
			"detail": {
				"Stderr": true,
				"CollectContainersFlag": true,
				"IncludeLabel": {
					"app": "^.*$"
				},
				"IncludeEnv": {
					"test": "^.*$"
				},
				"Stdout": true
			},
			"type": "service_docker_stdout"
		}],
		"global": {
			"AlwaysOnline": true,
			"type": "dockerStdout"
		}
	}`
	err := LoadLogstoreConfig(project, logstore, configName, 666, configStr)
	if err != nil {
		return err
	}
	LogtailConfig[configName] = ToStartPipelineConfigWithInput
	return nil
}

func (s *containerConfigTestSuite) TestStopCollectsPendingConfigResults() {
	configStr := `
	{
		"global": {
			"InputIntervalMs": 3600000,
			"InputMaxFirstCollectDelayMs": 0,
			"AggregatIntervalMs": 1000,
			"FlushIntervalMs": 1000,
			"DefaultLogQueueSize": 4,
			"DefaultLogGroupQueueSize": 4,
			"Tags" : {
				"base_version" : "0.1.0",
				"loongcollector_version" : "0.16.19"
			}
		},
		"inputs" : [
			{
				"type" : "metric_container",
				"detail" : null
			}
		],
		"flushers": [
			{
				"type": "flusher_checker"
			}
		]
	}`
	containerConfig, err := loadBuiltinConfig("container", "sls-test", "logtail_containers", "logtail_containers", configStr)
	s.Require().NoError(err)
	flusher := GetConfigFlushers(containerConfig.PluginRunner)[0].(*checker.FlusherChecker)
	containercenter.RecordContainerConfigResultIncrement(&containercenter.ContainerConfigResult{ConfigName: "initial-read"})
	containerConfig.Start()
	s.Require().Eventually(func() bool {
		flusher.Lock.RLock()
		defer flusher.Lock.RUnlock()
		for _, group := range flusher.GroupEvents {
			for _, event := range group.Events {
				if log, ok := event.(*models.Log); ok && log.GetIndices().Get("config_name") == "initial-read" {
					return true
				}
			}
		}
		return false
	}, 5*time.Second, time.Millisecond)

	const size = 10000
	for i := 0; i < size; i++ {
		containercenter.RecordContainerConfigResultIncrement(&containercenter.ContainerConfigResult{
			DataType: "collect_config_result", Project: "sls-test", Logstore: "container-store",
			ConfigName: "region$application", InputType: "container", SourceAddress: "source", FlusherType: "sls",
		})
	}
	containercenter.RecordContainerConfigResultIncrement(&containercenter.ContainerConfigResult{ConfigName: "final-read"})
	// The next scheduled collection is an hour away. Stop runs the last Read.
	s.Require().NoError(containerConfig.Stop(true))
	var first *models.Log
	var configResults, finalMarkers int
	for _, group := range flusher.GroupEvents {
		s.Require().LessOrEqual(len(group.Events), baseagg.MaxLogCount)
		for _, event := range group.Events {
			log := event.(*models.Log)
			switch log.GetIndices().Get("config_name") {
			case "final-read":
				finalMarkers++
			case "application":
				configResults++
				if first == nil {
					first = log
				}
			}
		}
	}
	s.Require().Equal(1, finalMarkers)
	s.Require().Equal(size, configResults)
	s.Require().NotNil(first)
	s.Equal("collect_config_result", first.GetIndices().Get("type"))
	s.Equal("sls-test", first.GetIndices().Get("project"))
	s.Equal("container-store", first.GetIndices().Get("logstore"))
	s.Equal("application", first.GetIndices().Get("config_name"))
	s.Equal("source", first.GetIndices().Get("input.source_addresses"))
	s.Equal("sls", first.GetIndices().Get("flusher.type"))
	s.Equal(uint64(0), first.GetTimestamp()%1e9)
	s.NotZero(first.GetTimestamp())
	s.Empty(first.GetTags().Iterator())
}
