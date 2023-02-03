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

package gpu

import (
	"strconv"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"github.com/mindprince/gonvml"
)

type InputGpuMetric struct {
	CollectIntervalMs int

	context   pipeline.Context
	collector pipeline.Collector

	waitGroup sync.WaitGroup

	shutdown chan struct{}
}

func (r *InputGpuMetric) Init(context pipeline.Context) (int, error) {
	r.context = context

	if r.CollectIntervalMs <= 0 {
		r.CollectIntervalMs = 1000
	}

	return 0, nil
}

func (r *InputGpuMetric) Description() string {
	return "collect gpu metric plugin for logtail (only linux and nvidia gpu)"
}

func (r *InputGpuMetric) Collect(collector pipeline.Collector) error {
	return nil
}

func (r *InputGpuMetric) Start(collector pipeline.Collector) error {
	err := gonvml.Initialize()
	if err != nil {
		logger.Error(r.context.GetRuntimeContext(), "GPU_NVML_INIT_ALARM", "Couldn't initialize nvml, error", err)
		return err
	}
	defer gonvml.Shutdown()

	r.collector = collector
	r.shutdown = make(chan struct{})
	r.waitGroup.Add(1)
	defer r.waitGroup.Done()

	timer := time.NewTimer(time.Duration(r.CollectIntervalMs) * time.Millisecond)
	defer timer.Stop()

	for {
		select {
		case <-r.shutdown:
			return nil
		case <-timer.C:
			err := r.CollectGpuMetric()
			if err != nil {
				logger.Error(r.context.GetRuntimeContext(), "GPU_NVML_COLLECT_ALARM", "GPU collect metric error", err)
				return nil
			}
			timer.Reset(time.Duration(r.CollectIntervalMs) * time.Millisecond)
		}
	}
}

func (r *InputGpuMetric) CollectGpuMetric() error {
	t := time.Now()
	numDevices, err := gonvml.DeviceCount()
	if err != nil {
		logger.Error(r.context.GetRuntimeContext(), "GPU_NVML_DEVICE_COUNT_ALARM", "GPU DeviceCount error", err)
		return err
	}

	for index := uint(0); index < numDevices; index++ {
		fields := make(map[string]string)
		fields["metric_type"] = "gpu"
		fields["device"] = strconv.FormatUint(uint64(index), 10)

		device, err := gonvml.DeviceHandleByIndex(index)
		if err != nil {
			logger.Error(r.context.GetRuntimeContext(), "GPU_NVML_DEVICE_INDEX_ALARM", "GPU DeviceHandleByIndex", index, "error", err)
			return err
		}
		powerUsage, _ := device.PowerUsage()
		fields["gpu_power_usage"] = strconv.FormatUint(uint64(powerUsage)/1000, 10)
		temperature, _ := device.Temperature()
		fields["gpu_temperature"] = strconv.FormatUint(uint64(temperature), 10)
		gpuUtil, memoryUtil, _ := device.UtilizationRates()
		fields["gpu_util"] = strconv.FormatUint(uint64(gpuUtil), 10)
		fields["gpu_memory_util"] = strconv.FormatUint(uint64(memoryUtil), 10)
		totalMemory, usedMemory, _ := device.MemoryInfo()
		fields["gpu_used_memory"] = strconv.FormatUint(usedMemory/1024/1024, 10)
		fields["gpu_total_memory"] = strconv.FormatUint(totalMemory/1024/1024, 10)
		fields["gpu_free_memory"] = strconv.FormatUint((totalMemory-usedMemory)/1024/1024, 10)
		r.collector.AddData(nil, fields, t)
	}
	return nil
}

func (r *InputGpuMetric) Stop() error {
	close(r.shutdown)
	r.waitGroup.Wait()
	return nil
}

func init() {
	pipeline.ServiceInputs["service_gpu_metric"] = func() pipeline.ServiceInput {
		return &InputGpuMetric{
			CollectIntervalMs: 1000,
		}
	}
}
