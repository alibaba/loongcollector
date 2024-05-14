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

//go:build windows
// +build windows

package helper

import (
	"testing"

	"github.com/stretchr/testify/require"
)

func TestGetMountedFilePath(t *testing.T) {
	testCases := []struct {
		dockerInstallPath   string
		dockerDataMountPath string
		logtailMountPath    string
		filePath            string
		outFilePath         string
	}{
		{
			dockerInstallPath:   "",
			dockerDataMountPath: "",
			logtailMountPath:    "C:",
			filePath:            "C:\\Program File\\docker\\containers\\id\\id.json",
			outFilePath:         "C:\\Program File\\docker\\containers\\id\\id.json",
		},
		{
			dockerInstallPath:   "",
			dockerDataMountPath: "",
			logtailMountPath:    "C:\\logtail_host",
			filePath:            "C:\\Program File\\docker\\containers\\id\\id.json",
			outFilePath:         "C:\\logtail_host\\Program File\\docker\\containers\\id\\id.json",
		},
		{
			dockerInstallPath:   "\\",
			dockerDataMountPath: "C:\\docker",
			logtailMountPath:    "C:\\logtail_host",
			filePath:            "D:\\containers\\id\\id.json",
			outFilePath:         "C:\\docker\\containers\\id\\id.json",
		},
		{
			dockerInstallPath:   "\\any_path\\xxx\\",
			dockerDataMountPath: "C:\\path\\docker",
			logtailMountPath:    "C:\\logtail_host",
			filePath:            "E:\\any_path\\xxx\\containers\\id\\id.json",
			outFilePath:         "C:\\path\\docker\\containers\\id\\id.json",
		},
		// containerd
		{
			dockerInstallPath:   "",
			dockerDataMountPath: "",
			logtailMountPath:    "C:\\logtail_host",
			filePath:            "\\var\\logs\\pods\\xxx\\0.log",
			outFilePath:         "\\var\\logs\\pods\\xxx\\0.log",
		},
	}

	for _, c := range testCases {
		*DockerInstallPath = addPathSeparatorAtEnd(c.dockerInstallPath)
		*DockerDataMountPath = addPathSeparatorAtEnd(c.dockerDataMountPath)
		DefaultLogtailMountPath = c.logtailMountPath
		require.Equal(t, c.outFilePath, GetMountedFilePath(c.filePath))
	}

	require.Equal(t, NormalizeWindowsPath("C:\\var\\addon-logtail\\token-config"), "C:\\var\\addon-logtail\\token-config")
	require.Equal(t, NormalizeWindowsPath("/var/addon-logtail/token-config"), "C:\\var\\addon-logtail\\token-config")
	require.Equal(t, NormalizeWindowsPath(""), "")
}
