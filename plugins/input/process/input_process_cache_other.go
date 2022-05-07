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

//go:build !linux
// +build !linux

package process

import (
	"context"
	"strconv"
	"time"

	"github.com/shirou/gopsutil/cpu"
	"github.com/shirou/gopsutil/process"

	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
)

type processCacheOther struct {
	meta       *processMeta
	status     *processStatus
	proc       *process.Process
	createTime int64
	commName   string
	isRunning  bool

	// Dynamic infos
	timesStat     *cpu.TimesStat
	lastTimesStat *cpu.TimesStat
}

func findAllProcessCache(maxLabelLength int) ([]processCache, error) {
	processes, err := process.Processes()
	if err != nil {
		logger.Errorf(context.Background(), "PROCESS_LIST_ALARM", "error: %v", err)
		return nil, err
	}
	if len(processes) == 0 {
		return []processCache{}, nil
	}
	caches := make([]processCache, 0, len(processes))
	for _, proc := range processes {
		createTime, err := proc.CreateTime()
		if err != nil {
			logger.Debugf(context.Background(), "PID:%d error in getting create time", proc.Pid, err)
			continue
		}
		caches = append(caches, &processCacheOther{
			meta:       newProcessCacheMeta(maxLabelLength),
			status:     newProcessCacheStatus(),
			proc:       proc,
			createTime: createTime,
			isRunning:  true,
		})
	}
	return caches, nil
}

func (pc *processCacheOther) GetProcessStatus() *processStatus {
	return pc.status
}

func (pc *processCacheOther) GetPid() int {
	return int(pc.proc.Pid)
}

func (pc *processCacheOther) Same(cache processCache) bool {
	other := cache.(*processCacheOther)
	return pc.proc.Pid == other.proc.Pid && pc.createTime == other.createTime
}

func (pc *processCacheOther) GetExe() string {
	if pc.isRunning && pc.meta.exe == "" {
		if exe, err := pc.proc.Exe(); err != nil {
			pc.tryChangeRunningStatus(err)
			logger.Debug(context.Background(), "get exe error, error", err)
		} else {
			pc.meta.exe = exe
		}
	}
	return pc.meta.exe
}

func (pc *processCacheOther) GetCmdLine() string {
	if pc.isRunning && pc.meta.cmdline == "" {
		cmdline, err := pc.proc.Cmdline()
		if err != nil {
			pc.tryChangeRunningStatus(err)
			logger.Debug(context.Background(), "get cmdline error, error", err)
			return ""
		}
		pc.meta.cmdline = cmdline
	}
	return pc.meta.cmdline
}

func (pc *processCacheOther) getCommName() string {
	if pc.isRunning && pc.commName == "" {
		name, err := pc.proc.Name()
		if err != nil {
			pc.tryChangeRunningStatus(err)
			logger.Debug(context.Background(), "get cmdline error, error", err)
			return ""
		}
		pc.commName = name
	}
	return pc.commName
}

func (pc *processCacheOther) FetchCoreCount() int64 {
	return pc.meta.fetchCoreCount
}

func (pc *processCacheOther) Labels(customLabels helper.KeyValues) string {
	if pc.meta.labels == "" {
		if name := pc.getCommName(); name != "" {
			processLabels := customLabels.Clone()
			processLabels.Append("pid", strconv.Itoa(pc.GetPid()))
			if pc.meta.maxLabelLength < len(name) {
				logger.Warningf(context.Background(), "PROCESS_LABEL_TOO_LONG_ALARM", "the stat cmdline label is over %d chars: %s", pc.meta.maxLabelLength, name)
				processLabels.Append("comm", name[:pc.meta.maxLabelLength])
			} else {
				processLabels.Append("comm", name)
			}
			processLabels.Sort()
			pc.meta.labels = processLabels.String()
		}
	}
	return pc.meta.labels
}

func (pc *processCacheOther) FetchCore() bool {
	if !pc.isRunning {
		return false
	}
	times, err := pc.proc.Times()
	if err != nil {
		pc.tryChangeRunningStatus(err)
		logger.Debug(context.Background(), "get cpu time error, error", err)
		return false
	}
	info, err := pc.proc.MemoryInfo()
	if err != nil {
		pc.tryChangeRunningStatus(err)
		logger.Debug(context.Background(), "get Memory error, error", err)
		return false
	}
	pc.meta.lastFetchTime = pc.meta.nowFetchTime
	pc.meta.nowFetchTime = time.Now()
	pc.meta.fetchCoreCount++
	pc.lastTimesStat = pc.timesStat
	pc.timesStat = times
	if pc.meta.fetchCoreCount > 1 {
		totalTime := pc.meta.nowFetchTime.Sub(pc.meta.lastFetchTime).Seconds()
		pc.status.CPUPercentage = &cpuPercentage{
			TotalPercentage: 100 * (pc.timesStat.System + pc.timesStat.User - pc.lastTimesStat.System - pc.lastTimesStat.User) / totalTime,
			STimePercentage: 100 * (pc.timesStat.System - pc.lastTimesStat.System) / totalTime,
			UTimePercentage: 100 * (pc.timesStat.User - pc.lastTimesStat.User) / totalTime,
		}
	}
	pc.status.Memory = &memory{
		Rss:  info.RSS,
		Data: info.Data,
		Vsz:  info.VMS,
		Swap: info.Swap,
	}
	return true
}

func (pc *processCacheOther) FetchFds() bool {
	if !pc.isRunning {
		return false
	}
	openFiles, err := pc.proc.OpenFiles()
	if err != nil {
		pc.tryChangeRunningStatus(err)
		logger.Debug(context.Background(), "get fds error, error", err)
		return false
	}
	pc.status.FdsNum = len(openFiles)
	return true
}

func (pc *processCacheOther) FetchNetIO() bool {
	if !pc.isRunning {
		return false
	}
	netIoCounters, err := pc.proc.NetIOCounters(false)
	if err != nil {
		pc.tryChangeRunningStatus(err)
		logger.Debug(context.Background(), "get net IO error, error", err)
		return false
	}
	pc.status.NetIO = &netIO{
		InBytes:   netIoCounters[0].BytesRecv,
		OutBytes:  netIoCounters[0].BytesSent,
		InPacket:  netIoCounters[0].PacketsRecv,
		OutPacket: netIoCounters[0].PacketsSent,
	}
	return true
}

func (pc *processCacheOther) FetchIO() bool {
	if !pc.isRunning {
		return false
	}
	ioCounters, err := pc.proc.IOCounters()
	if err != nil {
		pc.tryChangeRunningStatus(err)
		logger.Debug(context.Background(), "get IO error, error", err)
		return false
	}
	pc.status.IO = &io{
		ReadeBytes: ioCounters.ReadBytes,
		WriteBytes: ioCounters.WriteBytes,
		ReadCount:  ioCounters.ReadCount,
		WriteCount: ioCounters.WriteCount,
	}
	return true
}

func (pc *processCacheOther) FetchThreads() bool {
	if !pc.isRunning {
		return false
	}
	threads, err := pc.proc.Threads()
	if err != nil {
		pc.tryChangeRunningStatus(err)
		logger.Debug(context.Background(), "get threads error, error", err)
		return false
	}
	pc.status.ThreadsNum = len(threads)
	return true
}

func (pc *processCacheOther) tryChangeRunningStatus(err error) {
	if err == process.ErrorProcessNotRunning {
		pc.isRunning = false
		logger.Debugf(context.Background(), "PID_%d process closed", pc.GetPid())
	}
}
