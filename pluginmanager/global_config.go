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

package pluginmanager

import (
	"context"
	"encoding/json"
	"runtime"
	"sync"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"
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
}

// LogtailGlobalConfig is the singleton instance of GlobalConfig.
var LogtailGlobalConfig = newGlobalConfig()

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
			} else {
				// Update when both of them are not empty.
				logger.Debugf(context.Background(), "host IP: %v, hostname: %v",
					LogtailGlobalConfig.HostIP, LogtailGlobalConfig.Hostname)
				if len(LogtailGlobalConfig.Hostname) > 0 && len(LogtailGlobalConfig.HostIP) > 0 {
					util.SetNetworkIdentification(LogtailGlobalConfig.HostIP, LogtailGlobalConfig.Hostname)
				}
			}
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
		DelayStopSec:             300,
	}
	if runtime.GOOS == "windows" {
		cfg.LogtailSysConfDir = "C:\\LogtailData"
	} else {
		cfg.LogtailSysConfDir = "/etc/ilogtail"
	}
	return
}
