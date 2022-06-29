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

// DemoCase is only for generate ilogtail-e2e-demo.yaml.
var DemoCase = Case{
	Boot: Boot{
		Category: "docker-compose",
		Timeout:  "60s",
	},
	Ilogtail: Ilogtail{
		LoadConfigWait: "5s",
		CloseWait:      "5s",
		Config: []LogtailCfgs{
			{
				Name: "mock-metric-case",
				Content: []string{
					`{"inputs":[{"type":"metric_mock","detail":{"IntervalMs": 1000,"Tags":{"tag1":"aaaa","tag2":"bbb"},"Fields":{"content":"xxxxx","time":"2017.09.12 20:55:36"}}}]}`,
				},
			},
		},
		MountFiles: []string{},
	},
	SetUps: []SetUp{},
	Retry: Retry{
		Times:    0,
		Interval: "10s",
	},
	Verify: Verify{
		LogRules: []Rule{
			{
				Name:      "fields-check",
				Validator: "log_fields",
				Spec: map[string]interface{}{
					"expect_log_fields": []string{"tag1", "tag2", "content", "time"},
				},
			},
		},
		SystemRules: []Rule{
			{
				Name:      "counter-check",
				Validator: "sys_counter",
				Spec: map[string]interface{}{
					"expect_equal_raw_log":       true,
					"expect_equal_processed_log": true,
					"expect_equal_flush_log":     true,
					"expect_minimum_log_num":     15,
				},
			},
		},
	},
	TestingInterval: "16s",
	Subscriber: Subscriber{
		Name: "grpc",
		Config: map[string]interface{}{
			"address": ":9000",
			"network": "tcp",
		},
	},
	Trigger: Trigger{
		URL:      "http://ilogtail:18689/test",
		Method:   "GET",
		Interval: "1s",
		Times:    10,
	},
	CoveragePackages: []string{
		"ilogtail/pluginmanager",
		"ilogtail/main",
	},
}
