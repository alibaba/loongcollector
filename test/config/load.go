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

package config

import (
	"os"
	"path/filepath"
	"strings"
)

// The user defined configuration files.
const (
	DockerComposeFileName = "docker-compose.yaml"
)

var (
	CaseHome         string
	CaseName         string
	EngineLogFile    string
	FlusherFile      string
	ConfigDir        string
	OnetimeConfigDir string
	LogDir           string
)

// Load E2E engine config and define the global variables.
func Load(path string) error {
	abs, err := filepath.Abs(path)
	if err != nil {
		return err
	}
	CaseHome = abs + "/"
	CaseName = abs[strings.LastIndex(abs, "/")+1:]

	root, _ := filepath.Abs(".")
	reportDir := root + "/report/"
	EngineLogFile = reportDir + CaseName + "_engine.log"
	LogDir = reportDir + CaseName + "_log"

	if mkErr := os.MkdirAll(reportDir, 0o750); mkErr != nil {
		return mkErr
	}

	FlusherFile = reportDir + CaseName + "_default_flusher.json"

	if _, statErr := os.Stat(FlusherFile); os.IsNotExist(statErr) {
		if writeErr := os.WriteFile(FlusherFile, []byte("{\"type\":\"flusher_stdout\",\"detail\":{}}\n"), 0o600); writeErr != nil {
			return writeErr
		}
	} else if statErr != nil {
		return statErr
	}
	return nil
}
