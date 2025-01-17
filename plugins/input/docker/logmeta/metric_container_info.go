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

package logmeta

import (
	"encoding/json"
	"fmt"
	"os"
	"path"
	"reflect"
	"regexp"
	"sort"
	"strings"
	"time"

	"github.com/docker/docker/api/types"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/logtail"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
)

const (
	PluginDockerUpdateFile    = 1
	PluginDockerDeleteFile    = 2
	PluginDockerUpdateFileAll = 3
	PluginDockerStopFile      = 4
)

type Mount struct {
	Source      string
	Destination string
}

type DockerFileUpdateCmd struct {
	ID        string
	Tags      []string // 用户自定义Tag
	MetaDatas []string // 容器信息
	Mounts    []Mount  // 容器挂载路径
	UpperDir  string   // 容器默认路径
	LogPath   string   // 标准输出路径
}

type ContainerInfoCache struct {
	Mounts   []types.MountPoint
	UpperDir string
	LogPath  string
}

type DockerFileUpdateCmdAll struct {
	AllCmd []DockerFileUpdateCmd
}

type InputDockerFile struct {
	IncludeLabel          map[string]string // Deprecated： use IncludeContainerLabel and IncludeK8sLabel instead.
	ExcludeLabel          map[string]string // Deprecated： use ExcludeContainerLabel and ExcludeK8sLabel instead.
	IncludeEnv            map[string]string
	ExcludeEnv            map[string]string
	IncludeContainerLabel map[string]string
	ExcludeContainerLabel map[string]string
	IncludeK8sLabel       map[string]string
	ExcludeK8sLabel       map[string]string
	ExternalEnvTag        map[string]string
	ExternalK8sLabelTag   map[string]string
	LogPath               string
	FilePattern           string
	MountPath             string
	HostFlag              bool
	K8sNamespaceRegex     string
	K8sPodRegex           string
	K8sContainerRegex     string

	// export from ilogtail-trace component
	IncludeLabelRegex map[string]*regexp.Regexp
	ExcludeLabelRegex map[string]*regexp.Regexp
	IncludeEnvRegex   map[string]*regexp.Regexp
	ExcludeEnvRegex   map[string]*regexp.Regexp
	K8sFilter         *helper.K8SFilter

	lastContainerInfoCache map[string]ContainerInfoCache

	FlushIntervalMs   int `comment:"the interval of container discovery, and the timeunit is millisecond. Default value is 3000."`
	context           pipeline.Context
	lastClearTime     time.Time
	updateEmptyFlag   bool
	avgInstanceMetric pipeline.CounterMetric
	addMetric         pipeline.CounterMetric
	updateMetric      pipeline.CounterMetric
	deleteMetric      pipeline.CounterMetric
	lastUpdateTime    int64

	// Last return of GetAllAcceptedInfoV2
	fullList                 map[string]bool
	matchList                map[string]*helper.DockerInfoDetail
	CollectingContainersMeta bool
	firstStart               bool
}

func formatPath(path string) string {
	if len(path) == 0 {
		return path
	}
	if path[len(path)-1] == '/' {
		return path[0 : len(path)-1]
	}
	if path[len(path)-1] == '\\' {
		return path[0 : len(path)-1]
	}
	return path
}

func (idf *InputDockerFile) Name() string {
	return "InputDockerFile"
}

