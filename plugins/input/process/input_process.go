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
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"

	"regexp"
	"sort"
	"time"
)

const (
	defaultMaxProcessCount     = 100
	defaultMaxIdentifierLength = 100
)

// InputProcess plugin is modified with care, because two collect libs are used， which are procfs and gopsutil.
// They are works well on the host machine. But on the linux virtual environment, they are different.
// The procfs or read proc file system should mount the `logtail_host` path, more details please see `helper.mount_others.go`.
// The gopsutil lib only support mount path with ENV config, more details please see `github.com/shirou/gopsutil/internal/common/common.go`.
type InputProcess struct {
	MaxIdentifierLength int               // The maximum length for the process identifier ，such as CMD and CWD.
	MaxProcessCount     int               // The maximum count of the processes.
	TopNCPU             int               // The number of the selected processes that order by the CPU usage.
	TopNMem             int               // The number of the selected processes that order by the Memory usage.
	MinCPULimitPercent  float64           // The minimum CPU percentage for collecting.
	MinMemoryLimitKB    int               // The minimum Memory usage for collecting.
	ProcessNamesRegex   []string          // The regular expressions for matching processes.
	Labels              map[string]string // The user custom labels.
	// The optional metric switches
	OpenFD bool
	Thread bool
	NetIO  bool
	IO     bool

	context       pipeline.Context
	lastProcesses map[int]processCache
	regexpList    []*regexp.Regexp
	commonLabels  helper.MetricLabels
	collectTime   time.Time
}

func (ip *InputProcess) Init(context pipeline.Context) (int, error) {
	ip.context = context
	if ip.MaxProcessCount <= 0 {
		ip.MaxProcessCount = defaultMaxProcessCount
	}
	if ip.MaxIdentifierLength <= 0 {
		ip.MaxIdentifierLength = defaultMaxIdentifierLength
	}
	for _, regStr := range ip.ProcessNamesRegex {
		if reg, err := regexp.Compile(regStr); err == nil {
			ip.regexpList = append(ip.regexpList, reg)
		} else {
			logger.Error(ip.context.GetRuntimeContext(), "INVALID_REGEX_ALARM", "invalid regex", regStr, "error", err)
		}
	}
	ip.commonLabels.Append("hostname", util.GetHostName())
	ip.commonLabels.Append("ip", util.GetIPAddress())
	for key, val := range ip.Labels {
		ip.commonLabels.Append(key, val)
	}
	return 0, nil
}

func (ip *InputProcess) Description() string {
	return "Support collect process metrics on the host machine or Linux virtual environments."
}

func (ip *InputProcess) Collect(collector pipeline.Collector) error {
	ip.collectTime = time.Now()
	matchedProcesses, err := ip.filterMatchedProcesses()
	if err != nil {
		return err
	}
	for _, pc := range matchedProcesses {
		labels := pc.Labels(&ip.commonLabels)
		// add necessary metrics
		ip.addCPUMetrics(pc, labels, collector)
		ip.addMemMetrics(pc, labels, collector)
		// add optional metrics
		if ip.Thread {
			ip.addThreadMetrics(pc, labels, collector)
		}
		if ip.OpenFD {
			ip.addOpenFilesMetrics(pc, labels, collector)
		}
		if ip.NetIO {
			ip.addNetIOMetrics(pc, labels, collector)
		}
		if ip.IO {
			ip.addIOMetrics(pc, labels, collector)
		}
	}
	return nil
}

// filterMatchedProcesses select the matched processing in the whole processes.
func (ip *InputProcess) filterMatchedProcesses() (matchedProcesses []processCache, err error) {
	caches, err := findAllProcessCache(ip.MaxProcessCount)
	if err != nil {
		logger.Error(ip.context.GetRuntimeContext(), "PROCESS_LIST_ALARM", "error", err)
		return
	}
	matchedProcesses = ip.filterRegexMatchedProcess(caches)
	matchedProcesses = ip.filterTopAndThresholdMatchedProcesses(matchedProcesses)
	return
}

