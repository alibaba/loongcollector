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
	"strconv"
	"sync"
	"time"
	"unsafe"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/flags"
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/helper/containercenter"
	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/pkg/util"
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

// PBBuffer carries a serialized PipelineEventGroup protobuf that the C++ self-monitor
// timer pulls from Go via GetGoSelfMonitor*PB. data is allocated inside
// libGoPluginBase.so; after the C++ caller copies the bytes on receive it must
// release the buffer via FreeSelfMonitorPB (never free() directly) so that allocation
// and release stay in the same module and no memory crosses the module boundary. size
// is the byte length; it is size_t (unsigned, pointer-sized) rather than int so the
// cross-language length matches the C++ std::string consumer and cannot overflow or go
// negative on the ABI boundary. data is NULL and size is 0 when there is nothing to
// pull or on marshal failure.
typedef struct {
    char* data;
    size_t size;
} PBBuffer;

*/
import "C" //nolint:typecheck

var initOnce sync.Once
var loadOnce sync.Once

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
		if len(jsonStr) >= 2 { // For invalid JSON, use default value and return 0
			if err := json.Unmarshal([]byte(jsonStr), &config.LoongcollectorGlobalConfig); err != nil {
				fmt.Println("load global config error", "GlobalConfig", jsonStr, "err", err)
				retcode = 1
			}
			logger.InitLogger()
			for _, log := range flags.LogsWaitToPrint {
				switch log.LogType {
				case flags.LogTypeError:
					logger.Error(context.Background(), log.AlarmType, log.Content)
				case flags.LogTypeInfo:
					logger.Info(context.Background(), log.Content)
				case flags.LogTypeDebug:
					logger.Debug(context.Background(), log.Content)
				case flags.LogTypeWarning:
					logger.Warning(context.Background(), log.AlarmType, log.Content)
				}
			}
			logger.Info(context.Background(), "load global config", jsonStr)
			config.UserAgent = fmt.Sprintf("ilogtail/%v (%v) ip/%v", config.BaseVersion, runtime.GOOS, config.LoongcollectorGlobalConfig.HostIP)
		}
	})
	if retcode == 0 {
		// Update when both of them are not empty.
		logger.Debugf(context.Background(), "host IP: %v, hostname: %v",
			config.LoongcollectorGlobalConfig.HostIP, config.LoongcollectorGlobalConfig.Hostname)
		if len(config.LoongcollectorGlobalConfig.Hostname) > 0 && len(config.LoongcollectorGlobalConfig.HostIP) > 0 {
			util.SetNetworkIdentification(config.LoongcollectorGlobalConfig.HostIP, config.LoongcollectorGlobalConfig.Hostname)
		}
	}
	return retcode
}

//export LoadPipeline
func LoadPipeline(project string, logstore string, configName string, logstoreKey int64, jsonStr string) int {
	logger.Debug(context.Background(), "load config", configName, logstoreKey, "\n"+jsonStr)
	defer func() {
		if err := recover(); err != nil {
			trace := make([]byte, 2048)
			runtime.Stack(trace, true)
			logger.Critical(context.Background(), selfmonitor.PluginRuntimeAlarm, "panicked", err, "stack", string(trace))
		}
	}()

	err := pluginmanager.LoadLogstoreConfig(util.StringDeepCopy(project),
		util.StringDeepCopy(logstore), util.StringDeepCopy(configName),
		// Make deep copy if you want to save it in Go in the future.
		logstoreKey, jsonStr)
	if err != nil {
		logger.Critical(context.Background(), selfmonitor.ConfigLoadAlarm, "load config error, project",
			project, "logstore", logstore, "config", configName, "error", err)
		return 1
	}
	return 0
}

//export UnloadPipeline
func UnloadPipeline(configName string) int {
	logger.Debug(context.Background(), "unload config", configName)
	err := pluginmanager.UnloadPartiallyLoadedConfig(util.StringDeepCopy(configName))
	if err != nil {
		return 1
	}
	return 0
}

