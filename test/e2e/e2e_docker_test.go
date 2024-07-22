// Copyright 2024 iLogtail Authors
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
package e2e

import (
	"testing"

	"github.com/cucumber/godog"
)

func TestE2EOnDockerCompose(t *testing.T) {
	suite := godog.TestSuite{
		Name:                "E2EOnDockerCompose",
		ScenarioInitializer: scenarioInitializer,
		Options: &godog.Options{
			Format:   "pretty",
			Paths:    []string{"test_cases"},
			Tags:     "@e2e && @docker-compose && ~@ebpf",
			TestingT: t,
		},
	}
	if suite.Run() != 0 {
		t.Fail()
	}
}

func TestE2EOnDockerComposeCore(t *testing.T) {
	suite := godog.TestSuite{
		Name:                "E2EOnDockerCompose",
		ScenarioInitializer: scenarioInitializer,
		Options: &godog.Options{
			Format:   "pretty",
			Paths:    []string{"test_cases"},
			Tags:     "@e2e-core && @docker-compose && ~@ebpf",
			TestingT: t,
		},
	}
	if suite.Run() != 0 {
		t.Fail()
	}
}

func TestE2EOnDockerComposePerformance(t *testing.T) {
	suite := godog.TestSuite{
		Name:                "E2EOnDockerCompose",
		ScenarioInitializer: scenarioInitializer,
		Options: &godog.Options{
			Format:   "pretty",
			Paths:    []string{"test_cases"},
			Tags:     "@e2e-performance && @docker-compose && ~@ebpf",
			TestingT: t,
		},
	}
	if suite.Run() != 0 {
		t.Fail()
	}
}
