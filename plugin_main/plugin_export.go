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
	"encoding/json"
	"fmt"
	"runtime"
	"runtime/debug"
	"sync"
	"time"
	"unsafe"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugin_main/flags"
	"github.com/alibaba/ilogtail/pluginmanager"
)

/*
#include <stdlib.h>
static char**makeCharArray(int size) {
        return malloc(sizeof(char*) * size);
}

static void setArrayString(char **a, char *s, int n) {
        a[n] = s;
}

struct containerMeta{
	char* podName;
	char* k8sNamespace;
	char* containerName;
	char* image;
	int k8sLabelsSize;
	int containerLabelsSize;
	int envSize;
	char** k8sLabelsKey;
	char** k8sLabelsVal;
	char** containerLabelsKey;
	char** containerLabelsVal;
	char** envsKey;
	char** envsVal;
};

typedef struct {
    char* key;
    char* value;
} KeyValue;

typedef struct {
    KeyValue** keyValues;
    int count;
} PluginMetric;

typedef struct {
    PluginMetric** metrics;
    int count;
} PluginMetrics;

static KeyValue** makeKeyValueArray(int size) {
    return malloc(sizeof(KeyValue*) * size);
}

static void setArrayKeyValue(KeyValue **a, KeyValue *s, int n) {
    a[n] = s;
}

static PluginMetric** makePluginMetricArray(int size) {
    return malloc(sizeof(KeyValue*) * size);
}

static void setArrayPluginMetric(PluginMetric **a, PluginMetric *s, int n) {
    a[n] = s;
}
*/
import "C" //nolint:typecheck

var initOnce sync.Once
var loadOnce sync.Once
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
	// Only the first call will return non-zero.
	retcode := 0
	loadOnce.Do(func() {
		logger.Info(context.Background(), "load global config", jsonStr)
		if len(jsonStr) >= 2 { // For invalid JSON, use default value and return 0
			if err := json.Unmarshal([]byte(jsonStr), &config.LogtailGlobalConfig); err != nil {
				logger.Error(context.Background(), "LOAD_PLUGIN_ALARM", "load global config error", err)
				retcode = 1
			}
			config.UserAgent = fmt.Sprintf("ilogtail/%v (%v) ip/%v", config.BaseVersion, runtime.GOOS, config.LogtailGlobalConfig.HostIP)
		}
	})
	if retcode == 0 {
		// Update when both of them are not empty.
		logger.Debugf(context.Background(), "host IP: %v, hostname: %v",
			config.LogtailGlobalConfig.HostIP, config.LogtailGlobalConfig.Hostname)
		if len(config.LogtailGlobalConfig.Hostname) > 0 && len(config.LogtailGlobalConfig.HostIP) > 0 {
			util.SetNetworkIdentification(config.LogtailGlobalConfig.HostIP, config.LogtailGlobalConfig.Hostname)
		}
	}
	return retcode
}

