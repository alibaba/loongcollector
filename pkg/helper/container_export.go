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

package helper

import (
	"context"
	"fmt"
	"regexp"
	"strings"
	"time"

	"github.com/docker/docker/api/types"
	"github.com/docker/docker/api/types/events"
	docker "github.com/docker/docker/client"

	"github.com/alibaba/ilogtail/pkg/logger"
)

type ContainerMeta struct {
	PodName         string
	K8sNamespace    string
	ContainerName   string
	Image           string
	K8sLabels       map[string]string
	ContainerLabels map[string]string
	Env             map[string]string
}

type DockerInfoDetailWithFilteredEnvAndLabel struct {
	Detail          *DockerInfoDetail
	Env             map[string]string
	ContainerLabels map[string]string
	K8sLabels       map[string]string
}

func GetContainersLastUpdateTime() int64 {
	return getDockerCenterInstance().getLastUpdateMapTime()
}

// GetContainerMeta get a thread safe container meta struct.
func GetContainerMeta(containerID string) *ContainerMeta {
	getFunc := func() *ContainerMeta {
		instance := getDockerCenterInstance()
		instance.lock.RLock()
		defer instance.lock.RUnlock()
		detail, ok := instance.containerMap[containerID]
		if !ok {
			return nil
		}
		c := &ContainerMeta{
			K8sLabels:       make(map[string]string),
			ContainerLabels: make(map[string]string),
			Env:             make(map[string]string),
		}
		if detail.K8SInfo.ContainerName == "" {
			c.ContainerName = detail.ContainerNameTag["_container_name_"]
		} else {
			c.ContainerName = detail.K8SInfo.ContainerName
		}
		c.K8sNamespace = detail.K8SInfo.Namespace
		c.PodName = detail.K8SInfo.Pod
		c.Image = detail.ContainerNameTag["_image_name_"]
		for k, v := range detail.K8SInfo.Labels {
			c.K8sLabels[k] = v
		}
		for k, v := range detail.ContainerInfo.Config.Labels {
			c.ContainerLabels[k] = v
		}
		for _, env := range detail.ContainerInfo.Config.Env {
			var envKey, envValue string
			splitArray := strings.SplitN(env, "=", 2)
			if len(splitArray) < 2 {
				envKey = splitArray[0]
			} else {
				envKey = splitArray[0]
				envValue = splitArray[1]
			}
			c.Env[envKey] = envValue
		}
		return c
	}
	meta := getFunc()
	if meta != nil {
		return meta
	}
	if err := containerFindingManager.FetchOne(containerID); err != nil {
		logger.Debugf(context.Background(), "cannot fetch container for %s, error is %v", containerID, err)
		return nil
	}
	return getFunc()
}

func ProcessContainerAllInfo(processor func(*DockerInfoDetail)) {
	getDockerCenterInstance().processAllContainerInfo(processor)
}

func GetContainerBySpecificInfo(filter func(*DockerInfoDetail) bool) (infoList []*DockerInfoDetail) {
	return getDockerCenterInstance().getAllSpecificInfo(filter)
}

// GetContainerByAcceptedInfo gathers all info of containers that match the input parameters.
// Two conditions (&&) for matched container:
// 1. has a label in @includeLabel and don't have any label in @excludeLabel.
// 2. has a env in @includeEnv and don't have any env in @excludeEnv.
// If the input parameters is empty, then all containers are matched.
// It returns a map contains docker container info.
func GetContainerByAcceptedInfo(
	includeLabel map[string]string,
	excludeLabel map[string]string,
	includeLabelRegex map[string]*regexp.Regexp,
	excludeLabelRegex map[string]*regexp.Regexp,
	includeEnv map[string]string,
	excludeEnv map[string]string,
	includeEnvRegex map[string]*regexp.Regexp,
	excludeEnvRegex map[string]*regexp.Regexp,
	k8sFilter *K8SFilter,
) map[string]*DockerInfoDetail {
	return getDockerCenterInstance().getAllAcceptedInfo(includeLabel, excludeLabel, includeLabelRegex, excludeLabelRegex, includeEnv, excludeEnv, includeEnvRegex, excludeEnvRegex, k8sFilter)
}

