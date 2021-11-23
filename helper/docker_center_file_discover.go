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
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"

	"context"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"

	docker "github.com/fsouza/go-dockerclient"
)

const staticContainerInfoPathEnvKey = "ALIYUN_LOG_STATIC_CONTAINER_INFO"

// const staticContainerType = "ALIYUN_LOG_STATIC_CONTAINER_TYPE"
// const staticContainerDScanInterval = "ALIYUN_LOG_STATIC_CONTAINED_SCAN_INTERVAL"
// const staticContainerTypeContainerD = "containerd"

var containerdScanIntervalMs = 1000
var staticDockerContianer []*docker.Container
var staticDockerContianerError error
var staticDockerContianerLock sync.Mutex
var loadStaticContainerOnce sync.Once
var staticDockerContainerLastStat StateOS
var staticDockerContianerFile string
var staticDockerContainerLastBody string

type staticContainerMount struct {
	Source      string
	Destination string
	Driver      string
}

type staticContainerInfo struct {
	ID       string
	Name     string
	HostName string
	IP       string
	Image    string
	LogPath  string
	Labels   map[string]string
	LogType  string
	UpperDir string
	Env      map[string]string
	Mounts   []staticContainerMount
}

func staticContainerInfoToStandard(staticInfo *staticContainerInfo) *docker.Container {
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

	dockerContainer := &docker.Container{
		ID:      staticInfo.ID,
		Name:    staticInfo.Name,
		LogPath: staticInfo.LogPath,
		Config: &docker.Config{
			Labels:   staticInfo.Labels,
			Image:    staticInfo.Image,
			Env:      allEnv,
			Hostname: staticInfo.HostName,
		},
		HostConfig: &docker.HostConfig{
			LogConfig: docker.LogConfig{
				Type: staticInfo.LogType,
			},
		},
		GraphDriver: &docker.GraphDriver{
			Name: "overlay",
			Data: map[string]string{
				"UpperDir": staticInfo.UpperDir,
			},
		},
		NetworkSettings: &docker.NetworkSettings{
			IPAddress: staticInfo.IP,
		},
	}

	for _, mount := range staticInfo.Mounts {
		dockerContainer.Mounts = append(dockerContainer.Mounts, docker.Mount{
			Source:      mount.Source,
			Destination: mount.Destination,
			Driver:      mount.Driver,
		})
	}
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
		dir, err := ioutil.ReadDir(dirPath)
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

func innerReadStatisContainerInfo(file string, lastContainerInfo []*docker.Container) (containers []*docker.Container, removed []string, changed bool, err error) {
	body, err := ioutil.ReadFile(filepath.Clean(file))
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
		containers = append(containers, staticContainerInfoToStandard(&staticContainerInfos[i]))
		nowIDs[staticContainerInfos[i].ID] = struct{}{}
	}

	for _, lastContainer := range lastContainerInfo {
		if _, ok := nowIDs[lastContainer.ID]; !ok {
			removed = append(removed, lastContainer.ID)
		}
	}
	logger.Info(context.Background(), "read static container info", staticContainerInfos)
	return containers, removed, changed, err
}

func isStaticContainerInfoEnabled() bool {
	envPath := os.Getenv(staticContainerInfoPathEnvKey)
	return len(envPath) != 0
}

func tryReadStaticContainerInfo() ([]*docker.Container, []string, bool, error) {
	loadStaticContainerOnce.Do(
		func() {
			envPath := os.Getenv(staticContainerInfoPathEnvKey)
			if len(envPath) == 0 {
				return
			}
			staticDockerContianerFile = envPath
			staticDockerContianer, _, _, staticDockerContianerError = innerReadStatisContainerInfo(staticDockerContianerFile, nil)
			stat, err := os.Stat(staticDockerContianerFile)
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

	if staticDockerContianerFile == "" {
		return nil, nil, false, nil
	}

	staticDockerContianerLock.Lock()
	defer staticDockerContianerLock.Unlock()

	stat, err := os.Stat(staticDockerContianerFile)
	if err != nil {
		return staticDockerContianer, nil, false, staticDockerContianerError
	}

	osStat := GetOSState(stat)
	if !osStat.IsChange(staticDockerContainerLastStat) && staticDockerContianerError == nil {
		return staticDockerContianer, nil, false, nil
	}

	var removedIDs []string
	var changed bool

	staticDockerContianer, removedIDs, changed, staticDockerContianerError = innerReadStatisContainerInfo(staticDockerContianerFile, staticDockerContianer)

	if staticDockerContianerError == nil {
		staticDockerContainerLastStat = osStat
	}

	logger.Info(context.Background(), "static docker container info file changed or last read error, last", staticDockerContainerLastStat, "now", osStat, "removed ids", removedIDs, "error", staticDockerContianerError)

	return staticDockerContianer, removedIDs, changed, staticDockerContianerError
}
