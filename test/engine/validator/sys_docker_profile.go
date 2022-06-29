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

package validator

import (
	"context"
	"encoding/json"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/docker/docker/api/types"
	"github.com/docker/docker/api/types/filters"
	"github.com/docker/docker/client"
	"github.com/docker/go-units"
	"github.com/mitchellh/mapstructure"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const dockerProfileValidatorName = "sys_docker_profile"

type dockerProfileSystemValidator struct {
	ExpectEverySecondMaximumCPU float64 `mapstructure:"expect_every_second_maximum_cpu" comment:"the maximum CPU percentage in every second, the unit is percentage."`
	ExpectEverySecondMaximumMem string  `mapstructure:"expect_every_second_maximum_mem" comment:"the maximum memory cost in every second, such as 300KB."`

	memSize     int64
	cli         *client.Client
	waitGroup   sync.WaitGroup
	cancel      chan struct{}
	containerID string
	cpu         []float64
	mem         []uint64
}

func (d *dockerProfileSystemValidator) Description() string {
	return "this is a system validator to monitor the CPU and RAM status of Ilogtail docker container"
}

func (d *dockerProfileSystemValidator) Start() error {
	list, err := d.cli.ContainerList(context.Background(), types.ContainerListOptions{
		Filters: filters.NewArgs(filters.Arg("name", "ilogtail-e2e_ilogtail")),
	})
	if err != nil {
		logger.Errorf(context.Background(), "DOCKER_PROFILE_ALARM", "error in find logtailplugon container: %v", err)
		return err
	}
	if len(list) != 1 {
		logger.Errorf(context.Background(), "DOCKER_PROFILE_ALARM", "docekr container size is not equal 1, got %d count", len(list))
		return err
	}
	d.containerID = list[0].ID
	d.waitGroup.Add(1)
	go func() {
		ticker := time.NewTicker(time.Second)
		for {
			select {
			case <-d.cancel:
				d.waitGroup.Done()
				return
			case <-ticker.C:
				d.FetchProfile()
			}
		}
	}()
	return nil
}

func (d *dockerProfileSystemValidator) Valid(group *protocol.LogGroup) {
}

func (d *dockerProfileSystemValidator) FetchResult() (reports []*Report) {
	close(d.cancel)
	d.waitGroup.Wait()
	res := make([]string, 0, len(d.cpu))
	if d.ExpectEverySecondMaximumCPU > 0 {
		for i := 0; i < len(d.cpu); i++ {
			if d.cpu[i] > d.ExpectEverySecondMaximumCPU {
				reports = append(reports, &Report{Validator: dockerProfileValidatorName, Name: "maximum_second_cpu",
					Want: strconv.FormatFloat(d.ExpectEverySecondMaximumCPU, 'f', 2, 64) + "%",
					Got:  strconv.FormatFloat(d.cpu[i], 'f', 2, 64) + "%",
				})
			}
		}
	}
	for _, f := range d.cpu {
		res = append(res, strconv.FormatFloat(f, 'f', 2, 64)+"%")
	}
	logger.Info(context.Background(), "cost cpu", strings.Join(res, ","))
	res = res[:0]
	if d.ExpectEverySecondMaximumMem != "" {
		for i := 0; i < len(d.mem); i++ {
			if d.mem[i] > uint64(d.memSize) {
				reports = append(reports, &Report{Validator: dockerProfileValidatorName, Name: "maximum_second_mem",
					Want: d.ExpectEverySecondMaximumMem,
					Got:  units.HumanSize(float64(d.mem[i])),
				})
			}
		}
	}
	for _, f := range d.mem {
		res = append(res, units.HumanSize(float64(f)))
	}
	logger.Info(context.Background(), "cost mem", strings.Join(res, ","))
	return
}

func (d *dockerProfileSystemValidator) Name() string {
	return counterSysValidatorName
}

func (d *dockerProfileSystemValidator) FetchProfile() {
	resp, err := d.cli.ContainerStats(context.Background(), d.containerID, false)
	if err != nil {
		logger.Errorf(context.Background(), "DOCKER_PROFILE_ALARM", "error in fetching docker stats: %v", err)
		return
	}
	defer resp.Body.Close()

	dec := json.NewDecoder(resp.Body)
	var stats types.StatsJSON
	err = dec.Decode(&stats)
	if err != nil {
		logger.Errorf(context.Background(), "DOCKER_PROFILE_ALARM", "error in reading docker stats: %v", err)
	}
	if resp.OSType != "windows" {
		d.cpu = append(d.cpu, calculateCPUPercentUnix(&stats.PreCPUStats, &stats.CPUStats))
		d.mem = append(d.mem, stats.MemoryStats.Usage)
	} else {
		d.cpu = append(d.cpu, calculateCPUPercentWindows(&stats))
		d.mem = append(d.mem, stats.MemoryStats.PrivateWorkingSet)
	}
}

func calculateCPUPercentWindows(v *types.StatsJSON) float64 {
	// Max number of 100ns intervals between the previous time read and now
	possIntervals := uint64(v.Read.Sub(v.PreRead).Nanoseconds()) // Start with number of ns intervals
	possIntervals /= 100                                         // Convert to number of 100ns intervals
	possIntervals *= uint64(v.NumProcs)                          // Multiple by the number of processors

	// Intervals used
	intervalsUsed := v.CPUStats.CPUUsage.TotalUsage - v.PreCPUStats.CPUUsage.TotalUsage

	// Percentage avoiding divide-by-zero
	if possIntervals > 0 {
		return float64(intervalsUsed) / float64(possIntervals) * 100.0
	}
	return 0.00
}

func calculateCPUPercentUnix(preCPU, cpu *types.CPUStats) float64 {
	var (
		cpuPercent = 0.0
		// calculate the change for the cpu usage of the container in between readings
		cpuDelta = float64(cpu.CPUUsage.TotalUsage) - float64(preCPU.CPUUsage.TotalUsage)
		// calculate the change for the entire system between readings
		systemDelta = float64(cpu.SystemUsage) - float64(preCPU.SystemUsage)
		onlineCPUs  = float64(cpu.OnlineCPUs)
	)

	if onlineCPUs == 0.0 {
		onlineCPUs = float64(len(cpu.CPUUsage.PercpuUsage))
	}
	if systemDelta > 0.0 && cpuDelta > 0.0 {
		cpuPercent = (cpuDelta / systemDelta) * onlineCPUs * 100.0
	}
	return cpuPercent
}

func init() {
	RegisterSystemValidatorCreator(dockerProfileValidatorName, func(spec map[string]interface{}) (SystemValidator, error) {
		v := new(dockerProfileSystemValidator)
		err := mapstructure.Decode(spec, v)
		if err != nil {
			return nil, err
		}
		cli, err := client.NewClientWithOpts(client.FromEnv)
		if err != nil {
			return nil, err
		}
		v.cli = cli
		go func() {
			<-context.Background().Done()
			_ = v.cli.Close()
		}()
		v.cancel = make(chan struct{})
		if v.ExpectEverySecondMaximumMem != "" {
			size, err := units.FromHumanSize(v.ExpectEverySecondMaximumMem)
			if err != nil {
				return nil, err
			}
			v.memSize = size
		}
		return v, nil
	})
	doc.Register("sys_validator", dockerProfileValidatorName, new(dockerProfileSystemValidator))
}
