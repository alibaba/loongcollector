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
	"context"
	"encoding/json"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"

	"github.com/docker/docker/api/types"
	"github.com/docker/docker/api/types/container"
)

const staticContainerInfoPathEnvKey = "ALIYUN_LOG_STATIC_CONTAINER_INFO"

// const staticContainerType = "ALIYUN_LOG_STATIC_CONTAINER_TYPE"
// const staticContainerDScanInterval = "ALIYUN_LOG_STATIC_CONTAINED_SCAN_INTERVAL"
// const staticContainerTypeContainerD = "containerd"

var containerdScanIntervalMs = 1000
var staticDockerContainers []types.ContainerJSON
var staticDockerContainerError error
var staticDockerContainerLock sync.Mutex
var loadStaticContainerOnce sync.Once
var staticDockerContainerLastStat StateOS
var staticDockerContainerFile string
var staticDockerContainerLastBody string

type staticContainerMount struct {
	Source      string
	Destination string
	Driver      string
}

type staticContainerState struct {
	Status string
	Pid    int
}

type staticContainerInfo struct {
	ID       string
	Name     string
	Created  string
	HostName string
	IP       string
	Image    string
	LogPath  string
	Labels   map[string]string
	LogType  string
	UpperDir string
	Env      map[string]string
	Mounts   []staticContainerMount
	State    staticContainerState
}

func staticContainerInfoToStandard(staticInfo *staticContainerInfo, stat fs.FileInfo) types.ContainerJSON {
	created, err := time.Parse(time.RFC3339Nano, staticInfo.Created)
	if err != nil {
		created = stat.ModTime()
	}

	allEnv := make([]string, 0, len(staticInfo.Env))
	for key, val := range staticInfo.Env {
		allEnv = append(allEnv, key+"="+val)
	}

	if staticInfo.HostName == "" {
		staticInfo.HostName = util.GetHostName()
	}

	if staticInfo.IP == "" {
		staticInfo.IP = util.GetIPAddress()
	}

	status := staticInfo.State.Status
	if status != ContainerStatusExited {
		if !ContainerProcessAlive(staticInfo.State.Pid) {
			status = ContainerStatusExited
		}
	}

	dockerContainer := types.ContainerJSON{
		ContainerJSONBase: &types.ContainerJSONBase{
			ID:      staticInfo.ID,
			Name:    staticInfo.Name,
			Created: created.Format(time.RFC3339Nano),
			LogPath: staticInfo.LogPath,
			HostConfig: &container.HostConfig{
				LogConfig: container.LogConfig{
					Type: staticInfo.LogType,
				},
			},
			GraphDriver: types.GraphDriverData{
				Name: "overlay",
				Data: map[string]string{
					"UpperDir": staticInfo.UpperDir,
				},
			},
			State: &types.ContainerState{
				Status: status,
				Pid:    staticInfo.State.Pid,
			},
		},
		Config: &container.Config{
			Labels:   staticInfo.Labels,
			Image:    staticInfo.Image,
			Env:      allEnv,
			Hostname: staticInfo.HostName,
		},
		NetworkSettings: &types.NetworkSettings{
			DefaultNetworkSettings: types.DefaultNetworkSettings{
				IPAddress: staticInfo.IP,
			},
		},
	}

	for _, mount := range staticInfo.Mounts {
		dockerContainer.Mounts = append(dockerContainer.Mounts, types.MountPoint{
			Source:      filepath.Clean(mount.Source),
			Destination: filepath.Clean(mount.Destination),
			Driver:      mount.Driver,
		})
	}
	sortMounts := func(mounts []types.MountPoint) {
		sort.Slice(mounts, func(i, j int) bool {
			return mounts[i].Source < mounts[j].Source
		})
	}
	sortMounts(dockerContainer.Mounts)
	return dockerContainer
}

func overWriteSymLink(oldname, newname string) error {
	if _, err := os.Lstat(newname); err == nil {
		if err = os.Remove(newname); err != nil {
			return fmt.Errorf("failed to unlink: %+v", err)
		}
	} else if !os.IsNotExist(err) {
		return fmt.Errorf("failed to check symlink: %+v", err)
	}
	return os.Symlink(oldname, newname)
}

