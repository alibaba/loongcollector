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

package process

import (
	"github.com/shirou/gopsutil/process"

	"encoding/json"
	"fmt"
	"os"
	"reflect"
	"regexp"
	"sort"
	"strings"
	"testing"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

type TestProcessCache struct {
	pid int
	*processMeta
	*processStatus
}

func (t *TestProcessCache) GetPid() int {
	return t.pid
}

func (t *TestProcessCache) Same(cache processCache) bool {
	return t.pid == cache.GetPid()
}

func (t *TestProcessCache) GetExe() string {
	return ""
}

func (t *TestProcessCache) GetCmdLine() string {
	return ""
}

func (t *TestProcessCache) FetchCoreCount() int64 {
	return t.fetchCoreCount
}

func (t *TestProcessCache) Labels(values *helper.MetricLabels) *helper.MetricLabels {
	return nil
}

func (t *TestProcessCache) GetProcessStatus() *processStatus { // nolint:revive
	return t.processStatus
}

func (t *TestProcessCache) FetchCore() bool {
	t.fetchCoreCount++
	return true
}

func (t *TestProcessCache) FetchFds() bool {
	return true
}

func (t *TestProcessCache) FetchNetIO() bool {
	return true
}

func (t *TestProcessCache) FetchIO() bool {
	return true
}

func (t *TestProcessCache) FetchThreads() bool {
	return true
}

func TestInputProcess_RegexMatching1(t *testing.T) {
	getpid := os.Getpid()
	newProcess, _ := process.NewProcess(int32(getpid))
	cmdline, _ := newProcess.Cmdline()
	split := strings.Split(cmdline, " ")
	compile, _ := regexp.Compile(".*" + split[0] + ".*")
	fmt.Printf("%v", compile.Match([]byte(cmdline)))
}

func TestInputProcess_RegexMatching(t *testing.T) {
	cxt := mock.NewEmptyContext("project", "store", "config")
	p := pipeline.MetricInputs["metric_process_v2"]().(*InputProcess)
	currentPid := os.Getpid()
	newProcess, err := process.NewProcess(int32(currentPid))
	if err != nil {
		t.Errorf("cannot get current process proc: %v", err)
	}
	cmd, err := newProcess.Cmdline()
	if err != nil {
		t.Errorf("cannot get current process cmd: %v", err)
	}
	cmd = strings.Split(cmd, " ")[0]
	p.ProcessNamesRegex = []string{
		"^" + cmd + ".*",
	}
	fmt.Println()
	fmt.Println()
	fmt.Printf("%s\n", cmd)
	p.OpenFD = false
	p.Thread = false
	p.IO = false
	p.NetIO = false
	if _, err := p.Init(cxt); err != nil {
		t.Errorf("cannot init the mock process plugin: %v", err)
		return
	}
	c := &test.MockMetricCollector{}
	_ = p.Collect(c)
	_ = p.Collect(c)
	if len(c.Logs) != 7 {
		bytes, _ := json.Marshal(c.Logs)
		t.Errorf("regex matching should only match one process: %s", string(bytes))
	} else {
		for _, log := range c.Logs {
			bytes, _ := json.Marshal(log)
			fmt.Printf("got metrics: %s\n", string(bytes))
		}
	}
}

func TestInputProcess_filterTopAndThresholdMatchedProcesses(t *testing.T) {
	type fields struct {
		MaxProcessCount    int
		TopNCPU            int
		TopNMem            int
		MinCPULimitPercent float64
		MinMemoryLimitKB   int
	}
	creator := func(pid int) *TestProcessCache {
		return &TestProcessCache{
			pid:         pid,
			processMeta: newProcessCacheMeta(100),
			processStatus: &processStatus{
				CPUPercentage: &cpuPercentage{
					TotalPercentage: float64(pid) * 10,
				},
				Memory: &memory{
					Rss: (6 - uint64(pid)) * 1024 * 1024,
				},
			},
		}
	}

	process1 := creator(1)
	process2 := creator(2)
	process3 := creator(3)
	process4 := creator(4)
	process5 := creator(5)

	processList := []processCache{
		process1, process4, process5, process3, process2,
	}

	tests := []struct {
		name                 string
		fields               fields
		wantMatchedProcesses []processCache
	}{
		{
			name: "only-threshold",
			fields: fields{
				MaxProcessCount:    5,
				TopNMem:            0,
				TopNCPU:            0,
				MinCPULimitPercent: 30,
				MinMemoryLimitKB:   3 * 1024,
			},
			wantMatchedProcesses: []processCache{
				process1,
				process2,
				process3,
				process4,
				process5,
			},
		},
		{
			name: "threshold-and-max",
			fields: fields{
				MaxProcessCount:    4,
				TopNMem:            0,
				TopNCPU:            0,
				MinCPULimitPercent: 30,
				MinMemoryLimitKB:   3 * 1024,
			},
			wantMatchedProcesses: []processCache{
				process1,
				process3,
				process4,
				process5,
			},
		},
		{
			name: "threshold-and-top",
			fields: fields{
				MaxProcessCount:    5,
				TopNMem:            2,
				TopNCPU:            2,
				MinCPULimitPercent: 30,
				MinMemoryLimitKB:   3 * 1024,
			},
			wantMatchedProcesses: []processCache{
				process1,
				process2,
				process5,
				process4,
			},
		},
		{
			name: "threshold-and-top-and-max",
			fields: fields{
				MaxProcessCount:    2,
				TopNMem:            2,
				TopNCPU:            2,
				MinCPULimitPercent: 30,
				MinMemoryLimitKB:   3 * 1024,
			},
			wantMatchedProcesses: []processCache{
				process4,
				process5,
			},
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			ip := &InputProcess{
				MaxProcessCount:    tt.fields.MaxProcessCount,
				TopNCPU:            tt.fields.TopNCPU,
				TopNMem:            tt.fields.TopNMem,
				MinCPULimitPercent: tt.fields.MinCPULimitPercent,
				MinMemoryLimitKB:   tt.fields.MinMemoryLimitKB,
			}
			gotMatchedProcesses := ip.filterTopAndThresholdMatchedProcesses(processList)
			sort.Slice(gotMatchedProcesses, func(i, j int) bool {
				return gotMatchedProcesses[i].GetProcessStatus().CPUPercentage.TotalPercentage <
					gotMatchedProcesses[j].GetProcessStatus().CPUPercentage.TotalPercentage
			})
			sort.Slice(tt.wantMatchedProcesses, func(i, j int) bool {
				return tt.wantMatchedProcesses[i].GetProcessStatus().CPUPercentage.TotalPercentage <
					tt.wantMatchedProcesses[j].GetProcessStatus().CPUPercentage.TotalPercentage
			})
			if !reflect.DeepEqual(gotMatchedProcesses, tt.wantMatchedProcesses) {
				t.Errorf("filterTopAndThresholdMatchedProcesses() = %v, want %v", gotMatchedProcesses, tt.wantMatchedProcesses)
			}
		})
	}
}
