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
	"github.com/alibaba/ilogtail/pkg/helper"

	"time"
)

// processCache caches the process status
type processCache interface {
	GetPid() int
	Same(cache processCache) bool
	GetExe() string
	GetCmdLine() string
	FetchCoreCount() int64
	Labels(values *helper.MetricLabels) *helper.MetricLabels
	GetProcessStatus() *processStatus
	// FetchCore fetch the core exported status, such as CPU and Memory.
	FetchCore() bool
	FetchFds() bool
	FetchNetIO() bool
	FetchIO() bool
	FetchThreads() bool
}

type (
	// processMeta contains the stable process meta data.
	processMeta struct {
		maxLabelLength int
		labels         *helper.MetricLabels // The custom labels
		lastFetchTime  time.Time            // The last fetch stat time
		nowFetchTime   time.Time            // The fetch stat time
		fetchCoreCount int64                // Auto increment, the max value supports running 1462356043387 years when the fetching frequency is 5s
		cmdline        string               // The command line
		exe            string               // The absolute path of the executable command
	}

	// processStatus contains the dynamic process status.
	processStatus struct {
		FdsNum        int // The number of currently open file descriptors
		ThreadsNum    int
		CPUPercentage *cpuPercentage
		Memory        *memory
		IO            *io
		NetIO         *netIO
	}

	cpuPercentage struct {
		TotalPercentage float64
		UTimePercentage float64
		STimePercentage float64
	}
	memory struct {
		Rss  uint64
		Swap uint64
		Vsz  uint64
		Data uint64
	}
	io struct {
		ReadCount  uint64
		ReadeBytes uint64
		WriteCount uint64
		WriteBytes uint64
	}
	netIO struct {
		InPacket  uint64
		InBytes   uint64
		OutPacket uint64
		OutBytes  uint64
	}
)

func newProcessCacheMeta(maxLabelLength int) *processMeta {
	return &processMeta{
		maxLabelLength: maxLabelLength,
	}
}

func newProcessCacheStatus() *processStatus {
	return &processStatus{
		FdsNum:     -1,
		ThreadsNum: -1,
	}
}
