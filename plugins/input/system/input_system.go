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

package system

import (
	"strconv"
	"time"

	"github.com/alibaba/ilogtail/pkg/pipeline"

	"github.com/shirou/gopsutil/cpu"
	"github.com/shirou/gopsutil/disk"
	"github.com/shirou/gopsutil/host"
	"github.com/shirou/gopsutil/load"
	"github.com/shirou/gopsutil/mem"
	"github.com/shirou/gopsutil/net"
)

type InputSystem struct {
	Core     bool
	CPU      bool
	Mem      bool
	Disk     bool
	Net      bool
	Protocol bool

	CPUPercent    bool
	Disks         []string
	NetInterfaces []string

	lastInfo       *host.InfoStat
	lastCPUStat    cpu.TimesStat
	lastCPUTime    time.Time
	lastCPUTotal   float64
	lastCPUBusy    float64
	lastNetStat    net.IOCountersStat
	lastNetStatAll []net.IOCountersStat
	lastNetTime    time.Time
	lastProtoAll   []net.ProtoCountersStat
	lastProtoTime  time.Time

	lastDiskStat    disk.IOCountersStat
	lastDiskStatAll map[string]disk.IOCountersStat
	lastDiskTime    time.Time

	context pipeline.Context
}

func (r *InputSystem) Init(context pipeline.Context) (int, error) {
	r.context = context
	return 0, nil
}

func (r *InputSystem) Description() string {
	return "system metric input plugin for logtail"
}

func (r *InputSystem) CollectCore(collector pipeline.Collector) {
	fields := make(map[string]string)
	fields["metric_type"] = "core"

	// host info
	if r.lastInfo == nil {
		r.lastInfo, _ = host.Info()
	}
	if r.lastInfo != nil {
		fields["boot_time_readable"] = time.Unix(int64(r.lastInfo.BootTime), 0).Format(time.RFC3339)
		fields["boot_time"] = strconv.FormatInt(int64(r.lastInfo.BootTime), 10)
		fields["host_id"] = r.lastInfo.HostID
		fields["host_name"] = r.lastInfo.Hostname
		fields["kernel_version"] = r.lastInfo.KernelVersion
		fields["os"] = r.lastInfo.OS
		fields["platform"] = r.lastInfo.Platform
		fields["platform_family"] = r.lastInfo.PlatformFamily
		fields["platform_version"] = r.lastInfo.PlatformVersion
		fields["virtualization_role"] = r.lastInfo.VirtualizationRole
	}

	temps, err := host.SensorsTemperatures()
	if err == nil {
		for _, temp := range temps {
			fields["temperature_"+temp.SensorKey] = strconv.FormatFloat(temp.Temperature, 'f', 5, 64)
		}
	}

	// load stat
	loadStat, err := load.Avg()
	if err == nil {
		fields["load_1"] = strconv.FormatFloat(loadStat.Load1, 'f', 5, 32)
		fields["load_5"] = strconv.FormatFloat(loadStat.Load5, 'f', 5, 32)
		fields["load_15"] = strconv.FormatFloat(loadStat.Load15, 'f', 5, 32)
	}

	miscStat, err := load.Misc()
	if err == nil {
		fields["misc_procs_running"] = strconv.Itoa(miscStat.ProcsRunning)
		fields["misc_procs_blocked"] = strconv.Itoa(miscStat.ProcsBlocked)
		fields["misc_ctxt"] = strconv.Itoa(miscStat.Ctxt)
	}

	collector.AddData(nil, fields)
}