//export LoadConfig
func LoadConfig(project string, logstore string, configName string, logstoreKey int64, jsonStr string) int {
	logger.Debug(context.Background(), "load config", configName, logstoreKey, "\n"+jsonStr)
	if started {
		logger.Error(context.Background(), "CONFIG_LOAD_ALARM", "cannot load config before hold on the running configs")
		return 1
	}
	defer func() {
		if err := recover(); err != nil {
			trace := make([]byte, 2048)
			runtime.Stack(trace, true)
			logger.Error(context.Background(), "PLUGIN_RUNTIME_ALARM", "panicked", err, "stack", string(trace))
		}
	}()

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
func ProcessRawLog(configName string, rawLog []byte, packID string, topic string) int {
	plugin, flag := pluginmanager.LogtailConfig[configName]
	if !flag {
		return -1
	}

	// rawLog will be copied when it is converted to string, packID and topic
	// are unused now, so deep copy is unnecessary.
	return plugin.ProcessRawLog(rawLog, util.StringDeepCopy(packID), topic)
}

//export ProcessRawLogV2
func ProcessRawLogV2(configName string, rawLog []byte, packID string, topic string, tags []byte) int {
	config, exists := pluginmanager.LogtailConfig[configName]
	if !exists {
		return -1
	}
	return config.ProcessRawLogV2(rawLog, util.StringDeepCopy(packID), util.StringDeepCopy(topic), tags)
}

//export ProcessLog
func ProcessLog(configName string, logBytes []byte, packID string, topic string, tags []byte) int {
	config, exists := pluginmanager.LogtailConfig[configName]
	if !exists {
		logger.Debug(context.Background(), "config not found", configName)
		return -1
	}
	return config.ProcessLog(logBytes, util.StringDeepCopy(packID), util.StringDeepCopy(topic), tags)
}

//export ProcessLogGroup
func ProcessLogGroup(configName string, logBytes []byte, packID string) int {
	config, exists := pluginmanager.LogtailConfig[configName]
	if !exists {
		logger.Debug(context.Background(), "config not found", configName)
		return -1
	}
	return config.ProcessLogGroup(logBytes, util.StringDeepCopy(packID))
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
	if exitFlag != 0 {
		logger.Info(context.Background(), "logger", "close and recover")
		logger.Close()
	}
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
func CtlCmd(configName string, cmdID int, cmdDetail string) {
	logger.Info(context.Background(), "execute cmd", cmdID, "detail", cmdDetail, "config", configName)
}

//export GetContainerMeta
func GetContainerMeta(containerID string) *C.struct_containerMeta {
	logger.Init()
	meta := helper.GetContainerMeta(containerID)
	if meta == nil {
		logger.Debug(context.Background(), "get meta", "")
		return nil
	}
	if logger.DebugFlag() {
		bytes, _ := json.Marshal(meta)
		logger.Debug(context.Background(), "get meta", string(bytes))
	}
	returnStruct := (*C.struct_containerMeta)(C.malloc(C.size_t(unsafe.Sizeof(C.struct_containerMeta{}))))
	returnStruct.podName = C.CString(meta.PodName)
	returnStruct.k8sNamespace = C.CString(meta.K8sNamespace)
	returnStruct.containerName = C.CString(meta.ContainerName)
	returnStruct.image = C.CString(meta.Image)

	returnStruct.k8sLabelsSize = C.int(len(meta.K8sLabels))
	if len(meta.K8sLabels) > 0 {
		returnStruct.k8sLabelsKey = C.makeCharArray(returnStruct.k8sLabelsSize)
		returnStruct.k8sLabelsVal = C.makeCharArray(returnStruct.k8sLabelsSize)
		count := 0
		for k, v := range meta.K8sLabels {
			C.setArrayString(returnStruct.k8sLabelsKey, C.CString(k), C.int(count))
			C.setArrayString(returnStruct.k8sLabelsVal, C.CString(v), C.int(count))
			count++
		}
	}

	returnStruct.containerLabelsSize = C.int(len(meta.ContainerLabels))
	if len(meta.ContainerLabels) > 0 {
		returnStruct.containerLabelsKey = C.makeCharArray(returnStruct.containerLabelsSize)
		returnStruct.containerLabelsVal = C.makeCharArray(returnStruct.containerLabelsSize)
		count := 0
		for k, v := range meta.ContainerLabels {
			C.setArrayString(returnStruct.containerLabelsKey, C.CString(k), C.int(count))
			C.setArrayString(returnStruct.containerLabelsVal, C.CString(v), C.int(count))
			count++
		}
	}

	returnStruct.envSize = C.int(len(meta.Env))
	if len(meta.Env) > 0 {
		returnStruct.envsKey = C.makeCharArray(returnStruct.envSize)
		returnStruct.envsVal = C.makeCharArray(returnStruct.envSize)
		count := 0
		for k, v := range meta.Env {
			C.setArrayString(returnStruct.envsKey, C.CString(k), C.int(count))
			C.setArrayString(returnStruct.envsVal, C.CString(v), C.int(count))
			count++
		}
	}
	return returnStruct
}

//export GetPipelineMetrics
func GetPipelineMetrics() *C.PluginMetrics {
	results := pluginmanager.GetMetrics()
	// 统计所有键值对的总数，用于分配内存
	numMetrics := len(results)

	cPluginMetrics := (*C.PluginMetrics)(C.malloc(C.sizeof_PluginMetrics))
	cPluginMetrics.count = C.int(numMetrics)
	cPluginMetrics.metrics = C.makePluginMetricArray(cPluginMetrics.count)
	// 填充 PluginMetrics 中的 keyValues
	for i, metric := range results {
		metricLen := len(metric)
		cMetric := (*C.PluginMetric)(C.malloc(C.sizeof_PluginMetric))
		cMetric.count = C.int(metricLen)
		cMetric.keyValues = C.makeKeyValueArray(cMetric.count)

		j := 0
		for k, v := range metric {
			cKey := C.CString(k)
			cValue := C.CString(v)
			cKeyValue := (*C.KeyValue)(C.malloc(C.sizeof_KeyValue))
			cKeyValue.key = cKey
			cKeyValue.value = cValue

			C.setArrayKeyValue(cMetric.keyValues, cKeyValue, C.int(j))
			j++
		}
		C.setArrayPluginMetric(cPluginMetrics.metrics, cMetric, C.int(i))
	}
	return cPluginMetrics
}

func initPluginBase(cfgStr string) int {
	// Only the first call will return non-zero.
	rst := 0
	initOnce.Do(func() {
		logger.Init()
		flags.OverrideByEnv()
		InitHTTPServer()
		setGCPercentForSlowStart()
		logger.Info(context.Background(), "init plugin base, version", config.BaseVersion)
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
