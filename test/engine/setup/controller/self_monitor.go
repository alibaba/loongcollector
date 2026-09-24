// Copyright 2026 iLogtail Authors
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

package controller

import (
	"os"
	"path/filepath"
	"strings"

	"github.com/alibaba/ilogtail/test/config"
)

const e2eSelfMonitorConfigName = "e2e-self-monitor.yaml"

// e2eSelfMonitorConfig dumps all self-monitor categories once per minute to a
// host-mounted file so any docker-compose case can assert plugin_source labels.
const e2eSelfMonitorConfig = `enable: true
inputs:
  - Type: input_internal_metrics
    Agent:
      Enable: true
      Interval: 1
    Runner:
      Enable: true
      Interval: 1
    Pipeline:
      Enable: true
      Interval: 1
    Plugin:
      Enable: true
      Interval: 1
    Component:
      Enable: true
      Interval: 1
    PluginSource:
      Enable: true
      Interval: 1
flushers:
  - Type: flusher_file
    FilePath: /usr/local/loongcollector/self_monitor/self_metrics.log
`

func ensureE2ESelfMonitorConfig() error {
	if config.ConfigDir == "" {
		return nil
	}
	if err := os.MkdirAll(config.ConfigDir, 0750); err != nil {
		return err
	}
	if hasInputInternalMetricsConfig(config.ConfigDir) {
		return nil
	}
	return os.WriteFile(filepath.Join(config.ConfigDir, e2eSelfMonitorConfigName), []byte(e2eSelfMonitorConfig), 0600)
}

func hasInputInternalMetricsConfig(dir string) bool {
	entries, err := os.ReadDir(dir)
	if err != nil {
		return false
	}
	for _, entry := range entries {
		if entry.IsDir() {
			continue
		}
		name := entry.Name()
		if !strings.HasSuffix(name, ".yaml") && !strings.HasSuffix(name, ".yml") {
			continue
		}
		if name == e2eSelfMonitorConfigName {
			continue
		}
		content, err := os.ReadFile(filepath.Join(dir, name))
		if err != nil {
			continue
		}
		if strings.Contains(string(content), "input_internal_metrics") {
			return true
		}
	}
	return false
}