func (r *InputSystem) CollectCPU(collector pipeline.Collector) {
	fields := make(map[string]string)
	fields["metric_type"] = "cpu"
	// cpu stat
	cpuStat, err := cpu.Times(false)
	if err == nil && len(cpuStat) > 0 {
		fields["cpu_idle"] = strconv.FormatFloat(cpuStat[0].Idle, 'f', 5, 32)
		fields["cpu_iowait"] = strconv.FormatFloat(cpuStat[0].Iowait, 'f', 5, 32)
		fields["cpu_system"] = strconv.FormatFloat(cpuStat[0].System, 'f', 5, 32)
		fields["cpu_user"] = strconv.FormatFloat(cpuStat[0].User, 'f', 5, 32)
		fields["cpu_irq"] = strconv.FormatFloat(cpuStat[0].Irq, 'f', 5, 32)
		fields["cpu_softirq"] = strconv.FormatFloat(cpuStat[0].Softirq, 'f', 5, 32)
		fields["cpu_nice"] = strconv.FormatFloat(cpuStat[0].Nice, 'f', 5, 32)
		fields["cpu_steal"] = strconv.FormatFloat(cpuStat[0].Steal, 'f', 5, 32)         // Linux >= 2.6.11
		fields["cpu_guest"] = strconv.FormatFloat(cpuStat[0].Guest, 'f', 5, 32)         // Linux >= 2.6.24
		fields["cpu_guestnice"] = strconv.FormatFloat(cpuStat[0].GuestNice, 'f', 5, 32) // Linux >= 3.2.0

		cpuBusy := cpuStat[0].GuestNice + cpuStat[0].Guest + cpuStat[0].Steal + cpuStat[0].Nice +
			cpuStat[0].Softirq + cpuStat[0].Irq + cpuStat[0].User + cpuStat[0].System + cpuStat[0].Iowait
		cpuTotal := cpuBusy + cpuStat[0].Idle

		fields["cpu_total"] = strconv.FormatFloat(cpuTotal, 'f', 5, 32)
		fields["cpu_busy"] = strconv.FormatFloat(cpuBusy, 'f', 5, 32)

		deltaTotal := cpuTotal - r.lastCPUTotal
		if r.CPUPercent && !r.lastCPUTime.IsZero() && deltaTotal > 0 {
			fields["cpu_percent"] = strconv.FormatFloat(100*(cpuBusy-r.lastCPUBusy)/deltaTotal, 'f', 5, 32)
			fields["cpu_iowait_percent"] = strconv.FormatFloat(100*(cpuStat[0].Iowait-r.lastCPUStat.Iowait)/deltaTotal, 'f', 5, 32)
			fields["cpu_system_percent"] = strconv.FormatFloat(100*(cpuStat[0].System-r.lastCPUStat.System)/deltaTotal, 'f', 5, 32)
			fields["cpu_user_percent"] = strconv.FormatFloat(100*(cpuStat[0].User-r.lastCPUStat.User)/deltaTotal, 'f', 5, 32)
			fields["cpu_irq_percent"] = strconv.FormatFloat(100*(cpuStat[0].Irq-r.lastCPUStat.Irq)/deltaTotal, 'f', 5, 32)
			fields["cpu_softirq_percent"] = strconv.FormatFloat(100*(cpuStat[0].Softirq-r.lastCPUStat.Softirq)/deltaTotal, 'f', 5, 32)
			fields["cpu_nice_percent"] = strconv.FormatFloat(100*(cpuStat[0].Nice-r.lastCPUStat.Nice)/deltaTotal, 'f', 5, 32)
			fields["cpu_steal_percent"] = strconv.FormatFloat(100*(cpuStat[0].Steal-r.lastCPUStat.Steal)/deltaTotal, 'f', 5, 32)             // Linux >= 2.6.11
			fields["cpu_guest_percent"] = strconv.FormatFloat(100*(cpuStat[0].Guest-r.lastCPUStat.Guest)/deltaTotal, 'f', 5, 32)             // Linux >= 2.6.24
			fields["cpu_guestnice_percent"] = strconv.FormatFloat(100*(cpuStat[0].GuestNice-r.lastCPUStat.GuestNice)/deltaTotal, 'f', 5, 32) // Linux >= 3.2.0
		}

		r.lastCPUTime = time.Now()
		r.lastCPUStat = cpuStat[0]
		r.lastCPUBusy = cpuBusy
		r.lastCPUTotal = cpuTotal
	}
	collector.AddData(nil, fields)
}

