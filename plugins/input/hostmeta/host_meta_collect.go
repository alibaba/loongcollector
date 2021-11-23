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

package hostmeta

import (
	"fmt"

	"github.com/shirou/gopsutil/cpu"
	"github.com/shirou/gopsutil/disk"
	"github.com/shirou/gopsutil/mem"
	"github.com/shirou/gopsutil/net"
)

// the types of the node metadata.
const (
	CPU    = "CPU"
	Disk   = "DISK"
	Memory = "MEM"
	Net    = "NET"
)

// metaCollectFunc describe the meta collector statement.
type metaCollectFunc func() (category string, meta interface{}, err error)

// collectCPUMeta collect the metadata of CPU.
// Although there are multi processors, only the first one metadata would be extracted to avoid
// sending large amounts of duplicate data
func collectCPUMeta() (category string, meta interface{}, err error) {
	info, err := cpu.Info()
	if err != nil {
		return CPU, nil, fmt.Errorf("error when getting the cpu metadata: %v", err)
	}
	if len(info) == 0 {
		return
	}
	metadata := make(map[string]interface{}, 8)
	phyMap := make(map[string]struct{}, 16)
	coreMap := make(map[string]struct{}, 16)
	for i := 0; i < len(info); i++ {
		phyMap[info[i].PhysicalID] = struct{}{}
		coreMap[info[i].CoreID] = struct{}{}
	}
	metadata["processor_count"] = len(info)
	metadata["core_count"] = len(phyMap) * len(coreMap)
	metadata["vendor_id"] = info[0].VendorID
	// assume that the CPU models are the same to avoid sending large amounts of duplicate data
	metadata["family"] = info[0].Family
	metadata["model"] = info[0].Model
	metadata["model_name"] = info[0].ModelName
	metadata["mhz"] = info[0].Mhz
	metadata["cache_size"] = info[0].CacheSize
	return CPU, metadata, nil
}

// collectDiskMeta collect the metadata of whole disks, so an array would be returned.
func collectDiskMeta() (category string, meta interface{}, err error) {
	partitionStats, err := disk.Partitions(false)
	if err != nil {
		return Disk, nil, fmt.Errorf("error when getting disk metadata: %v", err)
	}
	metas := make([]map[string]interface{}, len(partitionStats))
	for i, stat := range partitionStats {
		device := make(map[string]interface{}, 4)
		device["device"] = stat.Device
		device["opts"] = stat.Opts
		device["fstype"] = stat.Fstype
		device["mount_point"] = stat.Mountpoint
		metas[i] = device
	}
	return Disk, metas, nil
}

// collectMemoryMeta collect the metadata of memory.
func collectMemoryMeta() (category string, meta interface{}, err error) {
	memory, err := mem.VirtualMemory()
	if err != nil {
		return Memory, nil, fmt.Errorf("error when getting memory metadata: %v", err)
	}
	metadata := make(map[string]interface{}, 4)
	metadata["mem_total"] = memory.Total
	metadata["swap_total"] = memory.SwapTotal
	metadata["vsz_total"] = memory.VMallocTotal
	return Memory, metadata, nil
}

// collectNetMeta collect the metadata of whole net interfaces, so an array would be returned.
func collectNetMeta() (category string, meta interface{}, err error) {
	interfaces, err := net.Interfaces()
	if err != nil {
		return Memory, nil, fmt.Errorf("error when getting memory metadata: %v", err)
	}
	metadata := make([]map[string]interface{}, len(interfaces))
	for i, info := range interfaces {
		netMap := make(map[string]interface{}, 8)
		netMap["index"] = info.Index
		netMap["name"] = info.Name
		netMap["mtu"] = info.MTU
		netMap["hardware_address"] = info.HardwareAddr
		netMap["flags"] = info.Flags
		netMap["addrs"] = info.Addrs
		metadata[i] = netMap
	}
	return Net, metadata, nil
}