func scanContainerdFilesAndReLink(filePath string) {
	const logSuffix = ".log"
	dirPath := filepath.Dir(filePath)

	maxFileNo := 0
	if err := overWriteSymLink(filepath.Join(dirPath, strconv.Itoa(maxFileNo)+logSuffix), filePath); err != nil {
		logger.Error(context.Background(), "overwrite symbol link error, from", maxFileNo, "to", filePath, "error", err)
	} else {
		logger.Info(context.Background(), "overwrite symbol link success, from", maxFileNo, "to", filePath)
	}
	for {
		time.Sleep(time.Millisecond * time.Duration(containerdScanIntervalMs))
		dir, err := os.ReadDir(dirPath)
		if err != nil {
			continue
		}

		for _, fi := range dir {
			if fi.IsDir() {
				continue
			}

			fileName := fi.Name()
			if ok := strings.HasSuffix(fi.Name(), logSuffix); !ok {
				continue
			}
			baseName := fileName[0 : len(fileName)-len(logSuffix)]
			fileNo, err := strconv.Atoi(baseName)
			// not x.log
			if err != nil {
				continue
			}
			if fileNo <= maxFileNo {
				continue
			}
			if err := overWriteSymLink(filepath.Join(dirPath, strconv.Itoa(fileNo)+logSuffix), filePath); err == nil {
				logger.Info(context.Background(), "overwrite symbol link success, from", fileNo, "to", filePath, "max", maxFileNo)
				maxFileNo = fileNo
			} else {
				logger.Error(context.Background(), "overwrite symbol link error, from", fileNo, "to", filePath, "error", err, "max", maxFileNo)
			}
		}
	}
}

func innerReadStatisContainerInfo(file string, lastContainerInfo []types.ContainerJSON, stat fs.FileInfo) (containers []types.ContainerJSON, removed []string, changed bool, err error) {
	body, err := os.ReadFile(filepath.Clean(file))
	if err != nil {
		return nil, nil, false, err
	}
	var staticContainerInfos []staticContainerInfo
	err = json.Unmarshal(body, &staticContainerInfos)
	if err != nil {
		return nil, nil, false, err
	}

	if staticDockerContainerLastBody == "" {
		changed = true
	} else if staticDockerContainerLastBody != string(body) {
		changed = true
	}
	staticDockerContainerLastBody = string(body)

	nowIDs := make(map[string]struct{})
	for i := 0; i < len(staticContainerInfos); i++ {
		containers = append(containers, staticContainerInfoToStandard(&staticContainerInfos[i], stat))
		nowIDs[staticContainerInfos[i].ID] = struct{}{}
	}

	for _, lastContainer := range lastContainerInfo {
		if _, ok := nowIDs[lastContainer.ID]; !ok {
			removed = append(removed, lastContainer.ID)
		}
	}
	logger.Infof(context.Background(), "read static container info: %#v", staticContainerInfos)
	return containers, removed, changed, err
}

func isStaticContainerInfoEnabled() bool {
	envPath := os.Getenv(staticContainerInfoPathEnvKey)
	return len(envPath) != 0
}

func tryReadStaticContainerInfo() ([]types.ContainerJSON, []string, bool, error) {
	statusChanged := false
	loadStaticContainerOnce.Do(
		func() {
			envPath := os.Getenv(staticContainerInfoPathEnvKey)
			if len(envPath) == 0 {
				return
			}
			staticDockerContainerFile = envPath
			stat, err := os.Stat(staticDockerContainerFile)
			staticDockerContainers, _, statusChanged, staticDockerContainerError = innerReadStatisContainerInfo(staticDockerContainerFile, nil, stat)
			if err == nil {
				staticDockerContainerLastStat = GetOSState(stat)
			}
			// not used yet
			// if containerType := os.Getenv(staticContainerType); containerType == staticContainerTypeContainerD {
			// 	if containerDScanInterval := os.Getenv(staticContainerDScanInterval); containerDScanInterval != "" {
			// 		interval, err := strconv.Atoi(containerDScanInterval)
			// 		if err != nil && interval > 0 && interval < 24*3600 {
			// 			containerdScanIntervalMs = interval
			// 		}
			// 	}
			// 	for i := 0; i < len(staticContainerInfos); i++ {
			// 		go scanContainerdFilesAndReLink(GetMountedFilePath(staticContainerInfos[i].LogPath))
			// 	}
			// }
		},
	)

	if staticDockerContainerFile == "" {
		return nil, nil, false, nil
	}

	for _, container := range staticDockerContainers {
		if container.State.Status == ContainerStatusRunning && !ContainerProcessAlive(container.State.Pid) {
			container.State.Status = ContainerStatusExited
			statusChanged = true
		}
	}

	stat, err := os.Stat(staticDockerContainerFile)
	if err != nil {
		return staticDockerContainers, nil, statusChanged, err
	}

	osStat := GetOSState(stat)
	if !osStat.IsChange(staticDockerContainerLastStat) && staticDockerContainerError == nil {
		return staticDockerContainers, nil, statusChanged, nil
	}

	newStaticDockerContainer, removedIDs, changed, staticDockerContainerError := innerReadStatisContainerInfo(staticDockerContainerFile, staticDockerContainers, stat)
	changed = changed || statusChanged
	if staticDockerContainerError == nil {
		staticDockerContainers = newStaticDockerContainer
	}
	staticDockerContainerLastStat = osStat

	logger.Info(context.Background(), "static docker container info file changed or last read error, last", staticDockerContainerLastStat,
		"now", osStat, "removed ids", removedIDs, "error", staticDockerContainerError)
	return staticDockerContainers, removedIDs, changed, staticDockerContainerError
}