func (r *InputSystem) CollectMem(collector pipeline.Collector) {
	fields := make(map[string]string)
	fields["metric_type"] = "mem"
	// mem stat
	memStat, err := mem.VirtualMemory()
	if err == nil {
		fields["mem"] = strconv.FormatFloat(memStat.UsedPercent, 'f', 5, 32)

		fields["mem_buffers"] = strconv.FormatUint(memStat.Buffers, 10)
		fields["mem_cached"] = strconv.FormatUint(memStat.Cached, 10)
		fields["mem_available"] = strconv.FormatUint(memStat.Available, 10)
		fields["mem_dirty"] = strconv.FormatUint(memStat.Dirty, 10)
		fields["mem_free"] = strconv.FormatUint(memStat.Free, 10)
		fields["mem_page_tables"] = strconv.FormatUint(memStat.PageTables, 10)
		fields["mem_shared"] = strconv.FormatUint(memStat.Shared, 10)
		fields["mem_used"] = strconv.FormatUint(memStat.Used, 10)
		fields["mem_swap_cached"] = strconv.FormatUint(memStat.SwapCached, 10)
		fields["mem_total"] = strconv.FormatUint(memStat.Total, 10)
	}

	swapStat, err := mem.SwapMemory()
	if err == nil {
		fields["mem_swap_free"] = strconv.FormatUint(swapStat.Free, 10)
		fields["mem_swap_total"] = strconv.FormatUint(swapStat.Total, 10)
		fields["mem_swap_used"] = strconv.FormatUint(swapStat.Used, 10)
		fields["mem_swap_percent"] = strconv.FormatFloat(swapStat.UsedPercent, 'f', 5, 32)
	}
	collector.AddData(nil, fields)
}

func collectOneDisk(collector pipeline.Collector, name string, timeDeltaSec float64, last, now *disk.IOCountersStat) {
	fields := make(map[string]string)
	fields["metric_type"] = "disk"
	fields["name"] = name
	fields["delta_seconds"] = strconv.FormatFloat(timeDeltaSec, 'f', 5, 32)

	fields["disk_read_bytes"] = strconv.FormatUint(now.ReadBytes, 10)
	fields["disk_write_bytes"] = strconv.FormatUint(now.WriteBytes, 10)
	fields["disk_read_count"] = strconv.FormatUint(now.ReadCount, 10)
	fields["disk_write_count"] = strconv.FormatUint(now.WriteCount, 10)
	fields["disk_read_time"] = strconv.FormatUint(now.ReadTime, 10)
	fields["disk_write_time"] = strconv.FormatUint(now.WriteTime, 10)
	fields["disk_iops_inprogress"] = strconv.FormatUint(now.IopsInProgress, 10)
	fields["disk_io_time"] = strconv.FormatUint(now.IoTime, 10)

	fields["disk_read_bytes_ps"] = strconv.FormatFloat((float64(now.ReadBytes-last.ReadBytes) / timeDeltaSec), 'f', 5, 32)
	fields["disk_write_bytes_ps"] = strconv.FormatFloat((float64(now.WriteBytes-last.WriteBytes) / timeDeltaSec), 'f', 5, 32)
	fields["disk_read_count_ps"] = strconv.FormatFloat((float64(now.ReadCount-last.ReadCount) / timeDeltaSec), 'f', 5, 32)
	fields["disk_write_count_ps"] = strconv.FormatFloat((float64(now.WriteCount-last.WriteCount) / timeDeltaSec), 'f', 5, 32)
	fields["disk_read_time_ps"] = strconv.FormatFloat((float64(now.ReadTime-last.ReadTime) / timeDeltaSec), 'f', 5, 32)
	fields["disk_write_time_ps"] = strconv.FormatFloat((float64(now.WriteTime-last.WriteTime) / timeDeltaSec), 'f', 5, 32)
	fields["disk_iops_inprogress_ps"] = strconv.FormatFloat((float64(now.IopsInProgress-last.IopsInProgress) / timeDeltaSec), 'f', 5, 32)
	fields["disk_io_time_ps"] = strconv.FormatFloat((float64(now.IoTime-last.IoTime) / timeDeltaSec), 'f', 5, 32)
	collector.AddData(nil, fields)
}

