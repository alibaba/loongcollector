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

	"github.com/alibaba/ilogtail/pkg/logger"

	docker "github.com/fsouza/go-dockerclient"
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
//  need to be check.
//
//   deleted = fullList - containerMap
//   newList = containerMap - fullList
//   matchList -= deleted + filter(newList)
//	 return len(deleted), len(filter(newList))
//
// @param fullList [in,out]: all containers.
// @param matchList [in,out]: all matched containers.
//
// It returns two integers: the number of new matched containers
//  and deleted containers.
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
) (int, int) {
	return getDockerCenterInstance().getAllAcceptedInfoV2(
		fullList, matchList, includeLabel, excludeLabel, includeLabelRegex, excludeLabelRegex, includeEnv, excludeEnv, includeEnvRegex, excludeEnvRegex, k8sFilter)

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

func CreateDockerClient() (client *docker.Client, err error) {
	client, err = docker.NewClientFromEnv()
	return
}

func RegisterDockerEventListener(c chan *docker.APIEvents) {
	getDockerCenterInstance().registerEventListener(c)
}

func UnRegisterDockerEventListener(c chan *docker.APIEvents) {
	getDockerCenterInstance().unRegisterEventListener(c)
}

func ContainerCenterInit() {
	getDockerCenterInstance()
}

func CreateContainerInfoDetail(info *docker.Container, envConfigPrefix string, selfConfigFlag bool) *DockerInfoDetail {
	return getDockerCenterInstance().CreateInfoDetail(info, envConfigPrefix, selfConfigFlag)
}
