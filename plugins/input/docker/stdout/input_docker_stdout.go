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

package stdout

import (
	"os"
	"regexp"
	"sync"
	"time"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/plugins/input"

	docker "github.com/fsouza/go-dockerclient"
)

const serviceDockerStdoutKey = "service_docker_stdout_v2"

func logDriverSupported(container *docker.Container) bool {
	// containerd has no hostConfig, return true
	if container.HostConfig == nil {
		return true
	}
	switch container.HostConfig.LogConfig.Type {
	case "json-file":
		return true
	default:
		return false
	}
}

type DockerFileSyner struct {
	dockerFileReader    *helper.LogFileReader
	dockerFileProcessor *DockerStdoutProcessor
	info                *helper.DockerInfoDetail
}

func NewDockerFileSynerByFile(sds *ServiceDockerStdout, filePath string) *DockerFileSyner {
	dockerInfoDetail := &helper.DockerInfoDetail{}
	dockerInfoDetail.ContainerInfo = &docker.Container{}
	dockerInfoDetail.ContainerInfo.LogPath = filePath
	sds.LogtailInDocker = false
	sds.StartLogMaxOffset = 10 * 1024 * 1024 * 1024
	return NewDockerFileSyner(sds, dockerInfoDetail, sds.checkpointMap)
}

func NewDockerFileSyner(sds *ServiceDockerStdout,
	info *helper.DockerInfoDetail,
	checkpointMap map[string]helper.LogFileReaderCheckPoint) *DockerFileSyner {
	var reg *regexp.Regexp
	var err error
	if len(sds.BeginLineRegex) > 0 {
		if reg, err = regexp.Compile(sds.BeginLineRegex); err != nil {
			logger.Warning(sds.context.GetRuntimeContext(), "DOCKER_REGEX_COMPILE_ALARM", "compile begin line regex error, regex", sds.BeginLineRegex, "error", err)
		}
	}

	tags := info.GetExternalTags(sds.ExternalEnvTag, sds.ExternalK8sLabelTag)
	processor := NewDockerStdoutProcessor(reg, time.Duration(sds.BeginLineTimeoutMs)*time.Millisecond, sds.BeginLineCheckLength, sds.MaxLogSize, sds.Stdout, sds.Stderr, sds.context, sds.collector, tags)

	checkpoint, ok := checkpointMap[info.ContainerInfo.ID]
	if !ok {
		if sds.LogtailInDocker {
			checkpoint.Path = helper.GetMountedFilePath(info.ContainerInfo.LogPath)
		} else {
			checkpoint.Path = info.ContainerInfo.LogPath
		}

		// first watch this container
		stat, err := os.Stat(checkpoint.Path)
		if err != nil {
			logger.Warning(sds.context.GetRuntimeContext(), "DOCKER_STDOUT_STAT_ALARM", "stat log file error, path", checkpoint.Path, "error", err.Error())
		} else {
			checkpoint.Offset = stat.Size()
			if checkpoint.Offset > sds.StartLogMaxOffset {
				logger.Warning(sds.context.GetRuntimeContext(), "DOCKER_STDOUT_START_ALARM", "log file too big, path", checkpoint.Path, "offset", checkpoint.Offset)
				checkpoint.Offset -= sds.StartLogMaxOffset
			} else {
				checkpoint.Offset = 0
			}
			checkpoint.State = helper.GetOSState(stat)
		}
	}
	if sds.CloseUnChangedSec < 10 {
		sds.CloseUnChangedSec = 10
	}

	logger.Info(sds.context.GetRuntimeContext(), "docker_id", info.ContainerInfo.ID,
		"checkpoint_logpath", checkpoint.Path,
		"in_docker", sds.LogtailInDocker)

	config := helper.LogFileReaderConfig{
		ReadIntervalMs:   sds.ReadIntervalMs,
		MaxReadBlockSize: sds.MaxLogSize,
		CloseFileSec:     sds.CloseUnChangedSec,
		Tracker:          sds.tracker,
	}
	reader, _ := helper.NewLogFileReader(sds.context.GetRuntimeContext(), checkpoint, config, processor)

	return &DockerFileSyner{
		dockerFileReader:    reader,
		info:                info,
		dockerFileProcessor: processor,
	}
}