// ProcessPipelineEventGroup is the C++ → Go PipelineEventGroup PB ingress.
// The reverse Go → C++ PEG PB path is logtail.SendPipelineEventGroup (adapter
// callback, sibling of SendPb); it is not a Go export.
//
//export ProcessPipelineEventGroup
func ProcessPipelineEventGroup(configName string, pbBytes []byte, packID string) int {
	pluginmanager.LogtailConfigLock.RLock()
	config, flag := pluginmanager.LogtailConfig[configName]
	pluginmanager.LogtailConfigLock.RUnlock()
	if !flag {
		logger.Critical(context.Background(), selfmonitor.PluginAlarm, "config not found", configName)
		return -1
	}
	return config.ProcessPipelineEventGroup(pbBytes, util.StringDeepCopy(packID))
}

//export StopAllPipelines
func StopAllPipelines(withInputFlag int) {
	logger.Info(context.Background(), "Stop all", "start", "with input", withInputFlag)
	err := pluginmanager.StopAllPipelines(withInputFlag != 0)
	if err != nil {
		logger.Error(context.Background(), selfmonitor.PluginAlarm, "stop all error", err)
	}
	logger.Info(context.Background(), "Stop all", "success", "with input", withInputFlag)
	// Stop with input first, without input last.
	if withInputFlag == 0 {
		logger.Info(context.Background(), "logger", "close and recover")
		logger.Flush()
		logger.Close()
	}
}

//export Stop
func Stop(configName string, removedFlag int) {
	logger.Info(context.Background(), "Stop", "start", "config", configName, "removed", removedFlag)
	err := pluginmanager.Stop(configName, removedFlag != 0)
	if err != nil {
		logger.Error(context.Background(), selfmonitor.PluginAlarm, "stop error", err)
	}
}

//export StopBuiltInModules
func StopBuiltInModules() {
	pluginmanager.StopBuiltInModulesConfig()
}

//export Start
func Start(configName string) {
	logger.Info(context.Background(), "Start", "start", "config", configName)
	err := pluginmanager.Start(configName)
	if err != nil {
		logger.Error(context.Background(), selfmonitor.PluginAlarm, "start error", err)
	}
	logger.Info(context.Background(), "Start", "success", "config", configName)
}

//export CtlCmd
func CtlCmd(configName string, cmdID int, cmdDetail string) {
	logger.Info(context.Background(), "execute cmd", cmdID, "detail", cmdDetail, "config", configName)
}

//export GetAllContainers
func GetAllContainers() *C.char {
	return C.CString(pluginmanager.GetAllContainers())
}

//export GetDiffContainers
func GetDiffContainers() *C.char {
	return C.CString(pluginmanager.GetDiffContainers())
}

