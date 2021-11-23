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

package main

import (
	"context"
	"runtime/debug"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/main/flags"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/pluginmanager"
)
import "C"

var initOnce sync.Once
var started bool

//export InitPluginBase
func InitPluginBase() int {
	return initPluginBase("")
}

//export InitPluginBaseV2
func InitPluginBaseV2(cfgStr string) int {
	return initPluginBase(cfgStr)
}

//export LoadGlobalConfig
func LoadGlobalConfig(jsonStr string) int {
	return pluginmanager.LoadGlobalConfig(jsonStr)
}

//export LoadConfig
func LoadConfig(project string, logstore string, configName string, logstoreKey int64, jsonStr string) int {
	logger.Debug(context.Background(), "load config", configName, logstoreKey, jsonStr)
	if started {
		logger.Error(context.Background(), "CONFIG_LOAD_ALARM", "cannot load config before hold on the running configs")
		return 1
	}
	err := pluginmanager.LoadLogstoreConfig(util.StringDeepCopy(project),
		util.StringDeepCopy(logstore), util.StringDeepCopy(configName),
		// Make deep copy if you want to save it in Go in the future.
		logstoreKey, jsonStr)
	if err != nil {
		logger.Error(context.Background(), "CONFIG_LOAD_ALARM", "load config error, project",
			project, "logstore", logstore, "config", configName, "error", err)
		return 1
	}
	return 0
}

//export UnloadConfig
func UnloadConfig(project string, logstore string, configName string) int {
	logger.Debug(context.Background(), "unload config", configName)
	return 0
}

//export ProcessRawLog
func ProcessRawLog(configName string, rawLog []byte, packId string, topic string) int {
	plugin, flag := pluginmanager.LogtailConfig[configName]
	if !flag {
		return -1
	} else {
		// rawLog will be copied when it is converted to string, packId and topic
		// are unused now, so deep copy is unnecessary.
		return plugin.ProcessRawLog(rawLog, packId, topic)
	}
}

//export ProcessRawLogV2
func ProcessRawLogV2(configName string, rawLog []byte, packId string, topic string, tags []byte) int {
	config, exists := pluginmanager.LogtailConfig[configName]
	if !exists {
		return -1
	}
	return config.ProcessRawLogV2(rawLog, packId, util.StringDeepCopy(topic), tags)
}

//export HoldOn
func HoldOn(exitFlag int) {
	logger.Info(context.Background(), "Hold on", "start", "flag", exitFlag)
	if started {
		err := pluginmanager.HoldOn(exitFlag != 0)
		if err != nil {
			logger.Error(context.Background(), "PLUGIN_ALARM", "hold on error", err)
		}
	}
	started = false
	logger.Info(context.Background(), "Hold on", "success")
}

//export Resume
func Resume() {
	logger.Info(context.Background(), "Resume", "start")
	if !started {
		err := pluginmanager.Resume()
		if err != nil {
			logger.Error(context.Background(), "PLUGIN_ALARM", "resume error", err)
		}
	}
	started = true
	logger.Info(context.Background(), "Resume", "success")
}

//export CtlCmd
func CtlCmd(configName string, cmdId int, cmdDetail string) {
	logger.Info(context.Background(), "execute cmd", cmdId, "detail", cmdDetail, "config", configName)
}

func initPluginBase(cfgStr string) int {
	// Only the first call will return non-zero.
	rst := 0
	initOnce.Do(func() {
		logger.Init()
		flags.OverrideByEnv()
		InitHTTPServer()
		setGCPercentForSlowStart()
		logger.Info(context.Background(), "init plugin base, version", pluginmanager.BaseVersion)
		LoadGlobalConfig(cfgStr)
		if err := pluginmanager.Init(); err != nil {
			logger.Error(context.Background(), "PLUGIN_ALARM", "init plugin error", err)
			rst = 1
		}
	})
	return rst
}

// setGCPercentForSlowStart sets GC percent with a small value at startup
// to avoid high RSS (caused by data catch-up) to trigger OOM-kill.
func setGCPercentForSlowStart() {
	gcPercent := 20
	_ = util.InitFromEnvInt("ALIYUN_LOGTAIL_GOLANG_GC_PERCENT", &gcPercent, gcPercent)
	defaultGCPercent := debug.SetGCPercent(gcPercent)
	logger.Infof(context.Background(), "set startup GC percent from %v to %v", defaultGCPercent, gcPercent)
	resumeSeconds := 5 * 60
	_ = util.InitFromEnvInt("ALIYUN_LOGTAIL_GOLANG_GC_PERCENT_RESUME_SEC", &resumeSeconds, resumeSeconds)
	go func(pc int, sec int) {
		time.Sleep(time.Second * time.Duration(sec))
		last := debug.SetGCPercent(pc)
		logger.Infof(context.Background(), "resume GC percent from %v to %v", last, pc)
	}(defaultGCPercent, resumeSeconds)
}