type ServiceDockerStdout struct {
	IncludeLabel          map[string]string `comment:"include container label for selector. [Deprecated： use IncludeContainerLabel and IncludeK8sLabel instead]"`
	ExcludeLabel          map[string]string `comment:"exclude container label for selector. [Deprecated： use ExcludeContainerLabel and ExcludeK8sLabel instead]"`
	IncludeEnv            map[string]string `comment:"the container would be selected when it is matched by any environment rules. Furthermore, the regular expression starts with '^' is supported as the env value, such as 'ENVA:^DE.*$'' would hit all containers having any envs starts with DE."`
	ExcludeEnv            map[string]string `comment:"the container would be excluded when it is matched by any environment rules. Furthermore, the regular expression starts with '^' is supported as the env value, such as 'ENVA:^DE.*$'' would hit all containers having any envs starts with DE."`
	IncludeContainerLabel map[string]string `comment:"the container would be selected when it is matched by any container labels. Furthermore, the regular expression starts with '^' is supported as the label value, such as 'LABEL:^DE.*$'' would hit all containers having any labels starts with DE."`
	ExcludeContainerLabel map[string]string `comment:"the container would be excluded when it is matched by any container labels. Furthermore, the regular expression starts with '^' is supported as the label value, such as 'LABEL:^DE.*$'' would hit all containers having any labels starts with DE."`
	IncludeK8sLabel       map[string]string `comment:"the container of pod would be selected when it is matched by any include k8s label rules. Furthermore, the regular expression starts with '^' is supported as the value to match pods."`
	ExcludeK8sLabel       map[string]string `comment:"the container of pod would be excluded when it is matched by any exclude k8s label rules. Furthermore, the regular expression starts with '^' is supported as the value to exclude pods."`
	ExternalEnvTag        map[string]string `comment:"extract the env value as the log tags for one container, such as the value of ENVA would be appended to the 'taga' of log tags when configured 'ENVA:taga' pair."`
	ExternalK8sLabelTag   map[string]string `comment:"extract the pod label value as the log tags for one container, such as the value of LABELA would be appended to the 'taga' of log tags when configured 'LABELA:taga' pair."`
	FlushIntervalMs       int               `comment:"the interval of container discovery，and the timeunit is millisecond. Default value is 3000."`
	ReadIntervalMs        int               `comment:"the interval of read stdout log，and the timeunit is millisecond. Default value is 1000."`
	SaveCheckPointSec     int               `comment:"the interval of save checkpoint，and the timeunit is second. Default value is 60."`
	BeginLineRegex        string            `comment:"the regular expression of begin line for the multi line log."`
	BeginLineTimeoutMs    int               `comment:"the maximum timeout milliseconds for begin line match. Default value is 3000."`
	BeginLineCheckLength  int               `comment:"the prefix length of log line to match the first line. Default value is 10240."`
	MaxLogSize            int               `comment:"the maximum log size. Default value is 512*1024, a.k.a 512K."`
	CloseUnChangedSec     int               `comment:"the reading file would be close when the interval between last read operation is over {CloseUnChangedSec} seconds. Default value is 60."`
	StartLogMaxOffset     int64             `comment:"the first read operation would read {StartLogMaxOffset} size history logs. Default value is 128*1024, a.k.a 128K."`
	Stdout                bool              `comment:"collect stdout log. Default is true."`
	Stderr                bool              `comment:"collect stderr log. Default is true."`
	LogtailInDocker       bool              `comment:"the logtail running mode. Default is true."`
	K8sNamespaceRegex     string            `comment:"the regular expression of kubernetes namespace to match containers."`
	K8sPodRegex           string            `comment:"the regular expression of kubernetes pod to match containers."`
	K8sContainerRegex     string            `comment:"the regular expression of kubernetes container to match containers."`

	// export from ilogtail-trace component
	IncludeLabelRegex map[string]*regexp.Regexp
	ExcludeLabelRegex map[string]*regexp.Regexp
	IncludeEnvRegex   map[string]*regexp.Regexp
	ExcludeEnvRegex   map[string]*regexp.Regexp
	K8sFilter         *helper.K8SFilter

	// for tracker
	tracker           *helper.ReaderMetricTracker
	avgInstanceMetric ilogtail.CounterMetric
	addMetric         ilogtail.CounterMetric
	deleteMetric      ilogtail.CounterMetric

	synerMap      map[string]*DockerFileSyner
	checkpointMap map[string]helper.LogFileReaderCheckPoint
	shutdown      chan struct {
	}
	waitGroup sync.WaitGroup
	context   ilogtail.Context
	collector ilogtail.Collector

	// Last return of GetAllAcceptedInfoV2
	fullList       map[string]bool
	matchList      map[string]*helper.DockerInfoDetail
	lastUpdateTime int64
}

