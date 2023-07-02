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
	"context"
	"encoding/json"
	"fmt"
	"runtime"
	"sync"

	"github.com/alibaba/ilogtail/pkg/logger"
)

// GlobalConfig represents global configurations of plugin system.
type GlobalConfig struct {
	InputIntervalMs          int
	AggregatIntervalMs       int
	FlushIntervalMs          int
	DefaultLogQueueSize      int
	DefaultLogGroupQueueSize int
	Tags                     map[string]string
	// Directory to store logtail data, such as checkpoint, etc.
	LogtailSysConfDir string
	// Network identification from logtail.
	HostIP       string
	Hostname     string
	AlwaysOnline bool
	DelayStopSec int

	EnableTimestampNanosecond bool
}

// LogtailGlobalConfig is the singleton instance of GlobalConfig.
var LogtailGlobalConfig = newGlobalConfig()

// StatisticsConfigJson, AlarmConfigJson
var BaseVersion = "0.1.0"                                                  // will be overwritten through ldflags at compile time
var UserAgent = fmt.Sprintf("ilogtail/%v (%v)", BaseVersion, runtime.GOOS) // set in global config

var loadOnce sync.Once

// LoadGlobalConfig updates LogtailGlobalConfig according to jsonStr (only once).
func LoadGlobalConfig(jsonStr string) int {
	// Only the first call will return non-zero.
	rst := 0
	loadOnce.Do(func() {
		logger.Info(context.Background(), "load global config", jsonStr)
		if len(jsonStr) >= 2 { // For invalid JSON, use default value and return 0
			if err := json.Unmarshal([]byte(jsonStr), &LogtailGlobalConfig); err != nil {
				logger.Error(context.Background(), "LOAD_PLUGIN_ALARM", "load global config error", err)
				rst = 1
			}
			UserAgent = fmt.Sprintf("ilogtail/%v (%v) ip/%v", BaseVersion, runtime.GOOS, LogtailGlobalConfig.HostIP)
		}
	})
	return rst
}

func newGlobalConfig() (cfg GlobalConfig) {
	cfg = GlobalConfig{
		InputIntervalMs:          1000,
		AggregatIntervalMs:       3000,
		FlushIntervalMs:          3000,
		DefaultLogQueueSize:      1000,
		DefaultLogGroupQueueSize: 4,
		LogtailSysConfDir:        ".",
		DelayStopSec:             300,
	}
	return
}