// filterRegexMatchedProcess select the processes matched the regular expressions and load the necessary
// process status, such as memory and CPU.
func (ip *InputProcess) filterRegexMatchedProcess(caches []processCache) (matchedProcesses []processCache) {
	newProcessesMap := make(map[int]processCache)
	matchedProcesses = make([]processCache, 0, util.MinInt(ip.MaxProcessCount, len(caches)))
	regexpChecker := func(name string) bool {
		for _, r := range ip.regexpList {
			if r.MatchString(name) {
				return true
			}
		}
		return false
	}
	for _, pc := range caches {
		// filter by history cache processes or regex conditions.
		if lpc, ok := ip.lastProcesses[pc.GetPid()]; ok && pc.Same(lpc) {
			pc = lpc
		} else if len(ip.regexpList) > 0 && !regexpChecker(pc.GetExe()) && !regexpChecker(pc.GetCmdLine()) {
			continue
		}
		if !pc.FetchCore() {
			continue
		}
		if pc.FetchCoreCount() > 1 {
			matchedProcesses = append(matchedProcesses, pc)
		}
		newProcessesMap[pc.GetPid()] = pc
	}
	ip.lastProcesses = newProcessesMap
	return
}

// filterTopAndThresholdMatchedProcesses select the processes by the following conditions includes the limit number and the threshold.
func (ip *InputProcess) filterTopAndThresholdMatchedProcesses(processList []processCache) (matchedProcesses []processCache) {
	inChecker := func(checkOne processCache, container []processCache) bool {
		for _, pc := range container {
			if pc.Same(checkOne) {
				return true
			}
		}
		return false
	}
	// select the processes matched the CPU and Memory threshold
	thresholdMatchedProcesses := make([]processCache, 0, 16)
	for _, pc := range processList {
		if pc.GetProcessStatus().CPUPercentage.TotalPercentage >= ip.MinCPULimitPercent {
			thresholdMatchedProcesses = append(thresholdMatchedProcesses, pc)
		}
	}
	for _, pc := range processList {
		if inChecker(pc, thresholdMatchedProcesses) {
			continue
		}
		if pc.GetProcessStatus().Memory.Rss >= uint64(ip.MinMemoryLimitKB)*1024 {
			thresholdMatchedProcesses = append(thresholdMatchedProcesses, pc)
		}
	}

	if ip.TopNMem <= 0 && ip.TopNCPU <= 0 {
		if ip.MaxProcessCount < len(thresholdMatchedProcesses) {
			matchedProcesses = thresholdMatchedProcesses[:ip.MaxProcessCount]
		} else {
			matchedProcesses = thresholdMatchedProcesses
		}
	}
	// select the processes matched the CPU or Memory top condition
	if ip.TopNCPU > 0 {
		sort.Slice(thresholdMatchedProcesses, func(i, j int) bool {
			return thresholdMatchedProcesses[i].GetProcessStatus().CPUPercentage.TotalPercentage >
				thresholdMatchedProcesses[j].GetProcessStatus().CPUPercentage.TotalPercentage
		})
		appendCount := util.MinInt(ip.MaxProcessCount, util.MinInt(len(thresholdMatchedProcesses), ip.TopNCPU))
		for i := 0; i < appendCount; i++ {
			matchedProcesses = append(matchedProcesses, thresholdMatchedProcesses[i])
		}
	}
	if ip.TopNMem > 0 {
		sort.Slice(thresholdMatchedProcesses, func(i, j int) bool {
			return thresholdMatchedProcesses[i].GetProcessStatus().Memory.Rss >
				thresholdMatchedProcesses[j].GetProcessStatus().Memory.Rss
		})
		appendCount := util.MinInt(ip.MaxProcessCount, util.MinInt(len(thresholdMatchedProcesses), ip.TopNMem))
		for i := 0; i < appendCount; i++ {
			if len(matchedProcesses) == ip.MaxProcessCount {
				break
			}
			if inChecker(thresholdMatchedProcesses[i], matchedProcesses) {
				continue
			}
			matchedProcesses = append(matchedProcesses, thresholdMatchedProcesses[i])
		}
	}
	return
}