func (sds *ServiceDockerStdout) Init(context ilogtail.Context) (int, error) {
	sds.context = context
	helper.ContainerCenterInit()
	sds.fullList = make(map[string]bool)
	sds.matchList = make(map[string]*helper.DockerInfoDetail)
	sds.synerMap = make(map[string]*DockerFileSyner)
	if sds.MaxLogSize < 1024 {
		sds.MaxLogSize = 1024
	}
	if sds.MaxLogSize > 1024*1024*20 {
		sds.MaxLogSize = 1024 * 1024 * 20
	}
	sds.tracker = helper.NewReaderMetricTracker()
	sds.context.RegisterCounterMetric(sds.tracker.CloseCounter)
	sds.context.RegisterCounterMetric(sds.tracker.OpenCounter)
	sds.context.RegisterCounterMetric(sds.tracker.ReadSizeCounter)
	sds.context.RegisterCounterMetric(sds.tracker.ReadCounter)
	sds.context.RegisterCounterMetric(sds.tracker.FileSizeCounter)
	sds.context.RegisterCounterMetric(sds.tracker.FileRotatorCounter)
	sds.context.RegisterLatencyMetric(sds.tracker.ProcessLatency)

	sds.avgInstanceMetric = helper.NewAverageMetric("container_count")
	sds.addMetric = helper.NewCounterMetric("add_container")
	sds.deleteMetric = helper.NewCounterMetric("remove_container")
	sds.context.RegisterCounterMetric(sds.avgInstanceMetric)
	sds.context.RegisterCounterMetric(sds.addMetric)
	sds.context.RegisterCounterMetric(sds.deleteMetric)

	var err error
	sds.IncludeEnv, sds.IncludeEnvRegex, err = helper.SplitRegexFromMap(sds.IncludeEnv)
	if err != nil {
		logger.Warning(sds.context.GetRuntimeContext(), "INVALID_REGEX_ALARM", "init include env regex error", err)
	}
	sds.ExcludeEnv, sds.ExcludeEnvRegex, err = helper.SplitRegexFromMap(sds.ExcludeEnv)
	if err != nil {
		logger.Warning(sds.context.GetRuntimeContext(), "INVALID_REGEX_ALARM", "init exclude env regex error", err)
	}
	if sds.IncludeLabel != nil {
		for k, v := range sds.IncludeContainerLabel {
			sds.IncludeLabel[k] = v
		}
	} else {
		sds.IncludeLabel = sds.IncludeContainerLabel
	}
	if sds.ExcludeLabel != nil {
		for k, v := range sds.ExcludeContainerLabel {
			sds.ExcludeLabel[k] = v
		}
	} else {
		sds.ExcludeLabel = sds.ExcludeContainerLabel
	}
	sds.IncludeLabel, sds.IncludeLabelRegex, err = helper.SplitRegexFromMap(sds.IncludeLabel)
	if err != nil {
		logger.Warning(sds.context.GetRuntimeContext(), "INVALID_REGEX_ALARM", "init include label regex error", err)
	}
	sds.ExcludeLabel, sds.ExcludeLabelRegex, err = helper.SplitRegexFromMap(sds.ExcludeLabel)

	if err != nil {
		logger.Warning(sds.context.GetRuntimeContext(), "INVALID_REGEX_ALARM", "init exclude label regex error", err)
	}
	sds.K8sFilter, err = helper.CreateK8SFilter(sds.K8sNamespaceRegex, sds.K8sPodRegex, sds.K8sContainerRegex, sds.IncludeK8sLabel, sds.ExcludeK8sLabel)

	return 0, err
}

func (sds *ServiceDockerStdout) Description() string {
	return "the container stdout input plugin for iLogtail, which supports docker and containerd."
}

func (sds *ServiceDockerStdout) Collect(ilogtail.Collector) error {
	return nil
}

func (sds *ServiceDockerStdout) FlushAll(c ilogtail.Collector, firstStart bool) error {
	newUpdateTime := helper.GetContainersLastUpdateTime()
	if sds.lastUpdateTime != 0 {
		if sds.lastUpdateTime >= newUpdateTime {
			return nil
		}
	}

	var err error
	newCount, delCount := helper.GetContainerByAcceptedInfoV2(
		sds.fullList, sds.matchList,
		sds.IncludeLabel, sds.ExcludeLabel,
		sds.IncludeLabelRegex, sds.ExcludeLabelRegex,
		sds.IncludeEnv, sds.ExcludeEnv,
		sds.IncludeEnvRegex, sds.ExcludeEnvRegex,
		sds.K8sFilter)
	sds.lastUpdateTime = newUpdateTime
	if !firstStart && newCount == 0 && delCount == 0 {
		return nil
	}
	logger.Infof(sds.context.GetRuntimeContext(), "update match list, first: %v, new: %v, delete: %v",
		firstStart, newCount, delCount)

	dockerInfos := sds.matchList
	logger.Debug(sds.context.GetRuntimeContext(), "flush all", len(dockerInfos))
	sds.avgInstanceMetric.Add(int64(len(dockerInfos)))
	for id, info := range dockerInfos {
		if !logDriverSupported(info.ContainerInfo) {
			continue
		}
		if _, ok := sds.synerMap[id]; !ok || firstStart {
			syner := NewDockerFileSyner(sds, info, sds.checkpointMap)
			sds.addMetric.Add(1)
			sds.synerMap[id] = syner
			syner.dockerFileReader.Start()
		}
	}

	// delete container
	for id, syner := range sds.synerMap {
		if _, ok := dockerInfos[id]; !ok {
			logger.Info(sds.context.GetRuntimeContext(), "delete docker stdout, id", id, "name", syner.info.ContainerInfo.Name)
			syner.dockerFileReader.Stop()
			delete(sds.synerMap, id)
			sds.deleteMetric.Add(1)
		}
	}

	return err
}

