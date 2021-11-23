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

// +build windows

package helper

import (
	"testing"

	"github.com/stretchr/testify/require"
)

func testGetMountedFilePath(t *testing.T) {
	testCases := []struct {
		dockerInstallPath   string
		dockerDataMountPath string
		filePath            string
		outFilePath         string
	}{
		{
			dockerInstallPath:   "",
			dockerDataMountPath: "",
			filePath:            "C:\\Program File\\docker\\containers\\id\\id.json",
			outFilePath:         "C:\\logtail_host\\Program File\\docker\\containers\\id\\id.json",
		},
		{
			dockerInstallPath:   "\\",
			dockerDataMountPath: "C:\\docker",
			filePath:            "D:\\containers\\id\\id.json",
			outFilePath:         "C:\\docker\\containers\\id\\id.json",
		},
		{
			dockerInstallPath:   "\\any_path\\xxx\\",
			dockerDataMountPath: "C:\\path\\docker",
			filePath:            "E:\\any_path\\xxx\\containers\\id\\id.json",
			outFilePath:         "C:\\path\\docker\\containers\\id\\id.json",
		},
	}

	for _, c := range testCases {
		*DockerInstallPath = c.dockerInstallPath
		*DockerDataMountPath = c.dockerDataMountPath
		require.Equal(t, c.outFilePath, GetMountedFilePath(c.filePath))
	}
}
