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

//go:build !windows
// +build !windows

package helper

import (
	"io/fs"
	"os"
	"strings"
)

var DefaultLogtailMountPath string

func GetMountedFilePath(logPath string) string {
	return GetMountedFilePathWithBasePath(DefaultLogtailMountPath, logPath)
}

func GetMountedFilePathWithBasePath(basePath, logPath string) string {
	return basePath + logPath
}

func TryGetRealPath(path string) (string, fs.FileInfo) {
	sepLen := len(string(os.PathSeparator))
	if len(path) < sepLen {
		return "", nil
	}

	index := 0 // assume path is absolute
	for i := 0; i < 10; i++ {
		if f, err := os.Stat(path); err == nil {
			return path, f
		}
		for {
			j := strings.IndexRune(path[index+sepLen:], os.PathSeparator)
			if j == -1 {
				index = len(path)
			} else {
				index += j + sepLen
			}

			f, err := os.Lstat(path[:index])
			if err != nil {
				return "", nil
			}
			if f.Mode()&os.ModeSymlink != 0 {
				// path[:index] is a symlink
				target, _ := os.Readlink(path[:index])
				partialPath := GetMountedFilePath(target)
				path = partialPath + path[index:]
				if _, err := os.Stat(partialPath); err != nil {
					// path referenced by partialPath does not exist or has symlink
					index = 0
				} else {
					index = len(partialPath)
				}
				break
			}
		}
	}
	return "", nil
}

func init() {
	defaultPath := "/logtail_host"
	_, err := os.Stat(defaultPath)
	// only change default mount path when `/logtail_host` path exists.
	if err == nil {
		DefaultLogtailMountPath = defaultPath
	}
}