func (sds *ServiceDockerStdout) SaveCheckPoint(force bool) error {
	checkpointChanged := false
	for id, syner := range sds.synerMap {
		checkpoint, changed := syner.dockerFileReader.GetCheckpoint()
		if changed {
			checkpointChanged = true
		}
		sds.checkpointMap[id] = checkpoint
	}
	if !force && !checkpointChanged {
		logger.Debug(sds.context.GetRuntimeContext(), "no need to save checkpoint, checkpoint size", len(sds.checkpointMap))
		return nil
	}
	logger.Debug(sds.context.GetRuntimeContext(), "save checkpoint, checkpoint size", len(sds.checkpointMap))
	return sds.context.SaveCheckPointObject(serviceDockerStdoutKey, sds.checkpointMap)
}

func (sds *ServiceDockerStdout) LoadCheckPoint() {
	if sds.checkpointMap != nil {
		return
	}
	sds.checkpointMap = make(map[string]helper.LogFileReaderCheckPoint)
	sds.context.GetCheckPointObject(serviceDockerStdoutKey, &sds.checkpointMap)
}

func (sds *ServiceDockerStdout) ClearUselessCheckpoint() {
	if sds.checkpointMap == nil {
		return
	}
	for id := range sds.checkpointMap {
		if _, ok := sds.synerMap[id]; !ok {
			logger.Info(sds.context.GetRuntimeContext(), "delete checkpoint, id", id)
			delete(sds.checkpointMap, id)
		}
	}
}

// Start starts the ServiceInput's service, whatever that may be
func (sds *ServiceDockerStdout) Start(c ilogtail.Collector) error {
	sds.collector = c
	sds.shutdown = make(chan struct{})
	sds.waitGroup.Add(1)
	defer sds.waitGroup.Done()

	sds.LoadCheckPoint()

	lastSaveCheckPointTime := time.Now()

	_ = sds.FlushAll(c, true)
	for {
		timer := time.NewTimer(time.Duration(sds.FlushIntervalMs) * time.Millisecond)
		select {
		case <-sds.shutdown:
			logger.Info(sds.context.GetRuntimeContext(), "docker stdout main runtime stop", "begin")
			for _, syner := range sds.synerMap {
				syner.dockerFileReader.Stop()
			}
			logger.Info(sds.context.GetRuntimeContext(), "docker stdout main runtime stop", "success")
			return nil
		case <-timer.C:
			if nowTime := time.Now(); nowTime.Sub(lastSaveCheckPointTime) > time.Second*time.Duration(sds.SaveCheckPointSec) {
				_ = sds.SaveCheckPoint(false)
				lastSaveCheckPointTime = nowTime
				sds.ClearUselessCheckpoint()
			}
			_ = sds.FlushAll(c, false)
		}
	}
}

// Stop stops the services and closes any necessary channels and connections
func (sds *ServiceDockerStdout) Stop() error {
	close(sds.shutdown)
	sds.waitGroup.Wait()
	// force save checkpoint
	_ = sds.SaveCheckPoint(true)
	return nil
}

func init() {
	ilogtail.ServiceInputs[input.ServiceDockerStdoutPluginName] = func() ilogtail.ServiceInput {
		return &ServiceDockerStdout{
			FlushIntervalMs:      3000,
			SaveCheckPointSec:    60,
			ReadIntervalMs:       1000,
			Stdout:               true,
			Stderr:               true,
			BeginLineTimeoutMs:   3000,
			LogtailInDocker:      true,
			CloseUnChangedSec:    60,
			BeginLineCheckLength: 10 * 1024,
			MaxLogSize:           512 * 1024,
			StartLogMaxOffset:    128 * 1024,
		}
	}
}