func (ip *InputProcess) addCPUMetrics(pc processCache, labels *helper.MetricLabels, collector pipeline.Collector) {
	if percentage := pc.GetProcessStatus().CPUPercentage; percentage != nil {

		ip.addMetric(collector, "process_cpu_percent", &ip.collectTime, labels, percentage.TotalPercentage)
		ip.addMetric(collector, "process_cpu_stime_percent", &ip.collectTime, labels, percentage.STimePercentage)
		ip.addMetric(collector, "process_cpu_utime_percent", &ip.collectTime, labels, percentage.UTimePercentage)
	}
}

func (ip *InputProcess) addMetric(collector pipeline.Collector, name string, t *time.Time, labels *helper.MetricLabels, val float64) {
	collector.AddRawLog(helper.NewMetricLog(name, t.UnixNano(), val, labels))
}

func (ip *InputProcess) addMemMetrics(pc processCache, labels *helper.MetricLabels, collector pipeline.Collector) {
	if mem := pc.GetProcessStatus().Memory; mem != nil {
		ip.addMetric(collector, "process_mem_rss", &ip.collectTime, labels, float64(mem.Rss))
		ip.addMetric(collector, "process_mem_swap", &ip.collectTime, labels, float64(mem.Swap))
		ip.addMetric(collector, "process_mem_vsz", &ip.collectTime, labels, float64(mem.Vsz))
		ip.addMetric(collector, "process_mem_data", &ip.collectTime, labels, float64(mem.Data))
	}
}

func (ip *InputProcess) addThreadMetrics(pc processCache, labels *helper.MetricLabels, collector pipeline.Collector) {
	if pc.FetchThreads() {
		ip.addMetric(collector, "process_threads", &ip.collectTime, labels, float64(pc.GetProcessStatus().ThreadsNum))
	}
}

func (ip *InputProcess) addOpenFilesMetrics(pc processCache, labels *helper.MetricLabels, collector pipeline.Collector) {
	if pc.FetchFds() {
		ip.addMetric(collector, "process_fds", &ip.collectTime, labels, float64(pc.GetProcessStatus().FdsNum))
	}
}

func (ip *InputProcess) addNetIOMetrics(pc processCache, labels *helper.MetricLabels, collector pipeline.Collector) {
	if pc.FetchNetIO() {
		net := pc.GetProcessStatus().NetIO
		ip.addMetric(collector, "process_net_in_bytes", &ip.collectTime, labels, float64(net.InBytes))
		ip.addMetric(collector, "process_net_in_packet", &ip.collectTime, labels, float64(net.InPacket))
		ip.addMetric(collector, "process_net_out_bytes", &ip.collectTime, labels, float64(net.OutBytes))
		ip.addMetric(collector, "process_net_out_packet", &ip.collectTime, labels, float64(net.OutPacket))
	}
}

func (ip *InputProcess) addIOMetrics(pc processCache, labels *helper.MetricLabels, collector pipeline.Collector) {
	if pc.FetchIO() {
		io := pc.GetProcessStatus().IO
		ip.addMetric(collector, "process_read_bytes", &ip.collectTime, labels, float64(io.ReadeBytes))
		ip.addMetric(collector, "process_write_bytes", &ip.collectTime, labels, float64(io.WriteBytes))
		ip.addMetric(collector, "process_read_count", &ip.collectTime, labels, float64(io.ReadCount))
		ip.addMetric(collector, "process_write_count", &ip.collectTime, labels, float64(io.WriteCount))
	}
}

func init() {
	pipeline.MetricInputs["metric_process_v2"] = func() pipeline.MetricInput {
		return &InputProcess{
			TopNCPU:          5,
			MinMemoryLimitKB: 100,
			lastProcesses:    make(map[int]processCache),
		}
	}
}
