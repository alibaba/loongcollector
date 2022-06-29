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
	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/plugin_main/flags"

	"context"
	"runtime"
	"runtime/debug"
	"sync"
	"time"
)

// Following variables are exported so that tests of main package can reference them.
var LogtailConfig map[string]*LogstoreConfig
var LastLogtailConfig map[string]*LogstoreConfig

// Two built-in logtail configs to report statistics and alarm (from system and other logtail configs).
var StatisticsConfig *LogstoreConfig
var AlarmConfig *LogstoreConfig

// Configs that were disabled because of slow or hang config.
var DisabledLogtailConfigLock sync.Mutex
var DisabledLogtailConfig = make(map[string]*LogstoreConfig)

// StatisticsConfigJson, AlarmConfigJson
var BaseVersion = "0.1.0"

var statisticsConfigJSON = `{
    "global": {
        "InputIntervalMs" :  60000,
        "AggregatIntervalMs": 1000,
        "FlushIntervalMs": 1000,
        "DefaultLogQueueSize": 4,
		"DefaultLogGroupQueueSize": 4,
		"Tags" : {
			"base_version" : "0.1.0",
			"logtail_version" : "0.16.19"
		}
	},
	"inputs" : [
		{
			"type" : "metric_statistics",
			"detail" : null
		}
	]
}`

var alarmConfigJSON = `{
    "global": {
        "InputIntervalMs" :  30000,
        "AggregatIntervalMs": 1000,
        "FlushIntervalMs": 1000,
        "DefaultLogQueueSize": 4,
		"DefaultLogGroupQueueSize": 4,
		"Tags" : {
			"base_version" : "0.1.0",
			"logtail_version" : "0.16.19"
		}
    },
	"inputs" : [
		{
			"type" : "metric_alarm",
			"detail" : null
		}
	]
}`

func panicRecover(pluginName string) {
	if err := recover(); err != nil {
		trace := make([]byte, 2048)
		runtime.Stack(trace, true)
		logger.Error(context.Background(), "PLUGIN_RUNTIME_ALARM", "plugin", pluginName, "panicked", err, "stack", string(trace))
	}
}

// Init initializes plugin manager.
func Init() (err error) {
	logger.Info(context.Background(), "init plugin, local env tags", helper.EnvTags)

	if err = CheckPointManager.Init(); err != nil {
		return
	}
	if StatisticsConfig, err = loadBuiltinConfig("statistics", "sls-admin", "logtail_plugin_profile",
		"shennong_log_profile", statisticsConfigJSON); err != nil {
		logger.Error(context.Background(), "LOAD_PLUGIN_ALARM", "load statistics config fail", err)
		return
	}
	if AlarmConfig, err = loadBuiltinConfig("alarm", "sls-admin", "logtail_alarm",
		"logtail_alarm", alarmConfigJSON); err != nil {
		logger.Error(context.Background(), "LOAD_PLUGIN_ALARM", "load alarm config fail", err)
		return
	}
	return
}

// timeoutStop wrappers LogstoreConfig.Stop with timeout (5s by default).
// @return true if Stop returns before timeout, otherwise false.
func timeoutStop(config *LogstoreConfig, flag bool) bool {
	if !flag && config.GlobalConfig.AlwaysOnline {
		config.pause()
		GetAlwaysOnlineManager().AddCachedConfig(config, time.Duration(config.GlobalConfig.DelayStopSec)*time.Second)
		logger.Info(config.Context.GetRuntimeContext(), "Pause config and add into always online manager", "done")
		return true
	}

	done := make(chan int)
	go func() {
		logger.Info(config.Context.GetRuntimeContext(), "Stop config in goroutine", "begin")
		_ = config.Stop(flag)
		close(done)
		logger.Info(config.Context.GetRuntimeContext(), "Stop config in goroutine", "end")

		// The config is valid but stop slowly, allow it to load again.
		DisabledLogtailConfigLock.Lock()
		if _, exists := DisabledLogtailConfig[config.ConfigName]; !exists {
			DisabledLogtailConfigLock.Unlock()
			return
		}
		delete(DisabledLogtailConfig, config.ConfigName)
		DisabledLogtailConfigLock.Unlock()
		logger.Info(config.Context.GetRuntimeContext(), "Valid but slow stop config, enable it again", config.ConfigName)
	}()
	select {
	case <-done:
		return true
	case <-time.After(30 * time.Second):
		return false
	}
}

