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
	"flag"
	"fmt"
	"io/fs"
	"os"
	"strings"

	"github.com/alibaba/ilogtail/pkg/util"
)

// Used to collect text files in containers.
// Users need to mount the path expected to collect to driver C, so that
// Logtail container can access them from this path.
// For example, user container mounts host path C:\logs\ to container
// path C:\app\logs\, then Logtail can access this directory through
// C:\logtail_host\logs\. Therefore, user can set Logtail to collect
// text files under C:\app\logs\, such as C:\app\logs\nginx\.
// Type of mount:
// - Docker: -v option when run container.
// - Kubernetes: emptyDir volumn.
var DefaultLogtailMountPath string

const dockerInstallPathFlagName = "WINDOWS_DOCKER_INSTALL_PATH"
const dockerDataMountPathFlagName = "WINDOWS_DOCKER_DATA_MOUNT_PATH"

// DockerInstallPath and DockerDataMountPath are used for Windows that
// has multiple drivers. For such case, docker data can be saved on
// non-C driver (DockerInstallPath) and be mounted to driver C (DockerDataMountPath).
//
// Example:
// DockerInstallPath is E:\docker\, DockerDataMountPath is C:\ProgramData\docker\,
// user file path from docker engine is E:\docker\containers\<id>\<id>.json,
// Logtail can access it from C:\ProgramData\docker\containers\<id>\<id>.json,
// because DockerDataMountPath will be mounted into Logtail container with
// same path.
var DockerInstallPath = flag.String(dockerInstallPathFlagName, "", "")
var DockerDataMountPath = flag.String(dockerDataMountPathFlagName, "C:\\ProgramData\\docker", "")

func init() {
	util.InitFromEnvString(dockerInstallPathFlagName, DockerInstallPath, *DockerInstallPath)
	*DockerInstallPath = addPathSeparatorAtEnd(*DockerInstallPath)
	// Driver letter is removed from DockerInstallPath to support Kubernetes,
	// because different Nodes can install docker to different drivers.
	// eg. Node 1 installs docker in D:\docker\, Node 2 installs docker
	// in E:\docker\, both of them mount docker path to C:\ProgramData\docker,
	// so the actual driver letter is unnecessary.
	*DockerInstallPath = removeDriverLetter(*DockerInstallPath)

	util.InitFromEnvString(dockerDataMountPathFlagName, DockerDataMountPath, *DockerDataMountPath)
	*DockerDataMountPath = addPathSeparatorAtEnd(*DockerDataMountPath)

	defaultPath := "C:\\logtail_host"
	_, err := os.Stat(defaultPath)
	// only change default mount path when `C:\logtail_host` path exists.
	if err == nil {
		DefaultLogtailMountPath = defaultPath
	} else {
		DefaultLogtailMountPath = "C:"
	}
	fmt.Printf("running with mount path: %s", DefaultLogtailMountPath)
}

// addPathSeparatorAtEnd adds path separator at the end of file path.
func addPathSeparatorAtEnd(filePath string) string {
	if filePath == "" || isPathSeparator(filePath[len(filePath)-1]) {
		return filePath
	}
	return filePath + "\\"
}

// removeDriverLetter removes driver letter and its colon.
// Examples:
// - D:\ -> \
// - E:\docker\ -> \docker\
func removeDriverLetter(filePath string) string {
	if colonPos := strings.Index(filePath, ":"); colonPos != -1 {
		return filePath[colonPos+1:]
	}
	return filePath
}

func GetMountedFilePath(filePath string) string {
	// No specific installed path, use driver C.
	if *DockerInstallPath == "" {
		return GetMountedFilePathWithBasePath(DefaultLogtailMountPath, filePath)
	}
	// Installed path is not under driver C, remove prefix from filePath
	// and use mount path as base path.
	tempFilePath := removeDriverLetter(filePath)
	if strings.HasPrefix(tempFilePath, *DockerInstallPath) {
		return *DockerDataMountPath + tempFilePath[len(*DockerInstallPath):]
	}
	return filePath
}

func GetMountedFilePathWithBasePath(basePath, filePath string) string {
	if 0 == len(filePath) || 0 == len(basePath) {
		return ""
	}
	colonPos := strings.Index(filePath, ":")
	if -1 == colonPos {
		// Case: containerd, filePath is not start with driver letter. e.g. \\var\\logs\\pods\\xxx\\0.log.
		return filePath
	}
	return basePath + filePath[colonPos+1:]
}

func TryGetRealPath(path string) (string, fs.FileInfo) {
	if f, err := os.Stat(path); err == nil {
		return path, f
	}
	return "", nil
}
