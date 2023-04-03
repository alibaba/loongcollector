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

package pluginmanager

import (
	"context"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

// 24h
var FetchAllInterval = time.Second * time.Duration(24*60*60)

// 30min
var FirstFetchAllInterval = time.Second * time.Duration(30*60)

var timerFetchRunning = false

var envAndLabelMutex sync.Mutex

var envSet map[string]struct{}
var containerLabelSet map[string]struct{}
var k8sLabelSet map[string]struct{}

func timerRecordData() {
	recordContainers(make(map[string]struct{}))
	// record all container config result at same time
	util.RecordConfigResult()
}

// 记录增量的容器
func recordAddedContainers() {
	containerIDs := util.GetAddedContainerIDs()
	if len(containerIDs) > 0 {
		recordContainers(containerIDs)
	}
}

func recordContainers(containerIDs map[string]struct{}) {
	projectSet := make(map[string]struct{})
	for _, logstoreConfig := range LogtailConfig {
		projectSet[logstoreConfig.ProjectName] = struct{}{}
	}
	keys := make([]string, 0, len(projectSet))
	for k := range projectSet {
		if len(k) > 0 {
			keys = append(keys, k)
		}
	}
	projectStr := util.GetStringFromList(keys)
	// get add container
	envAndLabelMutex.Lock()
	result := helper.GetAllContainerToRecord(envSet, containerLabelSet, k8sLabelSet, containerIDs)
	envAndLabelMutex.Unlock()
	for _, containerInfo := range result {
		var containerDetailToRecord util.ContainerDetail
		containerDetailToRecord.Project = projectStr
		containerDetailToRecord.DataType = "all_container_info"
		containerDetailToRecord.ContainerID = containerInfo.Detail.ContainerInfo.ID
		containerDetailToRecord.ContainerIP = containerInfo.Detail.ContainerIP
		containerDetailToRecord.ContainerName = containerInfo.Detail.K8SInfo.ContainerName
		containerDetailToRecord.Driver = containerInfo.Detail.ContainerInfo.Driver
		containerDetailToRecord.HostsPath = containerInfo.Detail.ContainerInfo.HostsPath
		containerDetailToRecord.Hostname = containerInfo.Detail.ContainerInfo.Config.Hostname
		containerDetailToRecord.ImageName = containerInfo.Detail.ContainerInfo.Config.Image
		containerDetailToRecord.Namespace = containerInfo.Detail.K8SInfo.Namespace
		containerDetailToRecord.PodName = containerInfo.Detail.K8SInfo.Pod
		containerDetailToRecord.LogPath = containerInfo.Detail.ContainerInfo.LogPath
		containerDetailToRecord.RootPath = containerInfo.Detail.DefaultRootPath
		containerDetailToRecord.RawContainerName = containerInfo.Detail.ContainerInfo.Name

		containerDetailToRecord.Env = containerInfo.Env
		containerDetailToRecord.ContainerLabels = containerInfo.ContainerLabels
		containerDetailToRecord.K8sLabels = containerInfo.K8sLabels
		util.RecordAddedContainer(&containerDetailToRecord)
	}
}

func CollectContainers(logGroup *protocol.LogGroup) {
	recordAddedContainers()
	util.SerializeContainerToPb(logGroup)
}

func CollectDeleteContainers(logGroup *protocol.LogGroup) {
	containerIDs := util.GetDeletedContainerIDs()
	logger.Debugf(context.Background(), "GetDeletedContainerIDs", containerIDs)
	if len(containerIDs) > 0 {
		projectSet := make(map[string]struct{})

		// get project list
		for _, logstoreConfig := range LogtailConfig {
			projectSet[logstoreConfig.ProjectName] = struct{}{}
		}
		keys := make([]string, 0, len(projectSet))
		for k := range projectSet {
			if len(k) > 0 {
				keys = append(keys, k)
			}
		}
		projectStr := util.GetStringFromList(keys)

		ids := make([]string, 0, len(containerIDs))
		for id := range containerIDs {
			if len(id) > 0 {
				ids = append(ids, id)
			}
		}
		util.SerializeDeleteContainerToPb(logGroup, projectStr, util.GetStringFromList(ids))
	}
}

func CollectConfigResult(logGroup *protocol.LogGroup) {
	util.SerializeConfigResultToPb(logGroup)
}

func refreshEnvAndLabel() {
	envAndLabelMutex.Lock()
	defer envAndLabelMutex.Unlock()
	envSet = make(map[string]struct{})
	containerLabelSet = make(map[string]struct{})
	k8sLabelSet = make(map[string]struct{})
	for _, logstoreConfig := range LogtailConfig {
		if logstoreConfig.CollectContainersFlag {
			for key := range logstoreConfig.EnvSet {
				envSet[key] = struct{}{}
			}
			for key := range logstoreConfig.ContainerLabelSet {
				containerLabelSet[key] = struct{}{}
			}
			for key := range logstoreConfig.K8sLabelSet {
				k8sLabelSet[key] = struct{}{}
			}
		}
	}
	logger.Debugf(context.Background(), "refreshEnvAndLabel", envSet, containerLabelSet, k8sLabelSet)
}

func compareEnvAndLabel() (diffEnvSet, diffContainerLabelSet, diffK8sLabelSet map[string]struct{}) {
	// get newest env label and compare with old
	diffEnvSet = make(map[string]struct{})
	diffContainerLabelSet = make(map[string]struct{})
	diffK8sLabelSet = make(map[string]struct{})
	for _, logstoreConfig := range LogtailConfig {
		if logstoreConfig.CollectContainersFlag {
			for key := range logstoreConfig.EnvSet {
				if _, ok := envSet[key]; !ok {
					envSet[key] = struct{}{}
					diffEnvSet[key] = struct{}{}
				}
			}
			for key := range logstoreConfig.ContainerLabelSet {
				if _, ok := containerLabelSet[key]; !ok {
					containerLabelSet[key] = struct{}{}
					diffContainerLabelSet[key] = struct{}{}
				}
			}
			for key := range logstoreConfig.K8sLabelSet {
				if _, ok := k8sLabelSet[key]; !ok {
					k8sLabelSet[key] = struct{}{}
					diffK8sLabelSet[key] = struct{}{}
				}
			}
		}
	}
	return diffEnvSet, diffContainerLabelSet, diffK8sLabelSet
}

func compareEnvAndLabelAndRecordContainer() {
	envAndLabelMutex.Lock()
	defer envAndLabelMutex.Unlock()

	diffEnvSet, diffContainerLabelSet, diffK8sLabelSet := compareEnvAndLabel()
	logger.Debugf(context.Background(), "compareEnvAndLabel", diffEnvSet, diffContainerLabelSet, diffK8sLabelSet)

	if len(diffEnvSet) != 0 || len(diffContainerLabelSet) != 0 || len(diffK8sLabelSet) != 0 {
		projectSet := make(map[string]struct{})
		for _, logstoreConfig := range LogtailConfig {
			projectSet[logstoreConfig.ProjectName] = struct{}{}
		}
		keys := make([]string, 0, len(projectSet))
		for k := range projectSet {
			if len(k) > 0 {
				keys = append(keys, k)
			}
		}
		projectStr := util.GetStringFromList(keys)
		result := helper.GetAllContainerIncludeEnvAndLabelToRecord(envSet, containerLabelSet, k8sLabelSet, diffEnvSet, diffContainerLabelSet, diffK8sLabelSet)
		logger.Debugf(context.Background(), "GetAllContainerIncludeEnvAndLabelToRecord", result)

		for _, containerInfo := range result {
			var containerDetailToRecord util.ContainerDetail
			containerDetailToRecord.Project = projectStr
			containerDetailToRecord.DataType = "all_container_info"
			containerDetailToRecord.ContainerID = containerInfo.Detail.ContainerInfo.ID
			containerDetailToRecord.ContainerIP = containerInfo.Detail.ContainerIP
			containerDetailToRecord.ContainerName = containerInfo.Detail.K8SInfo.ContainerName
			containerDetailToRecord.Driver = containerInfo.Detail.ContainerInfo.Driver
			containerDetailToRecord.HostsPath = containerInfo.Detail.ContainerInfo.HostsPath
			containerDetailToRecord.Hostname = containerInfo.Detail.ContainerInfo.Config.Hostname
			containerDetailToRecord.ImageName = containerInfo.Detail.ContainerInfo.Config.Image
			containerDetailToRecord.Namespace = containerInfo.Detail.K8SInfo.Namespace
			containerDetailToRecord.PodName = containerInfo.Detail.K8SInfo.Pod
			containerDetailToRecord.LogPath = containerInfo.Detail.ContainerInfo.LogPath
			containerDetailToRecord.RootPath = containerInfo.Detail.DefaultRootPath
			containerDetailToRecord.RawContainerName = containerInfo.Detail.ContainerInfo.Name

			containerDetailToRecord.Env = containerInfo.Env
			containerDetailToRecord.ContainerLabels = containerInfo.ContainerLabels
			containerDetailToRecord.K8sLabels = containerInfo.K8sLabels
			util.RecordAddedContainer(&containerDetailToRecord)
		}
	}
}

func isCollectContainers() bool {
	for _, logstoreConfig := range LogtailConfig {
		if logstoreConfig.CollectContainersFlag {
			return true
		}
	}
	return false
}

func TimerFetchFuction() {
	timerFetch := func() {
		lastFetchAllTime := time.Now()
		flagFirst := true
		refreshEnvAndLabel()
		for {
			logger.Debugf(context.Background(), "timerFetchFuction", time.Since(lastFetchAllTime))
			time.Sleep(time.Duration(10) * time.Second)
			if isCollectContainers() {
				fetchInterval := FetchAllInterval
				if flagFirst {
					// 第一次时间跟后面的周期性时间不同
					fetchInterval = FirstFetchAllInterval
				}
				if time.Since(lastFetchAllTime) >= fetchInterval {
					logger.Info(context.Background(), "timerFetchFuction running", time.Since(lastFetchAllTime))
					refreshEnvAndLabel()
					timerRecordData()
					lastFetchAllTime = time.Now()
					if flagFirst {
						flagFirst = false
					}
				} else {
					compareEnvAndLabelAndRecordContainer()
				}
			}
		}
	}
	if !timerFetchRunning {
		go timerFetch()
		timerFetchRunning = true
	}
}
