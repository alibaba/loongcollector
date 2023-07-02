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

package helper

import (
	"encoding/json"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

const ContainerIDPrefixSize = 12

var addedContainerMutex sync.Mutex

var addedContainerConfigResultMutex sync.Mutex

var addedContainerMapMutex sync.Mutex

var deletedContainerMutex sync.Mutex

// 新增的容器
var AddedContainers []*ContainerDetail

// 新增的采集配置结果
var AddedContainerConfigResult []*ContainerConfigResult

// 采集配置结果内存存储
var AddedContainerConfigResultMap map[string]*ContainerConfigResult

// 容器信息内存存储
var AddedContainerMap map[string]struct{}

// 删除的容器信息
var DeletedContainerMap map[string]struct{}

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
	addedContainerMutex.Lock()
	AddedContainers = make([]*ContainerDetail, 0)
	addedContainerMutex.Unlock()

	addedContainerConfigResultMutex.Lock()
	AddedContainerConfigResult = make([]*ContainerConfigResult, 0)
	AddedContainerConfigResultMap = make(map[string]*ContainerConfigResult)
	addedContainerConfigResultMutex.Unlock()

	addedContainerMapMutex.Lock()
	AddedContainerMap = make(map[string]struct{})
	addedContainerMapMutex.Unlock()

	deletedContainerMutex.Lock()
	DeletedContainerMap = make(map[string]struct{})
	deletedContainerMutex.Unlock()
}

// 记录容器信息
func RecordAddedContainer(message *ContainerDetail) {
	addedContainerMutex.Lock()
	AddedContainers = append(AddedContainers, message)
	addedContainerMutex.Unlock()
}

// 将内存Map中的数据转化到list中，用于输出
func RecordContainerConfigResult() {
	addedContainerConfigResultMutex.Lock()
	for _, value := range AddedContainerConfigResultMap {
		AddedContainerConfigResult = append(AddedContainerConfigResult, value)
	}
	AddedContainerConfigResultMap = make(map[string]*ContainerConfigResult)
	addedContainerConfigResultMutex.Unlock()
}

// 内存中记录每个采集配置的结果，用于RecordContainerConfigResult的时候全量输出一遍
func RecordContainerConfigResultMap(message *ContainerConfigResult) {
	addedContainerConfigResultMutex.Lock()
	AddedContainerConfigResultMap[message.ConfigName] = message
	addedContainerConfigResultMutex.Unlock()
}

// 增量记录采集配置结果
func RecordContainerConfigResultIncrement(message *ContainerConfigResult) {
	addedContainerConfigResultMutex.Lock()
	AddedContainerConfigResult = append(AddedContainerConfigResult, message)
	addedContainerConfigResultMutex.Unlock()
}

// 记录新增容器ID
func RecordAddedContainerIDs(containerID string) {
	addedContainerMapMutex.Lock()
	defer addedContainerMapMutex.Unlock()
	AddedContainerMap[containerID] = struct{}{}
}

// 获取新增容器ID列表
func GetAddedContainerIDs() map[string]struct{} {
	addedContainerMapMutex.Lock()
	defer addedContainerMapMutex.Unlock()
	result := make(map[string]struct{})
	for key := range AddedContainerMap {
		result[key] = struct{}{}
	}
	AddedContainerMap = make(map[string]struct{})
	return result
}

// 记录删除容器ID
func RecordDeletedContainerIDs(containerID string) {
	deletedContainerMutex.Lock()
	defer deletedContainerMutex.Unlock()
	DeletedContainerMap[containerID] = struct{}{}
}

// 获取删除容器ID列表
func GetDeletedContainerIDs() map[string]struct{} {
	deletedContainerMutex.Lock()
	defer deletedContainerMutex.Unlock()
	result := make(map[string]struct{})
	for key := range DeletedContainerMap {
		if len(key) > 0 {
			result[key] = struct{}{}
		}
	}
	DeletedContainerMap = make(map[string]struct{})
	return result
}