//export GetContainerMeta
func GetContainerMeta(containerID string) *C.struct_containerMeta {
	logger.InitLogger()
	meta := containercenter.GetContainerMeta(containerID)
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

// marshalAlarmsPB collects Go self-monitor alarms into a PipelineEventGroup PB
// (one LogEvent per alarm) using the D2 contract helper and returns the serialized
// bytes. These bytes are what GetGoSelfMonitorAlarmsPB hands to C++ when C++ pulls;
// factored out of the CGO export so it can be unit-tested without cgo types.
func marshalAlarmsPB(alarms []selfmonitor.AlarmExportMessage, now time.Time) ([]byte, error) {
	group, err := helper.TransferAlarmsToPipelineEventGroup(alarms, now)
	if err != nil {
		return nil, err
	}
	return group.Marshal()
}

// marshalMetricsPB collects Go self-monitor metrics of the given export type into a
// PipelineEventGroup PB (one MetricEvent per record) using the D2 contract helper
// and returns the serialized bytes, the payload GetGoSelfMonitorMetricsPB hands to
// C++ on pull and that ReadMetrics::ParseGoMetricsPB consumes.
func marshalMetricsPB(rawMetrics []map[string]string, now time.Time) ([]byte, error) {
	records := make([]selfmonitor.MetricExportRecord, 0, len(rawMetrics))
	for _, raw := range rawMetrics {
		if _, ok := raw[selfmonitor.MetricLabelPrefix]; ok {
			record, err := selfmonitor.ParseMetricExportRecord(raw)
			if err != nil {
				return nil, fmt.Errorf("parse self-monitor metric record: %w", err)
			}
			records = append(records, record)
			continue
		}
		// cpp_provided uses flat name/value maps, unlike direct plugin records.
		// Normalize them so process-level memory/goroutine gauges survive PB.
		record := selfmonitor.MetricExportRecord{Category: "agent", Gauges: make(map[string]float64)}
		for name, text := range raw {
			value, err := strconv.ParseFloat(text, 64)
			if err != nil {
				return nil, fmt.Errorf("parse self-monitor gauge %s: %w", name, err)
			}
			record.Gauges[name] = value
		}
		records = append(records, record)
	}
	return helper.TransferMetricsToPipelineEventGroup(records, now).Marshal()
}

// toPBBuffer copies data into a buffer allocated inside libGoPluginBase.so. The C++
// caller copies the bytes on receive and then releases the buffer via
// FreeSelfMonitorPB (not free()), keeping allocation and release in the same module
// so no memory crosses the boundary. An empty or failed marshal yields a nil buffer,
// which the receiver treats as no-op.
func toPBBuffer(data []byte, err error) C.PBBuffer {
	var buf C.PBBuffer
	if err != nil {
		logger.Error(context.Background(), selfmonitor.PluginAlarm, "marshal self-monitor PB error", err)
		return buf
	}
	if len(data) == 0 {
		return buf
	}
	buf.data = (*C.char)(C.CBytes(data))
	buf.size = C.size_t(len(data))
	return buf
}

// GetGoSelfMonitorAlarmsPB lets the C++ self-monitor timer pull Go self-monitor
// alarms as a serialized PipelineEventGroup protobuf. C++ copies the bytes into a
// std::string, feeds them to SelfMonitorServer::ReceiveGoAlarmsPB, then releases the
// buffer via FreeSelfMonitorPB. The timer uses this PB path on all platforms.
//
//export GetGoSelfMonitorAlarmsPB
func GetGoSelfMonitorAlarmsPB() C.PBBuffer {
	data, err := marshalAlarmsPB(pluginmanager.GetAlarmMessages(), time.Now())
	return toPBBuffer(data, err)
}

// GetGoSelfMonitorMetricsPB lets the C++ self-monitor timer pull Go self-monitor
// metrics of the given export type as a serialized PipelineEventGroup protobuf. C++
// copies the bytes into a std::string, feeds them to ReadMetrics::ParseGoMetricsPB,
// then releases the buffer via FreeSelfMonitorPB on all platforms.
//
//export GetGoSelfMonitorMetricsPB
func GetGoSelfMonitorMetricsPB(metricType string) C.PBBuffer {
	data, err := marshalMetricsPB(pluginmanager.GetMetrics(metricType), time.Now())
	return toPBBuffer(data, err)
}

// FreeSelfMonitorPB releases a PBBuffer.data block previously returned by
// GetGoSelfMonitorAlarmsPB / GetGoSelfMonitorMetricsPB. Both the allocation
// (C.CBytes in toPBBuffer) and this release run inside libGoPluginBase.so, so the
// buffer is never freed across the module boundary — this satisfies the dynamic
// library rule that memory must be allocated and freed within the same module and
// avoids relying on a shared CRT heap under static-CRT linking (the cross-boundary
// free pattern tracked as the Windows crash risk in #2645). A nil pointer is a no-op,
// so callers may invoke it unconditionally.
//
//export FreeSelfMonitorPB
func FreeSelfMonitorPB(data *C.char) {
	if data != nil {
		C.free(unsafe.Pointer(data))
	}
}

func initPluginBase(cfgStr string) int {
	// Only the first call will return non-zero.
	rst := 0
	initOnce.Do(func() {
		LoadGlobalConfig(cfgStr)
		InitHTTPServer()
		setGCPercentForSlowStart()
		logger.Info(context.Background(), "init plugin base, version", config.BaseVersion)
		if *flags.DeployMode == flags.DeploySingleton && *flags.EnableKubernetesMeta {
			instance := k8smeta.GetMetaManagerInstance()
			err := instance.Init("")
			if err != nil {
				logger.Critical(context.Background(), k8smeta.K8sMetaUnifyErrorCode, "init k8s meta manager fail", err)
				return
			}
			stopCh := make(chan struct{})
			instance.Run(stopCh)
		}
		if err := pluginmanager.Init(); err != nil {
			logger.Critical(context.Background(), selfmonitor.PluginAlarm, "init plugin error", err)
			rst = 1
		}
		if pluginmanager.ContainerConfig != nil {
			pluginmanager.ContainerConfig.Start()
		}
		err := pluginmanager.CheckPointManager.Init()
		if err != nil {
			logger.Error(context.Background(), selfmonitor.CheckpointInitAlarm, "init checkpoint manager error", err)
		}
		pluginmanager.CheckPointManager.Start()
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
