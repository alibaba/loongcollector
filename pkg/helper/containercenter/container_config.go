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

package containercenter

import (
	"encoding/json"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/util"
)

const ContainerIDPrefixSize = 12

var addedContainerConfigResultMutex sync.Mutex

// 新增的采集配置结果
var AddedContainerConfigResult []*ContainerConfigResult

// 采集配置结果内存存储
var AddedContainerConfigResultMap map[string]*ContainerConfigResult

type ContainerDetail struct {
	DataType         string
	Project          string
	ContainerID      string
	ContainerIP      string
	ContainerName    string
	RawContainerName string
	LogPath          string
	Driver           string
	Namespace        string
	ImageName        string
	PodName          string
	RootPath         string
	Hostname         string
	HostsPath        string
	Env              map[string]string
	ContainerLabels  map[string]string
	K8sLabels        map[string]string
}

type ContainerConfigResult struct {
	DataType                      string
	Project                       string
	Logstore                      string
	ConfigName                    string
	PathNotExistInputContainerIDs string
	PathExistInputContainerIDs    string
	SourceAddress                 string
	InputType                     string
	InputIsContainerFile          string
	FlusherType                   string
	FlusherTargetAddress          string
}

func InitContainer() {
	addedContainerConfigResultMutex.Lock()
	AddedContainerConfigResult = make([]*ContainerConfigResult, 0)
	AddedContainerConfigResultMap = make(map[string]*ContainerConfigResult)
	addedContainerConfigResultMutex.Unlock()
}

// 将内存Map中的数据转化到list中，用于输出
func RecordContainerConfigResult() {
	addedContainerConfigResultMutex.Lock()
	defer addedContainerConfigResultMutex.Unlock()
	for _, value := range AddedContainerConfigResultMap {
		AddedContainerConfigResult = append(AddedContainerConfigResult, value)
	}
	AddedContainerConfigResultMap = make(map[string]*ContainerConfigResult)
}

// 内存中记录每个采集配置的结果，用于RecordContainerConfigResult的时候全量输出一遍
func RecordContainerConfigResultMap(message *ContainerConfigResult) {
	addedContainerConfigResultMutex.Lock()
	defer addedContainerConfigResultMutex.Unlock()
	AddedContainerConfigResultMap[message.ConfigName] = message
}

// 增量记录采集配置结果
func RecordContainerConfigResultIncrement(message *ContainerConfigResult) {
	addedContainerConfigResultMutex.Lock()
	defer addedContainerConfigResultMutex.Unlock()
	AddedContainerConfigResult = append(AddedContainerConfigResult, message)
}

// NewDeletedContainersEvent describes containers removed from the tracked set.
func NewDeletedContainersEvent(project string, containerIDs string) *models.Log {
	nowTime := time.Now()
	log := models.NewSimpleLog(nil, models.NewTags(), uint64(nowTime.Truncate(time.Second).UnixNano()))
	log.Contents.Add("type", "delete_containers")
	log.Contents.Add("project", project)
	log.Contents.Add("container_ids", containerIDs)
	log.Contents.Add("ip", util.GetIPAddress())

	return log
}

// AppendContainerEvents adds metadata events for the supplied containers.
func AppendContainerEvents(events []models.PipelineEvent, addedContainers []*ContainerDetail) []models.PipelineEvent {
	nowTime := time.Now()
	for _, item := range addedContainers {
		log := models.NewSimpleLog(nil, models.NewTags(), uint64(nowTime.Truncate(time.Second).UnixNano()))
		log.Contents.Add("type", item.DataType)
		log.Contents.Add("project", item.Project)
		log.Contents.Add("container_id", GetShortID(item.ContainerID))
		log.Contents.Add("container_ip", item.ContainerIP)
		log.Contents.Add("container_name", item.ContainerName)
		log.Contents.Add("raw_container_name", item.RawContainerName)
		log.Contents.Add("log_path", item.LogPath)
		log.Contents.Add("driver", item.Driver)
		log.Contents.Add("namespace", item.Namespace)
		log.Contents.Add("image_name", item.ImageName)
		log.Contents.Add("pod_name", item.PodName)
		log.Contents.Add("root_path", item.RootPath)
		log.Contents.Add("hostname", item.Hostname)
		log.Contents.Add("hosts_path", item.HostsPath)

		envStr, err := json.Marshal(item.Env)
		if err == nil {
			log.Contents.Add("env", string(envStr))
		}
		labelsStr, err := json.Marshal(item.ContainerLabels)
		if err == nil {
			log.Contents.Add("labels", string(labelsStr))
		}
		k8sLabelsStr, err := json.Marshal(item.K8sLabels)
		if err == nil {
			log.Contents.Add("k8s_labels", string(k8sLabelsStr))
		}

		log.Contents.Add("ip", util.GetIPAddress())

		events = append(events, log)
	}
	return events
}

// AppendContainerConfigResultEvents drains pending container configuration results.
func AppendContainerConfigResultEvents(events []models.PipelineEvent) []models.PipelineEvent {
	nowTime := time.Now()
	addedContainerConfigResultMutex.Lock()
	defer addedContainerConfigResultMutex.Unlock()
	for _, item := range AddedContainerConfigResult {
		log := models.NewSimpleLog(nil, models.NewTags(), uint64(nowTime.Truncate(time.Second).UnixNano()))
		log.Contents.Add("type", item.DataType)
		log.Contents.Add("project", item.Project)
		log.Contents.Add("logstore", item.Logstore)
		configName := item.ConfigName
		splitArrs := strings.Split(configName, "$")
		if len(splitArrs) == 2 {
			configName = splitArrs[1]
		}
		log.Contents.Add("config_name", configName)
		log.Contents.Add("input.source_addresses", item.SourceAddress)
		log.Contents.Add("input.path_exist_container_ids", item.PathExistInputContainerIDs)
		log.Contents.Add("input.path_not_exist_container_ids", item.PathNotExistInputContainerIDs)
		log.Contents.Add("input.type", item.InputType)
		log.Contents.Add("input.container_file", item.InputIsContainerFile)
		log.Contents.Add("flusher.type", item.FlusherType)
		log.Contents.Add("flusher.target_addresses", item.FlusherTargetAddress)

		log.Contents.Add("ip", util.GetIPAddress())
		events = append(events, log)
	}
	clear(AddedContainerConfigResult)
	AddedContainerConfigResult = AddedContainerConfigResult[:0]
	return events
}

func GetShortID(fullID string) string {
	if len(fullID) < ContainerIDPrefixSize {
		return fullID
	}
	return fullID[0:12]
}

func GetStringFromList(list []string) string {
	return strings.Join(list, ";")
}