func (r *InputSystem) CollectDisk(collector pipeline.Collector) {
	if allParts, err := disk.Partitions(false); err == nil {
		for _, part := range allParts {
			fields := make(map[string]string)
			fields["metric_type"] = "disk"
			fields["name"] = "device"
			fields["device"] = part.Device
			fields["fstype"] = part.Fstype
			fields["mount"] = part.Mountpoint
			fields["opts"] = part.Opts

			usage, err := disk.Usage(part.Mountpoint)
			if err == nil {
				fields["disk_used_percent"] = strconv.FormatFloat(usage.UsedPercent, 'f', 5, 32)
				fields["inode_used_percent"] = strconv.FormatFloat(usage.InodesUsedPercent, 'f', 5, 32)
				fields["disk_used"] = strconv.FormatUint(usage.Used, 10)
				fields["disk_free"] = strconv.FormatUint(usage.Free, 10)
				fields["disk_total"] = strconv.FormatUint(usage.Total, 10)
				fields["inode_total"] = strconv.FormatUint(usage.InodesTotal, 10)
				fields["inode_used"] = strconv.FormatUint(usage.InodesUsed, 10)
				fields["inode_free"] = strconv.FormatUint(usage.InodesFree, 10)
			}
			collector.AddData(nil, fields)
		}
	}

	// disk stat
	allIoCounters, err := disk.IOCounters(r.Disks...)
	if err == nil {
		totalIoCount := disk.IOCountersStat{}
		for _, ioCount := range allIoCounters {
			totalIoCount.ReadBytes += ioCount.ReadBytes
			totalIoCount.WriteBytes += ioCount.WriteBytes
			totalIoCount.ReadCount += ioCount.ReadCount
			totalIoCount.WriteCount += ioCount.WriteCount
			totalIoCount.ReadTime += ioCount.ReadTime
			totalIoCount.WriteTime += ioCount.WriteTime
			totalIoCount.IopsInProgress += ioCount.IopsInProgress
			totalIoCount.IoTime += ioCount.IoTime
		}

		nowTime := time.Now()

		if !r.lastDiskTime.IsZero() {
			timeDeltaSec := float64(nowTime.Sub(r.lastDiskTime)) / float64(time.Second)
			collectOneDisk(collector, "total", timeDeltaSec, &r.lastDiskStat, &totalIoCount)
			for name, nowStat := range allIoCounters {
				if lastStatm, ok := r.lastDiskStatAll[name]; ok {
					s := nowStat
					collectOneDisk(collector, name, timeDeltaSec, &lastStatm, &s)
				}
			}
		}

		r.lastDiskTime = nowTime
		r.lastDiskStat = totalIoCount
		r.lastDiskStatAll = allIoCounters
	}
}

func collectOneNet(collector pipeline.Collector, name string, timeDeltaSec float64, last, now *net.IOCountersStat) {
	fields := make(map[string]string)
	fields["metric_type"] = "net"
	fields["name"] = name
	fields["net_in_bytes"] = strconv.FormatUint(now.BytesRecv, 10)
	fields["net_out_bytes"] = strconv.FormatUint(now.BytesSent, 10)
	fields["net_in_packet"] = strconv.FormatUint(now.PacketsRecv, 10)
	fields["net_out_packet"] = strconv.FormatUint(now.PacketsSent, 10)
	fields["net_in_error"] = strconv.FormatUint(now.Errin, 10)
	fields["net_out_error"] = strconv.FormatUint(now.Errout, 10)
	fields["net_in_drop"] = strconv.FormatUint(now.Dropin, 10)
	fields["net_out_drop"] = strconv.FormatUint(now.Dropout, 10)
	fields["net_in_fifo"] = strconv.FormatUint(now.Fifoin, 10)
	fields["net_out_fifo"] = strconv.FormatUint(now.Fifoout, 10)

	fields["net_in_bytes_ps"] = strconv.FormatFloat(float64(now.BytesRecv-last.BytesRecv)/timeDeltaSec, 'f', 5, 32)
	fields["net_out_bytes_ps"] = strconv.FormatFloat(float64(now.BytesSent-last.BytesSent)/timeDeltaSec, 'f', 5, 32)
	fields["net_in_packet_ps"] = strconv.FormatFloat(float64(now.PacketsRecv-last.PacketsRecv)/timeDeltaSec, 'f', 5, 32)
	fields["net_out_packet_ps"] = strconv.FormatFloat(float64(now.PacketsSent-last.PacketsSent)/timeDeltaSec, 'f', 5, 32)
	fields["net_in_error_ps"] = strconv.FormatFloat(float64(now.Errin-last.Errin)/timeDeltaSec, 'f', 5, 32)
	fields["net_out_error_ps"] = strconv.FormatFloat(float64(now.Errout-last.Errout)/timeDeltaSec, 'f', 5, 32)
	fields["net_in_drop_ps"] = strconv.FormatFloat(float64(now.Dropin-last.Dropin)/timeDeltaSec, 'f', 5, 32)
	fields["net_out_drop_ps"] = strconv.FormatFloat(float64(now.Dropout-last.Dropout)/timeDeltaSec, 'f', 5, 32)
	fields["net_in_fifo_ps"] = strconv.FormatFloat(float64(now.Fifoin-last.Fifoin)/timeDeltaSec, 'f', 5, 32)
	fields["net_out_fifo_ps"] = strconv.FormatFloat(float64(now.Fifoout-last.Fifoout)/timeDeltaSec, 'f', 5, 32)

	collector.AddData(nil, fields)
}

