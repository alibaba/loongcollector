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
	"bytes"
	"context"
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"

	"github.com/spf13/viper"

	"github.com/alibaba/ilogtail/pkg/logger"
)

// The user defined configuration files.
const (
	ConfigFileName        = "ilogtail-e2e.yaml"
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
	ConfigFile    string
	LogDir        string
)

// Load E2E engine config and define the global variables.
func Load(path string, profile bool) (*Case, error) {
	abs, err := filepath.Abs(path)
	if err != nil {
		return nil, err
	}
	CaseHome = abs + "/"
	CaseName = abs[strings.LastIndex(abs, "/")+1:]

	c, err := load(CaseHome + ConfigFileName)
	if err != nil {
		panic(fmt.Errorf("cannot load config from %s, got error: %v", CaseHome, err))
	}
	root, _ := filepath.Abs(".")
	reportDir := root + "/report/"
	_ = os.Mkdir(reportDir, 0750)
	EngineLogFile = reportDir + CaseName + "_engine.log"
	CoverageFile = reportDir + CaseName + "_coverage.out"
	ReportFile = reportDir + CaseName + "_report.json"
	PprofDir = reportDir + CaseName + "_pprof"
	LogDir = reportDir + CaseName + "_log"
	ProfileFlag = profile

	FlusherFile = reportDir + CaseName + "default_flusher.json"
	ConfigFile = reportDir + CaseName + "config.json"
	return c, nil
}

func load(path string) (*Case, error) {
	logger.Infof(context.Background(), "load config from: %s", path)
	content, err := ioutil.ReadFile(filepath.Clean(path))
	if err != nil {
		return nil, err
	}
	v := viper.New()
	v.SetConfigType("yaml")
	cfg := GetDefaultCase()
	if err := v.ReadConfig(bytes.NewReader(content)); err != nil {
		return nil, err
	}
	if err := v.Unmarshal(cfg); err != nil {
		return nil, err
	}
	return cfg, nil
}
