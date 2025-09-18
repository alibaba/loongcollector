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

	"github.com/alibaba/ilogtail/pkg/protocol"
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

func SerializeDeleteContainerToPb(logGroup *protocol.LogGroup, project string, containerIDsStr string) {
	nowTime := time.Now()
	log := &protocol.Log{}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "type", Value: "delete_containers"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "project", Value: project})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "container_ids", Value: containerIDsStr})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "ip", Value: util.GetIPAddress()})
	protocol.SetLogTime(log, uint32(nowTime.Unix()))
	logGroup.Logs = append(logGroup.Logs, log)
}

func SerializeContainerToPb(logGroup *protocol.LogGroup, addedContainers []*ContainerDetail) {
	nowTime := time.Now()
	for _, item := range addedContainers {
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
		protocol.SetLogTime(log, uint32(nowTime.Unix()))
		logGroup.Logs = append(logGroup.Logs, log)
	}
}

func SerializeContainerConfigResultToPb(logGroup *protocol.LogGroup) {
	nowTime := time.Now()
	addedContainerConfigResultMutex.Lock()
	defer addedContainerConfigResultMutex.Unlock()
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
		protocol.SetLogTime(log, uint32(nowTime.Unix()))
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "ip", Value: util.GetIPAddress()})
		logGroup.Logs = append(logGroup.Logs, log)
	}
	AddedContainerConfigResult = AddedContainerConfigResult[:0]
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
