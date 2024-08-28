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
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/shirou/gopsutil/host"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
)

const pluginType = "metric_meta_host"

const (
	Process = "PROCESS"
	Host    = "HOST"
)

type InputNodeMeta struct {
	CPU                  bool
	Memory               bool
	Net                  bool
	Disk                 bool
	Process              bool
	ProcessNamesRegex    []string          // The regular expressions for matching processes.
	Labels               map[string]string // The user custom labels.
	ProcessIntervalRound int64             // The process metadata fetched once after round times.

	count         int64
	regexpList    []*regexp.Regexp
	hostname      string
	ip            string
	hostCollects  []metaCollectFunc
	context       pipeline.Context
	hostInfo      *host.InfoStat
	processLabels map[string]string
}

func (in *InputNodeMeta) Init(context pipeline.Context) (int, error) {
	in.context = context
	for _, regStr := range in.ProcessNamesRegex {
		if reg, err := regexp.Compile(regStr); err != nil {
			logger.Error(in.context.GetRuntimeContext(), "INVALID_REGEX_ALARM", "invalid regex", regStr, "error", err)
		} else {
			in.regexpList = append(in.regexpList, reg)
		}
	}
	if in.CPU {
		in.hostCollects = append(in.hostCollects, collectCPUMeta)
	}
	if in.Memory {
		in.hostCollects = append(in.hostCollects, collectMemoryMeta)
	}
	if in.Disk {
		in.hostCollects = append(in.hostCollects, collectDiskMeta)
	}
	if in.Net {
		in.hostCollects = append(in.hostCollects, collectNetMeta)
	}
	if in.Process {
		in.processLabels = map[string]string{"hostname": in.hostname, "ip": in.ip}
		for k, v := range in.Labels {
			in.processLabels[k] = v
		}
	}
	return 0, nil
}

func (in *InputNodeMeta) Description() string {
	return "Collect the host metadata"
}

func (in *InputNodeMeta) Collect(collector pipeline.Collector) error {
	now := time.Now()
	if len(in.hostCollects) > 0 {
		node, err := in.collectHostMeta()
		if err != nil {
			return err
		}
		helper.AddMetadata(collector, now, node)
	}
	if in.Process && in.count%in.ProcessIntervalRound == 0 {
		nodes, err := in.collectProcessMeta()
		if err != nil {
			return err
		}
		for _, node := range nodes {
			helper.AddMetadata(collector, now, node)
		}
	}
	in.count++
	return nil
}

func (in *InputNodeMeta) collectHostMeta() (node *helper.MetaNode, err error) {
	info, err := in.getHostInfo()
	if err != nil {
		return
	}
	node = helper.NewMetaNode(in.getHostID(), Host).
		WithLabel("hostname", in.hostname).
		WithLabel("ip", in.ip).
		WithLabel("boot_time", strconv.Itoa(int(info.BootTime))).
		WithLabel("os", info.OS).
		WithLabel("platform", info.Platform).
		WithLabel("platform_family", info.PlatformFamily).
		WithLabel("platform_version", info.PlatformVersion).
		WithLabel("kernel_version", info.KernelVersion).
		WithLabel("kernel_arch", info.KernelArch).
		WithLabel("virtualization_system", info.VirtualizationSystem).
		WithLabel("virtualization_role", info.VirtualizationRole).
		WithLabel("host_id", info.HostID)
	for k, v := range in.Labels {
		node.WithLabel(k, v)
	}
	for _, collect := range in.hostCollects {
		category, meta, err := collect()
		if err != nil {
			logger.Error(in.context.GetRuntimeContext(), "FAILED_COLLECT_HOST_METADATA", "error", err)
			continue
		}
		node.WithAttribute(category, meta)
	}
	return
}

// genProcessNodeID create the process ID according to the host, pid, and startTime.
func (in *InputNodeMeta) genProcessNodeID(pid, startTime string) string {
	return strings.Join([]string{in.hostname, in.ip, Process, pid, startTime}, "_")
}

func (in *InputNodeMeta) regexMatch(str string) bool {
	if len(in.regexpList) == 0 {
		return true
	}
	for _, r := range in.regexpList {
		if r.MatchString(str) {
			return true
		}
	}
	return false
}

func (in *InputNodeMeta) getHostInfo() (*host.InfoStat, error) {
	if in.hostInfo == nil {
		info, err := host.Info()
		if err != nil {
			return nil, fmt.Errorf("error when getting host metadata: %v", err)
		}
		in.hostInfo = info
	}
	return in.hostInfo, nil
}

func (in *InputNodeMeta) getHostID() string {
	info, err := in.getHostInfo()
	if err != nil {
		return "no_hostID" + "_" + in.ip
	}
	return info.HostID + "_" + in.ip
}

// formatCmd reduce the length of the cmd when the cmd is over 8000 chars.
func formatCmd(cmd string) string {
	if len(cmd) > 8000 {
		prefix := cmd[:4000]
		suffix := cmd[len(cmd)-4000:]
		return prefix + " ... " + suffix
	}
	return cmd
}

func init() {
	pipeline.MetricInputs[pluginType] = func() pipeline.MetricInput {
		return &InputNodeMeta{
			CPU:                  true,
			Memory:               true,
			ProcessIntervalRound: 5,
			hostname:             util.GetHostName(),
			ip:                   util.GetIPAddress(),
		}
	}
}
