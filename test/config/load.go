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
	"path/filepath"
	"strings"
)

// The user defined configuration files.
const (
	DockerComposeFileName = "docker-compose.yaml"
)

var (
	CaseHome      string
	CaseName      string
	EngineLogFile string
	ReportFile    string
	PprofDir      string
	ProfileFlag   bool
	CoverageFile  string
	FlusherFile   string
	ConfigDir     string
	LogDir        string
)

// Load E2E engine config and define the global variables.
func Load(path string, profile bool) error {
	abs, err := filepath.Abs(path)
	if err != nil {
		return err
	}
	CaseHome = abs + "/"
	CaseName = abs[strings.LastIndex(abs, "/")+1:]

	root, _ := filepath.Abs(".")
	reportDir := root + "/report/"
	EngineLogFile = reportDir + CaseName + "_engine.log"
	CoverageFile = reportDir + CaseName + "_coverage.out"
	ReportFile = reportDir + CaseName + "_report.json"
	PprofDir = reportDir + CaseName + "_pprof"
	LogDir = reportDir + CaseName + "_log"
	ProfileFlag = profile

	FlusherFile = reportDir + CaseName + "default_flusher.json"
	return nil
}