func (idf *InputDockerFile) Init(context pipeline.Context) (int, error) {
	idf.context = context

	idf.lastContainerInfoCache = make(map[string]ContainerInfoCache)

	idf.firstStart = true
	idf.fullList = make(map[string]bool)
	idf.matchList = make(map[string]*helper.DockerInfoDetail)
	// Because docker on Windows will convert all mounted path to lowercase (see
	// Mounts field in output of docker inspect), so we have to change LogPath to
	// lowercase if it is a Windows path (with colon).
	idf.LogPath = formatPath(idf.LogPath)
	if colonPos := strings.Index(idf.LogPath, ":"); colonPos != -1 {
		idf.LogPath = strings.ToLower(idf.LogPath)
	}
	idf.lastClearTime = time.Now()
	helper.ContainerCenterInit()

	if idf.HostFlag {
		idf.MountPath = ""
	} else if envPath := os.Getenv("ALIYUN_LOGTAIL_MOUNT_PATH"); len(envPath) > 0 {
		if envPath[len(envPath)-1] == '/' || envPath[len(envPath)-1] == '\\' {
			envPath = envPath[0 : len(envPath)-1]
		}
		idf.MountPath = envPath
	} else {
		idf.MountPath = helper.DefaultLogtailMountPath
	}
	idf.updateEmptyFlag = true

	metricsRecord := idf.context.GetMetricRecord()
	idf.avgInstanceMetric = helper.NewAverageMetricAndRegister(metricsRecord, helper.MetricPluginContainerTotal)
	idf.addMetric = helper.NewCounterMetricAndRegister(metricsRecord, helper.MetricPluginAddContainerTotal)
	idf.deleteMetric = helper.NewCounterMetricAndRegister(metricsRecord, helper.MetricPluginRemoveContainerTotal)
	idf.updateMetric = helper.NewCounterMetricAndRegister(metricsRecord, helper.MetricPluginUpdateContainerTotal)

	var err error
	idf.IncludeEnv, idf.IncludeEnvRegex, err = helper.SplitRegexFromMap(idf.IncludeEnv)
	if err != nil {
		logger.Warning(idf.context.GetRuntimeContext(), "INVALID_REGEX_ALARM", "init include env regex error", err)
	}
	idf.ExcludeEnv, idf.ExcludeEnvRegex, err = helper.SplitRegexFromMap(idf.ExcludeEnv)
	if err != nil {
		logger.Warning(idf.context.GetRuntimeContext(), "INVALID_REGEX_ALARM", "init exclude env regex error", err)
	}
	if idf.IncludeLabel != nil {
		for k, v := range idf.IncludeContainerLabel {
			idf.IncludeLabel[k] = v
		}
	} else {
		idf.IncludeLabel = idf.IncludeContainerLabel
	}
	if idf.ExcludeLabel != nil {
		for k, v := range idf.ExcludeContainerLabel {
			idf.ExcludeLabel[k] = v
		}
	} else {
		idf.ExcludeLabel = idf.ExcludeContainerLabel
	}
	idf.IncludeLabel, idf.IncludeLabelRegex, err = helper.SplitRegexFromMap(idf.IncludeLabel)
	if err != nil {
		logger.Warning(idf.context.GetRuntimeContext(), "INVALID_REGEX_ALARM", "init include label regex error", err)
	}
	idf.ExcludeLabel, idf.ExcludeLabelRegex, err = helper.SplitRegexFromMap(idf.ExcludeLabel)
	if err != nil {
		logger.Warning(idf.context.GetRuntimeContext(), "INVALID_REGEX_ALARM", "init exclude label regex error", err)
	}
	idf.K8sFilter, err = helper.CreateK8SFilter(idf.K8sNamespaceRegex, idf.K8sPodRegex, idf.K8sContainerRegex, idf.IncludeK8sLabel, idf.ExcludeK8sLabel)

	logger.Debugf(idf.context.GetRuntimeContext(), "InputDockerFile inited successfully")
	return idf.FlushIntervalMs, err
}

func (idf *InputDockerFile) Description() string {
	return "docker file plugin for logtail"
}

// addMappingToLogtail  添加容器信息到allCmd里面，allCmd不为nil时，只添加不执行，allCmd为nil时，添加并执行
func (idf *InputDockerFile) addMappingToLogtail(info *helper.DockerInfoDetail, containerInfo ContainerInfoCache, allCmd *DockerFileUpdateCmdAll) {
	var cmd DockerFileUpdateCmd
	cmd.ID = info.ContainerInfo.ID
	cmd.UpperDir = path.Clean(containerInfo.UpperDir)
	cmd.LogPath = path.Clean(containerInfo.LogPath)
	// tags
	tags := info.GetExternalTags(idf.ExternalEnvTag, idf.ExternalK8sLabelTag)
	cmd.Tags = make([]string, 0, len(tags)*2)
	for key, val := range tags {
		cmd.Tags = append(cmd.Tags, key)
		cmd.Tags = append(cmd.Tags, val)
	}
	// info.ContainerNameTag
	cmd.MetaDatas = make([]string, 0, len(info.ContainerNameTag)*2)
	for key, val := range info.ContainerNameTag {
		cmd.MetaDatas = append(cmd.MetaDatas, key)
		cmd.MetaDatas = append(cmd.MetaDatas, val)
	}
	cmd.Mounts = make([]Mount, 0, len(containerInfo.Mounts))
	for _, mount := range containerInfo.Mounts {
		cmd.Mounts = append(cmd.Mounts, Mount{
			Source:      path.Clean(mount.Source),
			Destination: path.Clean(mount.Destination),
		})
	}
	cmdBuf, _ := json.Marshal(&cmd)
	configName := idf.context.GetConfigName()
	logger.Info(idf.context.GetRuntimeContext(), "addMappingToLogtail cmd", cmd)
	if allCmd != nil {
		allCmd.AllCmd = append(allCmd.AllCmd, cmd)
		return
	}
	if err := logtail.ExecuteCMD(configName, PluginDockerUpdateFile, cmdBuf); err != nil {
		logger.Error(idf.context.GetRuntimeContext(), "DOCKER_FILE_MAPPING_ALARM", "cmdType", PluginDockerUpdateFile, "cmd", cmdBuf, "error", err)
	}
}