// GetContainerByAcceptedInfoV2 works like GetContainerByAcceptedInfo, but uses less CPU.
// It reduces CPU cost by using full list and match list to find containers that
// need to be check.
//
//	  deleted = fullList - containerMap
//	  newList = containerMap - fullList
//	  matchList -= deleted + filter(newList)
//	  matchAddedList: new container ID for current config
//	  matchDeletedList: deleted container ID for current config
//	  fullAddedList = newList
//	  fullDeletedList = deleted
//		 return len(deleted), len(filter(newList)), matchAddedList, matchDeletedList, fullAddedList, fullDeletedList
//
// @param fullList [in,out]: all containers.
// @param matchList [in,out]: all matched containers.
//
// It returns two integers and four list
// two integers: the number of new matched containers and deleted containers.
// four list: new matched containers list, deleted matched containers list, added containers list, delete containers list
func GetContainerByAcceptedInfoV2(
	fullList map[string]bool,
	matchList map[string]*DockerInfoDetail,
	includeLabel map[string]string,
	excludeLabel map[string]string,
	includeLabelRegex map[string]*regexp.Regexp,
	excludeLabelRegex map[string]*regexp.Regexp,
	includeEnv map[string]string,
	excludeEnv map[string]string,
	includeEnvRegex map[string]*regexp.Regexp,
	excludeEnvRegex map[string]*regexp.Regexp,
	k8sFilter *K8SFilter,
) (newCount, delCount int, matchAddedList, matchDeletedList []string) {
	return getDockerCenterInstance().getAllAcceptedInfoV2(
		fullList, matchList, includeLabel, excludeLabel, includeLabelRegex, excludeLabelRegex, includeEnv, excludeEnv, includeEnvRegex, excludeEnvRegex, k8sFilter)

}

func GetDiffContainers(fullList map[string]struct{}) (fullAddedList, fullDeletedList []string) {
	return getDockerCenterInstance().getDiffContainers(fullList)
}

// SplitRegexFromMap extract regex from user config
// regex must begin with ^ and end with $(we only check ^)
func SplitRegexFromMap(input map[string]string) (staticResult map[string]string, regexResult map[string]*regexp.Regexp, err error) {
	staticResult = make(map[string]string)
	regexResult = make(map[string]*regexp.Regexp)
	for key, value := range input {
		if strings.HasPrefix(value, "^") {
			reg, err := regexp.Compile(value)
			if err != nil {
				err = fmt.Errorf("key : %s, value : %s is not valid regex, err is %s", key, value, err.Error())
				return input, nil, err
			}
			regexResult[key] = reg
		} else {
			staticResult[key] = value
		}
	}
	return staticResult, regexResult, nil
}

func CreateDockerClient(opt ...docker.Opt) (client *docker.Client, err error) {
	opt = append(opt, docker.FromEnv)
	client, err = docker.NewClientWithOpts(opt...)
	if err != nil {
		return nil, err
	}
	// add dockerClient connectivity tests
	pingCtx, cancel := context.WithTimeout(context.Background(), time.Second*5)
	defer cancel()
	ping, err := client.Ping(pingCtx)
	if err != nil {
		return nil, err
	}
	client.NegotiateAPIVersionPing(ping)
	return
}

func RegisterDockerEventListener(c chan events.Message) {
	getDockerCenterInstance().registerEventListener(c)
}

func UnRegisterDockerEventListener(c chan events.Message) {
	getDockerCenterInstance().unRegisterEventListener(c)
}

func ContainerCenterInit() {
	getDockerCenterInstance()
}

