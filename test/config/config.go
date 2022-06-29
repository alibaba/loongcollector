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

// Case declares a procedure for testing a case.
type Case struct {
	Boot             Boot       `mapstructure:"boot" yaml:"boot"`
	SetUps           []SetUp    `mapstructure:"setups" yaml:"setups"`
	Ilogtail         Ilogtail   `mapstructure:"ilogtail" yaml:"ilogtail"`
	Subscriber       Subscriber `mapstructure:"subscriber" yaml:"subscriber"`
	Trigger          Trigger    `mapstructure:"trigger" yaml:"trigger"`
	Verify           Verify     `mapstructure:"verify" yaml:"verify"`
	Retry            Retry      `mapstructure:"retry" yaml:"retry"`
	TestingInterval  string     `mapstructure:"testing_interval" yaml:"testing_interval"`
	CoveragePackages []string   `mapstructure:"coverage_packages" yaml:"coverage_packages"`
}

type (
	// Boot the dependency virtual environment, such as docker compose.
	Boot struct {
		Category string `mapstructure:"category" yaml:"category"`
		Timeout  string `mapstructure:"timeout" yaml:"timeout"`
	}
	// SetUp prepares the test environment after the virtual environment started, such as installing some software.
	SetUp struct {
		Name    string `mapstructure:"name" yaml:"name"`
		Command string `mapstructure:"command" yaml:"command"`
	}
	// Ilogtail is the logtail plugin configuration to load the ilogtail context.
	Ilogtail struct {
		Config         []LogtailCfgs          `mapstructure:"config" yaml:"config"`
		LoadConfigWait string                 `mapstructure:"load_config_wait" yaml:"load_config_wait"`
		CloseWait      string                 `mapstructure:"close_wait" yaml:"close_wait"` // wait interval for upstream process.
		ENV            map[string]string      `mapstructure:"env" yaml:"env"`
		DependsOn      map[string]interface{} `mapstructure:"depends_on" yaml:"depends_on"`
		MountFiles     []string               `mapstructure:"mounts" yaml:"mounts"`
	}

	LogtailCfgs struct {
		Name      string   `mapstructure:"name" yaml:"name"`
		MixedMode bool     `mapstructure:"mixed_mode" yaml:"mixed_mode"`
		Content   []string `mapstructure:"content" yaml:"content"`
	}

	Subscriber struct {
		Name   string                 `mapstructure:"name" yaml:"name"`
		Config map[string]interface{} `mapstructure:"config" yaml:"config"`
	}
	// Trigger triggers the custom HTTP endpoint to create some telemetry data.
	Trigger struct {
		URL      string `mapstructure:"url" yaml:"url"`
		Method   string `mapstructure:"method" yaml:"method"`
		Interval string `mapstructure:"interval" yaml:"interval"`
		Times    int    `mapstructure:"times" yaml:"times"`
	}
	// Verify content transferred to the mock backend to verify the collected telemetry data.
	Verify struct {
		LogRules    []Rule `mapstructure:"log_rules" yaml:"log_rules"`
		SystemRules []Rule `mapstructure:"system_rules" yaml:"system_rules"`
	}
	// Retry triggered when failing.
	Retry struct {
		Times    int    `mapstructure:"times" yaml:"times"`
		Interval string `mapstructure:"interval" yaml:"interval"`
	}
	Rule struct {
		Name      string                 `mapstructure:"name" yaml:"name"`
		Validator string                 `mapstructure:"validator" yaml:"validator"`
		Spec      map[string]interface{} `mapstructure:"spec" yaml:"spec"`
	}
)

func GetDefaultCase() *Case {
	return &Case{
		TestingInterval: "60s",
		Retry: Retry{
			Times:    0,
			Interval: "10s",
		},
		Subscriber: Subscriber{
			Name: "grpc",
			Config: map[string]interface{}{
				"address": ":8000",
				"network": "tcp",
			},
		},
		Ilogtail: Ilogtail{
			CloseWait:      "10s",
			LoadConfigWait: "5s",
		},
		Boot: Boot{
			Timeout: "60s",
		},
	}
}
