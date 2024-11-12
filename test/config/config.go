// Copyright 2021 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
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
	"strconv"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
)

var TestConfig Config

type Config struct {
	// Log
	LocalConfigDir  string `mapstructure:"local_config_dir" yaml:"local_config_dir"`
	GeneratedLogDir string `mapstructure:"generated_log_dir" yaml:"generated_log_dir"`
	WorkDir         string `mapstructure:"work_dir" yaml:"work_dir"`
	// Host
	SSHUsername string `mapstructure:"ssh_username" yaml:"ssh_username"`
	SSHIP       string `mapstructure:"ssh_ip" yaml:"ssh_ip"`
	SSHPassword string `mapstructure:"ssh_password" yaml:"ssh_password"`
	// K8s
	KubeConfigPath string `mapstructure:"kube_config_path" yaml:"kube_config_path"`
	// docker compose
	Profile          bool     `mapstructure:"profile" yaml:"profile"`
	CoveragePackages []string `mapstructure:"coverage_packages" yaml:"coverage_packages"`
	// SLS
	Project         string        `mapstructure:"project" yaml:"project"`
	Logstore        string        `mapstructure:"logstore" yaml:"logstore"`
	MetricStore     string        `mapstructure:"metric_store" yaml:"metric_store"`
	AccessKeyID     string        `mapstructure:"access_key_id" yaml:"access_key_id"`
	AccessKeySecret string        `mapstructure:"access_key_secret" yaml:"access_key_secret"`
	Endpoint        string        `mapstructure:"endpoint" yaml:"endpoint"`
	Aliuid          string        `mapstructure:"aliuid" yaml:"aliuid"`
	Region          string        `mapstructure:"region" yaml:"region"`
	RetryTimeout    time.Duration `mapstructure:"retry_timeout" yaml:"retry_timeout"`
}

func (s *Config) GetLogstore(telemetryType string) string {
	if s != nil && telemetryType == "metrics" {
		return s.MetricStore
	}
	return s.Logstore
}

func ParseConfig() {
	loggerOptions := []logger.ConfigOption{
		logger.OptionAsyncLogger,
	}
	loggerOptions = append(loggerOptions, logger.OptionInfoLevel)
	logger.InitTestLogger(loggerOptions...)

	TestConfig = Config{}
	// Log
	TestConfig.LocalConfigDir = os.Getenv("LOCAL_CONFIG_DIR")
	TestConfig.GeneratedLogDir = os.Getenv("GENERATED_LOG_DIR")
	if len(TestConfig.GeneratedLogDir) == 0 {
		TestConfig.GeneratedLogDir = "/tmp/loongcollector"
	}
	TestConfig.WorkDir = os.Getenv("WORK_DIR")
	if len(TestConfig.WorkDir) == 0 {
		testFileDir, _ := os.Getwd()
		TestConfig.WorkDir = filepath.Dir(testFileDir)
	}

	// SSH
	TestConfig.SSHUsername = os.Getenv("SSH_USERNAME")
	TestConfig.SSHIP = os.Getenv("SSH_IP")
	TestConfig.SSHPassword = os.Getenv("SSH_PASSWORD")

	// K8s
	TestConfig.KubeConfigPath = os.Getenv("KUBE_CONFIG_PATH")

	// SLS
	TestConfig.Project = os.Getenv("PROJECT")
	TestConfig.Logstore = os.Getenv("LOGSTORE")
	TestConfig.MetricStore = os.Getenv("METRIC_STORE")
	TestConfig.AccessKeyID = os.Getenv("ACCESS_KEY_ID")
	TestConfig.AccessKeySecret = os.Getenv("ACCESS_KEY_SECRET")
	TestConfig.Endpoint = os.Getenv("ENDPOINT")
	TestConfig.Aliuid = os.Getenv("ALIUID")
	TestConfig.Region = os.Getenv("REGION")
	timeout, err := strconv.ParseInt(os.Getenv("RETRY_TIMEOUT"), 10, 64)
	if err != nil {
		timeout = 60
	}
	TestConfig.RetryTimeout = time.Duration(timeout) * time.Second
}

func GetQueryEndpoint() string {
	idx := strings.Index(TestConfig.Endpoint, "-intranet")
	if idx == -1 {
		return TestConfig.Endpoint
	}
	return TestConfig.Endpoint[:idx] + TestConfig.Endpoint[idx+9:]
}