// deleteMappingFromLogtail  把需要删除的容器信息的id 发送给c++
func (idf *InputDockerFile) deleteMappingFromLogtail(id string) {
	var cmd DockerFileUpdateCmd
	cmd.ID = id
	logger.Info(idf.context.GetRuntimeContext(), "deleteMappingFromLogtail cmd", cmd)
	cmdBuf, _ := json.Marshal(&cmd)
	configName := idf.context.GetConfigName()
	if err := logtail.ExecuteCMD(configName, PluginDockerDeleteFile, cmdBuf); err != nil {
		logger.Error(idf.context.GetRuntimeContext(), "DOCKER_FILE_MAPPING_ALARM", "cmdType", PluginDockerDeleteFile, "cmd", cmdBuf, "error", err)
	}
}

// notifyStopToLogtail 通知c++ 该容器已经停止
func (idf *InputDockerFile) notifyStopToLogtail(id string) {
	var cmd DockerFileUpdateCmd
	cmd.ID = id
	logger.Info(idf.context.GetRuntimeContext(), "notifyStopToLogtail cmd", cmd)
	cmdBuf, _ := json.Marshal(&cmd)
	configName := idf.context.GetConfigName()
	if err := logtail.ExecuteCMD(configName, PluginDockerStopFile, cmdBuf); err != nil {
		logger.Error(idf.context.GetRuntimeContext(), "DOCKER_FILE_MAPPING_ALARM", "cmdType", PluginDockerStopFile, "cmd", cmdBuf, "error", err)
	}
}

// updateAll  更新所有容器信息
func (idf *InputDockerFile) updateAll(allCmd *DockerFileUpdateCmdAll) {
	logger.Info(idf.context.GetRuntimeContext(), "update all", len(allCmd.AllCmd))
	cmdBuf, _ := json.Marshal(allCmd)
	configName := idf.context.GetConfigName()
	if err := logtail.ExecuteCMD(configName, PluginDockerUpdateFileAll, cmdBuf); err != nil {
		logger.Error(idf.context.GetRuntimeContext(), "DOCKER_FILE_MAPPING_ALARM", "cmdType", PluginDockerUpdateFileAll, "cmd", cmdBuf, "error", err)
	}
}