func SerializeDeleteContainerToPb(logGroup *protocol.LogGroup, project string, containerIDsStr string) {
	nowTime := time.Now()
	deletedContainerMutex.Lock()
	log := &protocol.Log{}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "type", Value: "delete_containers"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "project", Value: project})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "container_ids", Value: containerIDsStr})

	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "ip", Value: util.GetIPAddress()})
	log.Time = (uint32)(nowTime.Unix())
	if config.LogtailGlobalConfig.EnableTimestampNanosecond {
		log.TimeNs = (uint32)(nowTime.Nanosecond())
	}
	logGroup.Logs = append(logGroup.Logs, log)
	deletedContainerMutex.Unlock()
}

func SerializeContainerToPb(logGroup *protocol.LogGroup) {
	nowTime := time.Now()
	addedContainerMutex.Lock()
	for _, item := range AddedContainers {
		log := &protocol.Log{}
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "type", Value: item.DataType})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "project", Value: item.Project})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "container_id", Value: GetShortID(item.ContainerID)})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "container_ip", Value: item.ContainerIP})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "container_name", Value: item.ContainerName})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "raw_container_name", Value: item.RawContainerName})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "log_path", Value: item.LogPath})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "driver", Value: item.Driver})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "namespace", Value: item.Namespace})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "image_name", Value: item.ImageName})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "pod_name", Value: item.PodName})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "root_path", Value: item.RootPath})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "hostname", Value: item.Hostname})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "hosts_path", Value: item.HostsPath})

		envStr, err := json.Marshal(item.Env)
		if err == nil {
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "env", Value: string(envStr)})
		}
		labelsStr, err := json.Marshal(item.ContainerLabels)
		if err == nil {
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "labels", Value: string(labelsStr)})
		}
		k8sLabelsStr, err := json.Marshal(item.K8sLabels)
		if err == nil {
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "k8s_labels", Value: string(k8sLabelsStr)})
		}

		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "ip", Value: util.GetIPAddress()})
		log.Time = (uint32)(nowTime.Unix())
		if config.LogtailGlobalConfig.EnableTimestampNanosecond {
			log.TimeNs = (uint32)(nowTime.Nanosecond())
		}
		logGroup.Logs = append(logGroup.Logs, log)
	}
	AddedContainers = AddedContainers[:0]
	addedContainerMutex.Unlock()
}

func SerializeContainerConfigResultToPb(logGroup *protocol.LogGroup) {
	nowTime := time.Now()
	addedContainerConfigResultMutex.Lock()
	for _, item := range AddedContainerConfigResult {
		log := &protocol.Log{}
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "type", Value: item.DataType})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "project", Value: item.Project})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "logstore", Value: item.Logstore})
		configName := item.ConfigName
		splitArrs := strings.Split(configName, "$")
		if len(splitArrs) == 2 {
			configName = splitArrs[1]
		}
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "config_name", Value: configName})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "input.source_addresses", Value: item.SourceAddress})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "input.path_exist_container_ids", Value: item.PathExistInputContainerIDs})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "input.path_not_exist_container_ids", Value: item.PathNotExistInputContainerIDs})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "input.type", Value: item.InputType})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "input.container_file", Value: item.InputIsContainerFile})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "flusher.type", Value: item.FlusherType})
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "flusher.target_addresses", Value: item.FlusherTargetAddress})

		log.Time = (uint32)(nowTime.Unix())
		if config.LogtailGlobalConfig.EnableTimestampNanosecond {
			log.TimeNs = (uint32)(nowTime.Nanosecond())
		}
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "ip", Value: util.GetIPAddress()})
		logGroup.Logs = append(logGroup.Logs, log)
	}
	AddedContainerConfigResult = AddedContainerConfigResult[:0]
	addedContainerConfigResultMutex.Unlock()
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
