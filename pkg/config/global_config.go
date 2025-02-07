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
	"fmt"
	"runtime"
)

// GlobalConfig represents global configurations of plugin system.
type GlobalConfig struct {
	InputMaxFirstCollectDelayMs int // 10s by default, If InputMaxFirstCollectDelayMs is greater than interval, it will use interval instead.
	InputIntervalMs             int
	AggregatIntervalMs          int
	FlushIntervalMs             int
	DefaultLogQueueSize         int
	DefaultLogGroupQueueSize    int
	// Directory to store prometheus configuration file.
	LoongCollectorPrometheusAuthorizationPath string
	// Directory to store loongcollector data, such as checkpoint, etc.
	LoongCollectorConfDir string
	// Directory to store loongcollector log config.
	LoongCollectorLogConfDir string
	// Directory to store loongcollector log.
	LoongCollectorLogDir string
	// Directory to store loongcollector data.
	LoongCollectorDataDir string
	// Directory to store loongcollector debug data.
	LoongCollectorDebugDir string
	// Directory to store loongcollector third party data.
	LoongCollectorThirdPartyDir string
	// Log name of loongcollector plugin.
	LoongCollectorPluginLogName string
	// Tag of loongcollector version.
	LoongCollectorVersionTag string
	// Checkpoint dir name of loongcollector plugin.
	LoongCollectorGoCheckPointDir string
	// Checkpoint file name of loongcollector plugin.
	LoongCollectorGoCheckPointFile string
	// Network identification from loongcollector.
	HostIP       string
	Hostname     string
	DelayStopSec int

	EnableTimestampNanosecond bool
	UsingOldContentTag        bool
	EnableSlsMetricsFormat    bool
	EnableProcessorTag        bool

	PipelineMetaTagKey     map[string]string
	AppendingAllEnvMetaTag bool
	AgentEnvMetaTagKey     map[string]string
}

// LoongcollectorGlobalConfig is the singleton instance of GlobalConfig.
var LoongcollectorGlobalConfig = newGlobalConfig()

// StatisticsConfigJson, AlarmConfigJson
var BaseVersion = "0.1.0"                                                  // will be overwritten through ldflags at compile time
var UserAgent = fmt.Sprintf("ilogtail/%v (%v)", BaseVersion, runtime.GOOS) // set in global config

func newGlobalConfig() (cfg GlobalConfig) {
	cfg = GlobalConfig{
		InputMaxFirstCollectDelayMs:               10000, // 10s
		InputIntervalMs:                           1000,  // 1s
		AggregatIntervalMs:                        3000,
		FlushIntervalMs:                           3000,
		DefaultLogQueueSize:                       1000,
		DefaultLogGroupQueueSize:                  4,
		LoongCollectorConfDir:                     "./conf/",
		LoongCollectorLogConfDir:                  "./conf/",
		LoongCollectorLogDir:                      "./log/",
		LoongCollectorPluginLogName:               "go_plugin.LOG",
		LoongCollectorVersionTag:                  "loongcollector_version",
		LoongCollectorGoCheckPointDir:             "./data/",
		LoongCollectorGoCheckPointFile:            "go_plugin_checkpoint",
		LoongCollectorDataDir:                     "./data/",
		LoongCollectorDebugDir:                    "./debug/",
		LoongCollectorThirdPartyDir:               "./thirdparty/",
		LoongCollectorPrometheusAuthorizationPath: "./conf/",
		DelayStopSec:                              300,
	}
	return
}