func CreateContainerInfoDetail(info types.ContainerJSON, envConfigPrefix string, selfConfigFlag bool) *DockerInfoDetail {
	return getDockerCenterInstance().CreateInfoDetail(info, envConfigPrefix, selfConfigFlag)
}

// for test
func GetContainerMap() map[string]*DockerInfoDetail {
	instance := getDockerCenterInstance()
	instance.lock.RLock()
	defer instance.lock.RUnlock()
	return instance.containerMap
}

func GetAllContainerToRecord(envSet, labelSet, k8sLabelSet map[string]struct{}, containerIds map[string]struct{}) []*DockerInfoDetailWithFilteredEnvAndLabel {
	instance := getDockerCenterInstance()
	instance.lock.RLock()
	defer instance.lock.RUnlock()
	result := make([]*DockerInfoDetailWithFilteredEnvAndLabel, 0)
	if len(containerIds) > 0 {
		for key := range containerIds {
			value, ok := instance.containerMap[key]
			if !ok {
				continue
			}
			result = append(result, CastContainerDetail(value, envSet, labelSet, k8sLabelSet))
		}
	} else {
		for _, value := range instance.containerMap {
			result = append(result, CastContainerDetail(value, envSet, labelSet, k8sLabelSet))
		}
	}
	return result
}

func GetAllContainerIncludeEnvAndLabelToRecord(envSet, labelSet, k8sLabelSet, diffEnvSet, diffLabelSet, diffK8sLabelSet map[string]struct{}) []*DockerInfoDetailWithFilteredEnvAndLabel {
	instance := getDockerCenterInstance()
	instance.lock.RLock()
	defer instance.lock.RUnlock()
	result := make([]*DockerInfoDetailWithFilteredEnvAndLabel, 0)
	for _, value := range instance.containerMap {
		match := false
		if len(diffEnvSet) > 0 {
			for _, env := range value.ContainerInfo.Config.Env {
				splitArray := strings.SplitN(env, "=", 2)
				envKey := splitArray[0]
				if len(splitArray) != 2 {
					continue
				}
				_, ok := diffEnvSet[envKey]
				if ok {
					match = true
				}
			}
		}
		if len(diffLabelSet) > 0 {
			if !match {
				for key := range value.ContainerInfo.Config.Labels {
					_, ok := diffLabelSet[key]
					if ok {
						match = true
					}
				}
			}
		}
		if len(diffK8sLabelSet) > 0 {
			if !match {
				for key := range value.K8SInfo.Labels {
					_, ok := diffK8sLabelSet[key]
					if ok {
						match = true
					}
				}
			}
		}
		if match {
			result = append(result, CastContainerDetail(value, envSet, labelSet, k8sLabelSet))
		}
	}
	return result
}

func CastContainerDetail(containerInfo *DockerInfoDetail, envSet, labelSet, k8sLabelSet map[string]struct{}) *DockerInfoDetailWithFilteredEnvAndLabel {
	newEnv := make(map[string]string)
	for _, env := range containerInfo.ContainerInfo.Config.Env {
		splitArray := strings.SplitN(env, "=", 2)
		envKey := splitArray[0]
		if len(splitArray) != 2 {
			continue
		}
		envValue := splitArray[1]
		_, ok := envSet[envKey]
		if ok {
			newEnv[envKey] = envValue
		}
	}
	newLabels := make(map[string]string)
	for key, value := range containerInfo.ContainerInfo.Config.Labels {
		_, ok := labelSet[key]
		if ok {
			newLabels[key] = value
		}
	}
	newK8sLabels := make(map[string]string)
	for key, value := range containerInfo.K8SInfo.Labels {
		_, ok := k8sLabelSet[key]
		if ok {
			newK8sLabels[key] = value
		}
	}
	return &DockerInfoDetailWithFilteredEnvAndLabel{
		Detail:          containerInfo,
		Env:             newEnv,
		ContainerLabels: newLabels,
		K8sLabels:       newK8sLabels,
	}
}
