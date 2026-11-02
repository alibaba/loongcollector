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
	"github.com/alibaba/ilogtail/pkg/protocol"
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
// timer pulls from Go via GetGoSelfMonitor*PB. Go marshals directly into data
// allocated inside libGoPluginBase.so; after C++ finishes parsing the borrowed bytes it must
// release the buffer via FreeSelfMonitorPB (never free() directly) so that allocation
// and release stay in the same module. C++ borrows the buffer but never owns it. size
// is the byte length; it is size_t (unsigned, pointer-sized) rather than int so the
// cross-language length matches the C++ string_view consumer and cannot overflow or go
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

// buildAlarmsPB sends business fields to C++; the final sender adds ip/os/ver.
// Empty snapshots require neither a protobuf envelope nor a cross-language allocation.
func buildAlarmsPB(alarms []selfmonitor.AlarmExportMessage, now time.Time) (*protocol.PipelineEventGroup, error) {
	if len(alarms) == 0 {
		return nil, nil
	}
	return helper.TransferAlarmsToPipelineEventGroup(alarms, now)
}

// buildMetricsPB fills the final protobuf directly from exported JSON fields.
// Numeric maps and a MetricExportRecord batch are unnecessary intermediates.
// PB bytes borrow immutable decoded strings only until synchronous marshaling.
func buildMetricsPB(rawMetrics []map[string]string, now time.Time) (*protocol.PipelineEventGroup, error) {
	if len(rawMetrics) == 0 {
		return nil, nil
	}
	events := make([]protocol.MetricEvent, len(rawMetrics))
	refs := make([]*protocol.MetricEvent, len(rawMetrics))
	timestamp := uint64(now.Unix())*1e9 + uint64(now.Nanosecond())
	for i, raw := range rawMetrics {
		event := &events[i]
		refs[i] = event
		event.Timestamp = timestamp
		values := make(map[string]*protocol.UntypedMultiDoubleValue)
		if labelsJSON, ok := raw[selfmonitor.MetricLabelPrefix]; ok {
			var labels map[string]string
			if labelsJSON != "" {
				if err := json.Unmarshal([]byte(labelsJSON), &labels); err != nil {
					return nil, fmt.Errorf("parse self-monitor labels: %w", err)
				}
			}
			event.Name = util.ZeroCopyStringToBytes(labels[selfmonitor.MetricLabelKeyMetricCategory])
			if len(labels) > 1 {
				event.Tags = make(map[string][]byte, len(labels)-1)
			}
			for name, value := range labels {
				if name == selfmonitor.MetricLabelKeyMetricCategory {
					continue
				}
				if event.Tags == nil {
					event.Tags = make(map[string][]byte)
				}
				event.Tags[name] = util.ZeroCopyStringToBytes(value)
			}
			// Reuse one decoding map for counters/gauges. This also preserves the
			// existing invalid numeric value -> zero contract.
			var fields map[string]string
			for _, field := range [...]struct {
				key  string
				kind protocol.UntypedValueMetricType
			}{
				{selfmonitor.MetricCounterPrefix, protocol.UntypedValueMetricType_METRIC_TYPE_COUNTER},
				{selfmonitor.MetricGaugePrefix, protocol.UntypedValueMetricType_METRIC_TYPE_GAUGE},
			} {
				clear(fields)
				if raw[field.key] != "" {
					if err := json.Unmarshal([]byte(raw[field.key]), &fields); err != nil {
						return nil, fmt.Errorf("parse self-monitor %s: %w", field.key, err)
					}
				}
				for name, text := range fields {
					value, err := strconv.ParseFloat(text, 64)
					if err != nil {
						value = 0
					}
					if field.kind == protocol.UntypedValueMetricType_METRIC_TYPE_COUNTER {
						value = float64(uint64(value))
					}
					values[name] = &protocol.UntypedMultiDoubleValue{MetricType: field.kind, Value: value}
				}
			}
		} else {
			// cpp_provided process gauges are already a flat name/value map.
			event.Name = []byte("agent")
			for name, text := range raw {
				value, err := strconv.ParseFloat(text, 64)
				if err != nil {
					return nil, fmt.Errorf("parse self-monitor gauge %s: %w", name, err)
				}
				values[name] = &protocol.UntypedMultiDoubleValue{MetricType: protocol.UntypedValueMetricType_METRIC_TYPE_GAUGE, Value: value}
			}
		}
		event.Value = &protocol.MetricEvent_UntypedMultiDoubleValues{
			UntypedMultiDoubleValues: &protocol.UntypedMultiDoubleValues{Values: values},
		}
	}
	return &protocol.PipelineEventGroup{
		PipelineEvents: &protocol.PipelineEventGroup_Metrics{
			Metrics: &protocol.PipelineEventGroup_MetricEvents{Events: refs},
		},
	}, nil
}

// toPBBuffer marshals directly into the final C allocation. No temporary Go
// payload or C.CBytes copy is needed. C++ borrows this allocation only until it
// calls FreeSelfMonitorPB; both allocation and release stay inside the Go module.
func toPBBuffer(group *protocol.PipelineEventGroup, err error) C.PBBuffer {
	if err != nil {
		logger.Error(context.Background(), selfmonitor.PluginAlarm, "build self-monitor PB error", err)
		return C.PBBuffer{}
	}
	if group == nil {
		return C.PBBuffer{}
	}
	size := group.Size()
	if size == 0 {
		return C.PBBuffer{}
	}
	data := C.malloc(C.size_t(size))
	buf := unsafe.Slice((*byte)(data), size)
	n, err := group.MarshalToSizedBuffer(buf)
	if err != nil || n != size {
		C.free(data)
		logger.Error(context.Background(), selfmonitor.PluginAlarm, "marshal self-monitor PB error", err, "size", size, "written", n)
		return C.PBBuffer{}
	}
	return C.PBBuffer{data: (*C.char)(data), size: C.size_t(size)}
}

// GetGoSelfMonitorAlarmsPB returns alarm PB bytes for the C++ timer on every
// platform. The receiver parses the borrowed buffer and releases it via Go.
//
//export GetGoSelfMonitorAlarmsPB
func GetGoSelfMonitorAlarmsPB() C.PBBuffer {
	return toPBBuffer(buildAlarmsPB(pluginmanager.GetAlarmMessages(), time.Now()))
}

// GetGoSelfMonitorMetricsPB returns metric PB bytes for the C++ timer on every
// platform. The receiver consumes typed fields without a JSON round trip.
//
//export GetGoSelfMonitorMetricsPB
func GetGoSelfMonitorMetricsPB(metricType string) C.PBBuffer {
	return toPBBuffer(buildMetricsPB(pluginmanager.GetMetrics(metricType), time.Now()))
}

// FreeSelfMonitorPB releases a PBBuffer.data block previously returned by
// GetGoSelfMonitorAlarmsPB / GetGoSelfMonitorMetricsPB. Both the allocation
// (C.malloc in toPBBuffer) and this release run inside libGoPluginBase.so, so the
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
