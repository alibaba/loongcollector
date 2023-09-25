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

package systemv2

import (
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"github.com/shirou/gopsutil/disk"
	"github.com/shirou/gopsutil/net"
)

func (r *InputSystem) Init(context pipeline.Context) (int, error) {
	return r.CommonInit(context)
}

func (r *InputSystem) CollectTCPStats(collector pipeline.Collector, stat *net.ProtoCountersStat) {
	r.addMetric(collector, "protocol_tcp_established", &r.commonLabels, float64(stat.Stats["CurrEstab"]))
}

func (r *InputSystem) CollectOpenFD(collector pipeline.Collector) {

}

func (r *InputSystem) CollectDiskUsage(collector pipeline.Collector) {
	if allParts, err := disk.Partitions(false); err == nil {
		for _, part := range allParts {
			if r.excludeDiskFsTypeRegex != nil && r.excludeDiskFsTypeRegex.MatchString(part.Fstype) {
				logger.Debug(r.context.GetRuntimeContext(), "ignore disk path", part.Mountpoint)
				continue
			}
			if r.excludeDiskPathRegex != nil && r.excludeDiskPathRegex.MatchString(part.Mountpoint) {
				logger.Debug(r.context.GetRuntimeContext(), "ignore disk path", part.Mountpoint)
				continue
			}
			labels := r.commonLabels.Clone()
			labels.Append("path", part.Mountpoint)
			labels.Append("device", part.Device)
			labels.Append("fs_type", part.Fstype)

			usage, err := disk.Usage(part.Mountpoint)
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
}
