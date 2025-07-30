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

//go:build linux
// +build linux

package process

import (
	"context"
	"strconv"
	"time"

	"github.com/prometheus/procfs"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/helper/containercenter"
	"github.com/alibaba/ilogtail/pkg/logger"
)

const userHZ = 100

type processCacheLinux struct {
	proc   procfs.Proc
	meta   *processMeta
	status *processStatus

	// Dynamic infos
	stat       *procfs.ProcStat   // The values in `/proc/pid/stat`
	lastStat   *procfs.ProcStat   // The last values in `/proc/pid/stat`
	procStatus *procfs.ProcStatus // The values in `/proc/pid/status`
}

func findAllProcessCache(maxLabelLength int) ([]processCache, error) {
	fs, err := procfs.NewFS(containercenter.GetMountedFilePath(procfs.DefaultMountPoint))
	if err != nil {
		logger.Error(context.Background(), "OPEN_PROCFS_ALARM", "error", err)
		return nil, err
	}
	procs, err := fs.AllProcs()
	if err != nil {
		logger.Error(context.Background(), "PROCESS_LIST_ALARM", "error", err)
		return nil, err
	}
	if len(procs) == 0 {
		return []processCache{}, nil
	}
	pcs := make([]processCache, 0, len(procs))
	for _, proc := range procs {
		pcs = append(pcs, &processCacheLinux{
			proc:   proc,
			meta:   newProcessCacheMeta(maxLabelLength),
			status: newProcessCacheStatus(),
		})
	}
	return pcs, nil
}

func (pc *processCacheLinux) Same(other processCache) bool {
	linux := other.(*processCacheLinux)
	if pc.GetPid() != linux.GetPid() {
		return false
	}
	if linux.stat == nil {
		linux.fetchStat()
	}
	if pc.stat == nil {
		pc.fetchStat()
	}
	if pc.stat == nil || linux.stat == nil {
		return false
	}
	return pc.stat.Starttime == linux.stat.Starttime
}

func (pc *processCacheLinux) GetProcessStatus() *processStatus {
	return pc.status
}

func (pc *processCacheLinux) FetchCoreCount() int64 {
	return pc.meta.fetchCoreCount
}

func (pc *processCacheLinux) FetchNetIO() bool {
	netDev, err := pc.proc.NetDev()
	if err != nil {
		logger.Debugf(context.Background(), "error when getting proc net dev: %v", err)
		return false
	}
	netDevLine := netDev.Total()
	pc.status.NetIO = &netIO{
		InPacket:  netDevLine.RxPackets,
		OutPacket: netDevLine.TxPackets,
		InBytes:   netDevLine.RxBytes,
		OutBytes:  netDevLine.TxBytes,
	}
	return true
}

func (pc *processCacheLinux) FetchThreads() bool {
	if pc.meta.fetchCoreCount > 0 {
		pc.status.ThreadsNum = pc.stat.NumThreads
		return true
	}
	return false
}

func (pc *processCacheLinux) GetPid() int {
	return pc.proc.PID
}

func (pc *processCacheLinux) GetExe() string {
	if pc.meta.exe == "" {
		if exe, err := pc.proc.Executable(); err != nil {
			logger.Debugf(context.Background(), "error when getting exe in proc: %v", err)
		} else {
			pc.meta.exe = exe
		}
	}
	return pc.meta.exe
}

func (pc *processCacheLinux) GetCmdLine() string {
	if pc.meta.cmdline == "" {
		if cmdline, err := pc.proc.CmdLine(); err != nil {
			logger.Debugf(context.Background(), "error when getting cmdline in proc: %v", err)
		} else if len(cmdline) > 0 {
			pc.meta.cmdline = cmdline[0]
		}
	}
	return pc.meta.cmdline
}

func (pc *processCacheLinux) Labels(customLabels *helper.MetricLabels) *helper.MetricLabels {
	if pc.meta.labels == nil {
		if pc.stat == nil && !pc.fetchStat() {
			return nil
		}
		processLabels := customLabels.Clone()
		processLabels.Append("pid", strconv.Itoa(pc.GetPid()))
		comm := pc.stat.Comm
		if pc.meta.maxLabelLength < len(comm) {
			logger.Warningf(context.Background(), "PROCESS_LABEL_TOO_LONG_ALARM", "the stat comm label is over %d chars: %s", pc.meta.maxLabelLength, comm)
			processLabels.Append("comm", comm[:pc.meta.maxLabelLength])
		} else {
			processLabels.Append("comm", comm)
		}
		pc.meta.labels = processLabels
	}
	return pc.meta.labels
}

func (pc *processCacheLinux) FetchFds() bool {
	//  fetch file descriptors from /proc/pid/fd
	fds, err := pc.proc.FileDescriptorsLen()
	if err != nil {
		logger.Debugf(context.Background(), "error when getting proc fd: %v", err)
		return false
	}
	pc.status.FdsNum = fds
	return true
}

func (pc *processCacheLinux) FetchCore() bool {
	pc.meta.lastFetchTime = pc.meta.nowFetchTime
	pc.meta.nowFetchTime = time.Now()
	pc.lastStat = pc.stat
	// fetch the stat and status from /proc/pid/stat and /proc/pid/status
	if !pc.fetchStat() || !pc.fetchStatus() {
		return false
	}
	pc.meta.fetchCoreCount++
	pc.status.Memory = &memory{
		Rss:  pc.procStatus.VmRSS,
		Data: pc.procStatus.VmData,
		Vsz:  pc.procStatus.VmSize,
		Swap: pc.procStatus.VmSwap,
	}
	if pc.meta.fetchCoreCount > 1 {
		totalTime := pc.meta.nowFetchTime.Sub(pc.meta.lastFetchTime).Seconds()
		pc.status.CPUPercentage = &cpuPercentage{
			TotalPercentage: 100 * float64(pc.stat.UTime+pc.stat.STime-pc.lastStat.UTime-pc.lastStat.STime) / userHZ / totalTime,
			STimePercentage: 100 * float64(pc.stat.STime-pc.lastStat.STime) / userHZ / totalTime,
			UTimePercentage: 100 * float64(pc.stat.UTime-pc.lastStat.UTime) / userHZ / totalTime,
		}
	}
	return true
}

func (pc *processCacheLinux) FetchIO() bool {
	// fetch IO from /proc/pid/io
	i, err := pc.proc.IO()
	if err != nil {
		logger.Debugf(context.Background(), "error when getting proc IO: %v", err)
		return false
	}
	pc.status.IO = &io{
		ReadeBytes: i.ReadBytes,
		WriteBytes: i.WriteBytes,
		ReadCount:  i.SyscR,
		WriteCount: i.SyscW,
	}
	return true
}

func (pc *processCacheLinux) fetchStat() bool {
	// fetch stat from /proc/pid/stat
	stat, err := pc.proc.Stat()
	if err != nil {
		logger.Debugf(context.Background(), "error when getting proc stat: %v", err)
		return false
	}
	pc.stat = &stat
	return true
}

func (pc *processCacheLinux) fetchStatus() bool {
	// fetch status from /proc/pid/status
	status, err := pc.proc.NewStatus()
	if err != nil {
		logger.Debugf(context.Background(), "error when getting proc status: %v", err)
		return false
	}
	pc.procStatus = &status
	return true
}
