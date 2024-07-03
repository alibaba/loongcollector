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

import "time"

var TestConfig Config

type Config struct {
	// Log
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
	AccessKeyID     string        `mapstructure:"access_key_id" yaml:"access_key_id"`
	AccessKeySecret string        `mapstructure:"access_key_secret" yaml:"access_key_secret"`
	Endpoint        string        `mapstructure:"endpoint" yaml:"endpoint"`
	Aliuid          string        `mapstructure:"aliuid" yaml:"aliuid"`
	QueryEndpoint   string        `mapstructure:"query_endpoint" yaml:"query_endpoint"`
	Region          string        `mapstructure:"region" yaml:"region"`
	RetryTimeout    time.Duration `mapstructure:"retry_timeout" yaml:"retry_timeout"`
}
