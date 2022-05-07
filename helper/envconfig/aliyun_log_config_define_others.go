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

package envconfig

import (
	"errors"
	"strings"
)

const invalidLogPath = "/invalid_log_path"

func splitLogPathAndFilePattern(filePath string) (logPath string, filePattern string, err error) {
	lastSeperatorPos := strings.LastIndexByte(filePath, '/')
	if lastSeperatorPos <= 0 || len(filePath) < 2 || lastSeperatorPos == len(filePath)-1 {
		logPath = invalidLogPath
		filePattern = invalidFilePattern
		err = errors.New("invalid Unix file path")
	} else {
		logPath = filePath[0 : lastSeperatorPos+1]
		filePattern = filePath[lastSeperatorPos+1:]
	}
	return
}