func (idf *InputDockerFile) updateMapping(info *helper.DockerInfoDetail, allCmd *DockerFileUpdateCmdAll) {
	logPath := path.Clean(info.StdoutPath)
	id := info.ContainerInfo.ID
	mounts := info.ContainerInfo.Mounts
	upperDir := info.DefaultRootPath
	changed := false

	// logPath
	if val, ok := idf.lastContainerInfoCache[id]; ok && val.LogPath != logPath {
		// send delete first and then add this info
		logger.Info(idf.context.GetRuntimeContext(), "container logPath", "changed", "last", val, "logPath", logPath,
			"id", info.IDPrefix(), "name", info.ContainerInfo.Name, "created", info.ContainerInfo.Created, "status", info.Status())
		changed = true
	} else if !ok {
		logger.Info(idf.context.GetRuntimeContext(), "container logPath", "added", "logPath", logPath,
			"id", info.IDPrefix(), "name", info.ContainerInfo.Name, "created", info.ContainerInfo.Created, "status", info.Status())
		changed = true
	}
	// upperDir
	if !changed {
		if val, ok := idf.lastContainerInfoCache[id]; ok && val.UpperDir != upperDir {
			// send delete first and then add this info
			logger.Info(idf.context.GetRuntimeContext(), "container upperDir", "changed", "last", val, "upperDir", upperDir,
				"id", info.IDPrefix(), "name", info.ContainerInfo.Name, "created", info.ContainerInfo.Created, "status", info.Status())
			changed = true
		} else if !ok {
			logger.Info(idf.context.GetRuntimeContext(), "container upperDir", "added", "upperDir", upperDir,
				"id", info.IDPrefix(), "name", info.ContainerInfo.Name, "created", info.ContainerInfo.Created, "status", info.Status())
			changed = true
		}
	}

	sortMounts := func(mounts []types.MountPoint) {
		sort.Slice(mounts, func(i, j int) bool {
			return mounts[i].Source < mounts[j].Source
		})
	}
	sortMounts(mounts)
	// 判断mounts
	if !changed {
		if val, ok := idf.lastContainerInfoCache[id]; ok && !reflect.DeepEqual(val.Mounts, mounts) {
			// send delete first and then add this info
			logger.Info(idf.context.GetRuntimeContext(), "container mounts", "changed", "last", val, "mounts", mounts,
				"id", info.IDPrefix(), "name", info.ContainerInfo.Name, "created", info.ContainerInfo.Created, "status", info.Status())
			changed = true
		} else if !ok {
			logger.Info(idf.context.GetRuntimeContext(), "container mounts", "added", "mounts", mounts,
				"id", info.IDPrefix(), "name", info.ContainerInfo.Name, "created", info.ContainerInfo.Created, "status", info.Status())
			changed = true
		}
	}

	if changed {
		idf.updateMetric.Add(1)
		newContainerInfoCache := ContainerInfoCache{
			Mounts:   mounts,
			UpperDir: upperDir,
			LogPath:  logPath,
		}
		idf.lastContainerInfoCache[id] = newContainerInfoCache
		idf.addMappingToLogtail(info, newContainerInfoCache, allCmd)
	}
}

// deleteMapping  删除容器信息
func (idf *InputDockerFile) deleteMapping(id string) {
	idf.deleteMappingFromLogtail(id)
	logger.Info(idf.context.GetRuntimeContext(), "container mapping", "deleted", "id", helper.GetShortID(id),
		"logPath", idf.lastContainerInfoCache[id].LogPath,
		"upperDir", idf.lastContainerInfoCache[id].UpperDir,
		"mounts", idf.lastContainerInfoCache[id].Mounts)
	delete(idf.lastContainerInfoCache, id)
}

// notifyStop 通知容器停止
func (idf *InputDockerFile) notifyStop(id string) {
	idf.notifyStopToLogtail(id)
	logger.Info(idf.context.GetRuntimeContext(), "container mapping", "stopped", "id", helper.GetShortID(id),
		"logPath", idf.lastContainerInfoCache[id].LogPath,
		"upperDir", idf.lastContainerInfoCache[id].UpperDir,
		"mounts", idf.lastContainerInfoCache[id].Mounts)
}

