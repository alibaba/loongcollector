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
	"os"
	"path/filepath"
	"runtime"
	"strings"
)

var windowsSystemDrive = mustGetWindowsSystemDrive()

// mustGetWindowsSystemDrive try to find the system drive on Windows.
func mustGetWindowsSystemDrive() string {
	if runtime.GOOS != "windows" {
		return ""
	}
	var systemDrive = os.Getenv("SYSTEMDRIVE")
	if systemDrive == "" {
		systemDrive = filepath.VolumeName(os.Getenv("SYSTEMROOT"))
	}
	if systemDrive == "" {
		systemDrive = "C:"
	}
	return systemDrive
}

// NormalizeWindowsPath returns the normal path in heterogeneous platform.
// parses the root path with windows system driver.
func NormalizeWindowsPath(path string) string {
	if runtime.GOOS == "windows" {
		// parses the root path with windows system driver.
		if strings.HasPrefix(path, "/") {
			path = filepath.FromSlash(path)
			return windowsSystemDrive + path
		}
	}
	return path
}