// HoldOn stops all config instance and checkpoint manager so that it is ready
//   to load new configs or quit.
// For user-defined config, timeoutStop is used to avoid hanging.
func HoldOn(exitFlag bool) error {
	defer panicRecover("Run plugin")

	for _, logstoreConfig := range LogtailConfig {
		if hasStopped := timeoutStop(logstoreConfig, exitFlag); !hasStopped {
			// TODO: This alarm can not be sent to server in current alarm design.
			logger.Error(logstoreConfig.Context.GetRuntimeContext(), "CONFIG_STOP_TIMEOUT_ALARM",
				"timeout when stop config, goroutine might leak")
			DisabledLogtailConfigLock.Lock()
			DisabledLogtailConfig[logstoreConfig.ConfigName] = logstoreConfig
			DisabledLogtailConfigLock.Unlock()
		}
	}
	if StatisticsConfig != nil {
		if *flags.ForceSelfCollect {
			logger.Info(context.Background(), "force collect the static metrics")
			for _, plugin := range StatisticsConfig.MetricPlugins {
				_ = plugin.Input.Collect(plugin)
			}
		}
		_ = StatisticsConfig.Stop(exitFlag)
	}
	if AlarmConfig != nil {
		if *flags.ForceSelfCollect {
			logger.Info(context.Background(), "force collect the alarm metrics")
			for _, plugin := range AlarmConfig.MetricPlugins {
				_ = plugin.Input.Collect(plugin)
			}
		}
		_ = AlarmConfig.Stop(exitFlag)
	}
	// clear all config
	LastLogtailConfig = LogtailConfig
	LogtailConfig = make(map[string]*LogstoreConfig)
	CheckPointManager.HoldOn()
	return nil
}

// Resume starts all configs.
func Resume() error {
	defer panicRecover("Run plugin")
	if StatisticsConfig != nil {
		StatisticsConfig.Start()
	}
	if AlarmConfig != nil {
		AlarmConfig.Start()
	}

	// Remove deleted configs from online manager.
	deletedCachedConfigs := GetAlwaysOnlineManager().GetDeletedConfigs(LogtailConfig)
	for _, cfg := range deletedCachedConfigs {
		go func(config *LogstoreConfig) {
			defer panicRecover(config.ConfigName)
			logger.Infof(config.Context.GetRuntimeContext(), "always online config %v is deleted, stop it", config.ConfigName)
			err := config.Stop(true)
			logger.Infof(config.Context.GetRuntimeContext(), "always online config %v stopped, error: %v", config.ConfigName, err)
		}(cfg)
	}

	for _, logstoreConfig := range LogtailConfig {
		if logstoreConfig.alreadyStarted {
			logstoreConfig.resume()
			continue
		}
		logstoreConfig.Start()
	}

	err := CheckPointManager.Init()
	if err != nil {
		logger.Error(context.Background(), "CHECKPOINT_INIT_ALARM", "init checkpoint manager error", err)
	}
	CheckPointManager.Resume()
	// clear last logtail config
	LastLogtailConfig = make(map[string]*LogstoreConfig)
	return nil
}

func init() {
	go func() {
		for {
			// force gc every 3 minutes
			time.Sleep(time.Minute * 3)
			logger.Debug(context.Background(), "force gc done", time.Now())
			runtime.GC()
			logger.Debug(context.Background(), "force gc done", time.Now())
			debug.FreeOSMemory()
			logger.Debug(context.Background(), "free os memory done", time.Now())
			if logger.DebugFlag() {
				gcStat := debug.GCStats{}
				debug.ReadGCStats(&gcStat)
				logger.Debug(context.Background(), "gc stats", gcStat)
				memStat := runtime.MemStats{}
				runtime.ReadMemStats(&memStat)
				logger.Debug(context.Background(), "mem stats", memStat)
			}
		}
	}()
}