func (idf *InputDockerFile) Collect(collector pipeline.Collector) error {
	newUpdateTime := helper.GetContainersLastUpdateTime()
	if idf.lastUpdateTime != 0 {
		// Nothing update, just skip.
		if idf.lastUpdateTime >= newUpdateTime {
			return nil
		}
	}

	var allCmd *DockerFileUpdateCmdAll
	allCmd = nil
	// if cache is empty, use update all cmd
	if len(idf.lastContainerInfoCache) == 0 {
		allCmd = new(DockerFileUpdateCmdAll)
	}
	newCount, delCount, addResultList, deleteResultList := helper.GetContainerByAcceptedInfoV2(

		idf.fullList, idf.matchList,
		idf.IncludeLabel, idf.ExcludeLabel,
		idf.IncludeLabelRegex, idf.ExcludeLabelRegex,
		idf.IncludeEnv, idf.ExcludeEnv,
		idf.IncludeEnvRegex, idf.ExcludeEnvRegex,
		idf.K8sFilter)
	idf.lastUpdateTime = newUpdateTime
	// record config result
	havingPathkeys := make([]string, 0)
	nothavingPathkeys := make([]string, 0)
	if newCount != 0 || delCount != 0 {
		logger.Infof(idf.context.GetRuntimeContext(), "update match list, new: %v, delete: %v", newCount, delCount)
		// Can not return here because we should notify empty update to clear
		//  cache in docker_path_helper.json.
	} else {
		logger.Debugf(idf.context.GetRuntimeContext(), "update match list, new: %v, delete: %v", newCount, delCount)
	}

	dockerInfoDetails := idf.matchList
	logger.Debug(idf.context.GetRuntimeContext(), "match list length", len(dockerInfoDetails))
	idf.avgInstanceMetric.Add(int64(len(dockerInfoDetails)))

	for k, info := range dockerInfoDetails {
		if len(idf.LogPath) > 0 && info.ContainerInfo.State.Status == helper.ContainerStatusRunning {
			// inputFile
			idf.updateMapping(info, allCmd)
		} else if len(idf.LogPath) == 0 {
			// stdout
			idf.updateMapping(info, allCmd)
		}
		// 容器元信息预览使用
		if idf.CollectingContainersMeta && len(idf.LogPath) > 0 {
			sourcePath, containerPath := info.FindBestMatchedPath(idf.LogPath)

			formatSourcePath := formatPath(sourcePath)
			formateContainerPath := formatPath(containerPath)
			destPath := helper.GetMountedFilePathWithBasePath(idf.MountPath, formatSourcePath) + idf.LogPath[len(formateContainerPath):]

			if ok, err := util.PathExists(destPath); err == nil {
				if !ok {
					nothavingPathkeys = append(nothavingPathkeys, helper.GetShortID(k))
				} else {
					havingPathkeys = append(havingPathkeys, helper.GetShortID(k))
				}
			} else {
				nothavingPathkeys = append(nothavingPathkeys, helper.GetShortID(k))
			}
		}
	}
	if idf.CollectingContainersMeta {
		var configResult *helper.ContainerConfigResult
		if len(idf.LogPath) == 0 {
			keys := make([]string, 0, len(idf.matchList))
			for k := range idf.matchList {
				if len(k) > 0 {
					keys = append(keys, helper.GetShortID(k))
				}
			}
			configResult = &helper.ContainerConfigResult{
				DataType:                   "container_config_result",
				Project:                    idf.context.GetProject(),
				Logstore:                   idf.context.GetLogstore(),
				ConfigName:                 idf.context.GetConfigName(),
				PathExistInputContainerIDs: helper.GetStringFromList(keys),
				SourceAddress:              "stdout",
				InputType:                  "input_container_stdio",
				FlusherType:                "flusher_sls",
				FlusherTargetAddress:       fmt.Sprintf("%s/%s", idf.context.GetProject(), idf.context.GetLogstore()),
			}
		} else {
			configResult = &helper.ContainerConfigResult{
				DataType:                      "container_config_result",
				Project:                       idf.context.GetProject(),
				Logstore:                      idf.context.GetLogstore(),
				ConfigName:                    idf.context.GetConfigName(),
				SourceAddress:                 fmt.Sprintf("%s/**/%s", idf.LogPath, idf.FilePattern),
				PathExistInputContainerIDs:    helper.GetStringFromList(havingPathkeys),
				PathNotExistInputContainerIDs: helper.GetStringFromList(nothavingPathkeys),
				InputType:                     "file_log",
				InputIsContainerFile:          "true",
				FlusherType:                   "flusher_sls",
				FlusherTargetAddress:          fmt.Sprintf("%s/%s", idf.context.GetProject(), idf.context.GetLogstore()),
			}
		}
		helper.RecordContainerConfigResultMap(configResult)
		if newCount != 0 || delCount != 0 || idf.firstStart {
			helper.RecordContainerConfigResultIncrement(configResult)
			idf.firstStart = false
		}
		logger.Debugf(idf.context.GetRuntimeContext(), "update match list, addResultList: %v, deleteResultList: %v", addResultList, deleteResultList)
	}

	for id := range idf.lastContainerInfoCache {
		if c, ok := dockerInfoDetails[id]; !ok {
			idf.deleteMetric.Add(1)
			idf.notifyStop(id)
			idf.deleteMapping(id)
		} else if c.Status() != helper.ContainerStatusRunning && len(idf.LogPath) > 0 {
			// input_file时会触发
			idf.notifyStop(id)
		}
	}
	if allCmd != nil {
		if len(allCmd.AllCmd) == 0 {
			// only update empty if empty flag is true
			if idf.updateEmptyFlag {
				idf.updateAll(allCmd)
				idf.updateEmptyFlag = false
			}
		} else {
			idf.updateAll(allCmd)
			idf.updateEmptyFlag = true
		}

	}

	if time.Since(idf.lastClearTime) > time.Hour {
		idf.lastContainerInfoCache = make(map[string]ContainerInfoCache)
		idf.lastClearTime = time.Now()
	}

	return nil
}

func init() {
	pipeline.MetricInputs["metric_container_info"] = func() pipeline.MetricInput {
		return &InputDockerFile{
			FlushIntervalMs: 3000,
		}
	}
}