func (r *InputSystem) CollectNet(collector pipeline.Collector) {
	netIoStatAll, err := net.IOCounters(true)
	if err == nil && len(netIoStatAll) > 0 {
		netIoStatTotal := net.IOCountersStat{}

		for _, netIoStat := range netIoStatAll {
			netIoStatTotal.BytesRecv += netIoStat.BytesRecv
			netIoStatTotal.BytesSent += netIoStat.BytesSent
			netIoStatTotal.Dropin += netIoStat.Dropin
			netIoStatTotal.Dropout += netIoStat.Dropout
			netIoStatTotal.Errin += netIoStat.Errin
			netIoStatTotal.Errout += netIoStat.Errout
			netIoStatTotal.Fifoin += netIoStat.Fifoin
			netIoStatTotal.Fifoout += netIoStat.Fifoout
			netIoStatTotal.PacketsRecv += netIoStat.PacketsRecv
			netIoStatTotal.PacketsSent += netIoStat.PacketsSent
		}

		nowTime := time.Now()
		if !r.lastNetTime.IsZero() {
			timeDeltaSec := float64(nowTime.Sub(r.lastNetTime)) / float64(time.Second)
			collectOneNet(collector, "total", timeDeltaSec, &r.lastNetStat, &netIoStatTotal)
			if len(netIoStatAll) == len(r.lastNetStatAll) {
				for i := 0; i < len(netIoStatAll); i++ {
					if r.lastNetStatAll[i].Name == netIoStatAll[i].Name {
						collectOneNet(collector, netIoStatAll[i].Name, timeDeltaSec, &r.lastNetStatAll[i], &netIoStatAll[i])
					}

				}
			}

		}
		r.lastNetTime = nowTime
		r.lastNetStat = netIoStatTotal
		r.lastNetStatAll = netIoStatAll
	}
}

func (r *InputSystem) CollectProtocol(collector pipeline.Collector) {
	protoCounterStats, err := net.ProtoCounters([]string{})
	if err == nil && len(protoCounterStats) > 0 {

		nowTime := time.Now()
		if !r.lastProtoTime.IsZero() && len(protoCounterStats) == len(r.lastProtoAll) {
			timeDeltaSec := float64(nowTime.Sub(r.lastProtoTime)) / float64(time.Second)
			for index, protoStat := range protoCounterStats {
				if r.lastProtoAll[index].Protocol != protoStat.Protocol {
					continue
				}
				fields := make(map[string]string)
				fields["metric_type"] = "protocol"
				fields["protocol"] = protoStat.Protocol
				for field, val := range protoStat.Stats {
					if lastVal, ok := r.lastProtoAll[index].Stats[field]; ok {
						fields[field+"_ps"] = strconv.FormatFloat(float64(val-lastVal)/timeDeltaSec, 'f', 5, 32)
					}
					fields[field] = strconv.FormatInt(val, 10)
				}
				collector.AddData(nil, fields)
			}
		}

		r.lastProtoTime = nowTime
		r.lastProtoAll = protoCounterStats
	}
}

func (r *InputSystem) Collect(collector pipeline.Collector) error {
	if r.Core {
		r.CollectCore(collector)
	}
	if r.CPU {
		r.CollectCPU(collector)
	}
	if r.Mem {
		r.CollectMem(collector)
	}
	if r.Disk {
		r.CollectDisk(collector)
	}
	if r.Net {
		r.CollectNet(collector)
	}
	if r.Protocol {
		r.CollectProtocol(collector)
	}

	return nil
}

func init() {
	pipeline.MetricInputs["metric_system"] = func() pipeline.MetricInput {
		return &InputSystem{
			CPUPercent: true,
			Core:       true,
			CPU:        true,
			Mem:        true,
			Disk:       true,
			Net:        true,
			Protocol:   true,
		}
	}
}
