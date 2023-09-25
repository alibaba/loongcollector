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

package systemv2

import (
	"bufio"
	"bytes"
	"io"
	"os"
	"strconv"
	"strings"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"github.com/prometheus/procfs"
	"github.com/shirou/gopsutil/disk"
	"github.com/shirou/gopsutil/net"
)

type tcpState uint64

const (
	// TCP_ESTABLISHED
	tcpEstablished tcpState = iota + 1
	// TCP_SYN_SENT
	tcpSynSent
	// TCP_SYN_RECV
	tcpSynRecv
	// TCP_FIN_WAIT1
	tcpFinWait1
	// TCP_FIN_WAIT2
	tcpFinWait2
	// TCP_TIME_WAIT
	tcpTimeWait
	// TCP_CLOSE
	tcpClose
	// TCP_CLOSE_WAIT
	tcpCloseWait
	// TCP_LAST_ACK
	tcpLastAck
	// TCP_LISTEN
	tcpListen
	// TCP_CLOSING
	tcpClosing
	// TCP_RX_BUFFER
	tcpRxQueuedBytes
	// TCP_TX_BUFFER
	tcpTxQueuedBytes
)

func (st tcpState) String() string {
	switch st {
	case tcpEstablished:
		return "established"
	case tcpSynSent:
		return "syn_sent"
	case tcpSynRecv:
		return "syn_recv"
	case tcpFinWait1:
		return "fin_wait1"
	case tcpFinWait2:
		return "fin_wait2"
	case tcpTimeWait:
		return "time_wait"
	case tcpClose:
		return "close"
	case tcpCloseWait:
		return "close_wait"
	case tcpLastAck:
		return "last_ack"
	case tcpListen:
		return "listen"
	case tcpClosing:
		return "closing"
	case tcpRxQueuedBytes:
		return "rx_queued_bytes"
	case tcpTxQueuedBytes:
		return "tx_queued_bytes"
	default:
		return "unknown"
	}
}

func (r *InputSystem) Init(context pipeline.Context) (int, error) {
	// mount the host proc path
	fs, err := procfs.NewFS(helper.GetMountedFilePath(procfs.DefaultMountPoint))
	if err != nil {
		logger.Error(r.context.GetRuntimeContext(), "READ_PROC_ALARM", "err", err)
		return 0, err
	}
	r.fs = &fs
	return r.CommonInit(context)
}

func (r *InputSystem) CollectTCPStats(collector pipeline.Collector, stat *net.ProtoCountersStat) {
	if !r.TCP {
		r.addMetric(collector, "protocol_tcp_established", &r.commonLabels, float64(stat.Stats["CurrEstab"]))
		return
	}

	tcp, errTCP := r.fs.NetTCP()
	if errTCP != nil {
		logger.Error(r.context.GetRuntimeContext(), "READ_PTOCTCP_ALARM", "err", errTCP)
	}
	tcp6, errTCP6 := r.fs.NetTCP6()
	if errTCP6 != nil {
		logger.Error(r.context.GetRuntimeContext(), "READ_PTOCTCP6_ALARM", "err", errTCP6)
	}
	if errTCP != nil && errTCP6 != nil {
		return
	}
	tcpStats := make(map[tcpState]uint64)
	for _, line := range tcp {
		tcpStats[tcpState(line.St)]++
		tcpStats[tcpTxQueuedBytes] += line.TxQueue
		tcpStats[tcpRxQueuedBytes] += line.RxQueue
	}
	for _, line := range tcp6 {
		tcpStats[tcpState(line.St)]++
		tcpStats[tcpTxQueuedBytes] += line.TxQueue
		tcpStats[tcpRxQueuedBytes] += line.RxQueue
	}
	for s, num := range tcpStats {
		r.addMetric(collector, "protocol_tcp_"+s.String(), &r.commonLabels, float64(num))
	}
}

func (r *InputSystem) CollectOpenFD(collector pipeline.Collector) {
	// mount the host proc path
	file, err := os.Open(helper.GetMountedFilePath("/proc/sys/fs/file-nr"))
	if err != nil {
		logger.Error(r.context.GetRuntimeContext(), "OPEN_FILENR_ALARM", "err", err)
		return
	}
	defer func(file *os.File) {
		_ = file.Close()
	}(file)
	content, err := io.ReadAll(file)
	if err != nil {
		logger.Error(r.context.GetRuntimeContext(), "READ_FILENR_ALARM", "err", err)
		return
	}
	parts := bytes.Split(bytes.TrimSpace(content), []byte("\u0009"))
	if len(parts) < 3 {
		logger.Error(r.context.GetRuntimeContext(), "FILENR_PATTERN_ALARM", "want", 3, "got", len(parts))
		return
	}
	allocated, _ := strconv.ParseFloat(string(parts[0]), 64)
	maximum, _ := strconv.ParseFloat(string(parts[2]), 64)
	r.addMetric(collector, "fd_allocated", &r.commonLabels, allocated)
	r.addMetric(collector, "fd_max", &r.commonLabels, maximum)
}

// CollectDiskUsage use `/proc/1/mounts` to find the device rather than `proc/self/mounts`
// because one device would be mounted many times in virtual environment.
func (r *InputSystem) CollectDiskUsage(collector pipeline.Collector) {
	// mount the host proc path
	file, err := os.Open(helper.GetMountedFilePath("/proc/1/mounts"))
	if err != nil {
		logger.Error(r.context.GetRuntimeContext(), "OPEN_1MOUNTS_ALARM", "err", err)
		return
	}
	defer func(file *os.File) {
		_ = file.Close()
	}(file)
	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		text := scanner.Text()
		parts := strings.Fields(text)
		if len(parts) < 4 {
			logger.Error(r.context.GetRuntimeContext(), "READ_1MOUNTS_ALARM", "got", text)
			return
		}
		if r.excludeDiskFsTypeRegex != nil && r.excludeDiskFsTypeRegex.MatchString(parts[2]) {
			logger.Debug(r.context.GetRuntimeContext(), "ignore disk path", text)
			continue
		}
		parts[1] = strings.ReplaceAll(parts[1], "\\040", " ")
		parts[1] = strings.ReplaceAll(parts[1], "\\011", "\t")
		if r.excludeDiskPathRegex != nil && r.excludeDiskPathRegex.MatchString(parts[1]) {
			logger.Debug(r.context.GetRuntimeContext(), "ignore disk path", text)
			continue
		}
		labels := r.commonLabels.Clone()
		labels.Append("path", parts[1])
		labels.Append("device", parts[0])
		labels.Append("fs_type", parts[2])
		// wrapper with mountedpath because of using unix statfs rather than proc file system.
		usage, err := disk.Usage(helper.GetMountedFilePath(parts[1]))
		if err == nil {
			r.addMetric(collector, "disk_space_usage", labels, usage.UsedPercent)
			r.addMetric(collector, "disk_inode_usage", labels, usage.InodesUsedPercent)
			r.addMetric(collector, "disk_space_used", labels, float64(usage.Used))
			r.addMetric(collector, "disk_space_total", labels, float64(usage.Total))
			r.addMetric(collector, "disk_inode_total", labels, float64(usage.InodesTotal))
			r.addMetric(collector, "disk_inode_used", labels, float64(usage.InodesUsed))
		}
	}
}
