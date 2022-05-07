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

package hostmeta

import (
	"fmt"
	"strconv"

	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/logger"

	"github.com/shirou/gopsutil/process"
)

func (in *InputNodeMeta) collectProcessMeta() (nodes []*helper.MetaNode, err error) {
	processes, err := process.Processes()
	if err != nil {
		return nil, fmt.Errorf("error when getting process metadata: %v", err)
	}
	for _, p := range processes {
		if p.Pid == 0 {
			continue
		}
		if node := in.getProcessNode(p); node != nil {
			nodes = append(nodes, node)
		}
	}
	return
}

func (in *InputNodeMeta) getProcessNode(p *process.Process) (node *helper.MetaNode) {
	cmdline, err := p.Cmdline()
	if err != nil {
		logger.Debug(in.context.GetRuntimeContext(), "error in getting process cmdline", err)
		return
	}
	if cmdline == "" {
		return
	}
	exe, err := p.Exe()
	if err != nil {
		logger.Debug(in.context.GetRuntimeContext(), "error in getting process exe", err)
		return
	}
	if !in.regexMatch(cmdline) && !in.regexMatch(exe) {
		return
	}
	info, err := p.MemoryInfo()
	if err != nil {
		logger.Debug(in.context.GetRuntimeContext(), "error in getting process mem info", err)
		return
	}
	if info.RSS == 0 {
		return
	}
	name, err := p.Name()
	if err != nil {
		logger.Debug(in.context.GetRuntimeContext(), "error in getting process name", err)
		return
	}
	ppid, err := p.Ppid()
	if err != nil {
		logger.Debug(in.context.GetRuntimeContext(), "error in getting process name", err)
		return
	}
	time, _ := p.CreateTime()
	return helper.NewMetaNode(in.genProcessNodeID(strconv.Itoa(int(p.Pid)), strconv.Itoa(int(time))), Process).
		WithLabels(in.processLabels).
		WithAttribute("pid", p.Pid).
		WithAttribute("command", formatCmd(cmdline)).
		WithAttribute("exe", exe).
		WithAttribute("name", name).
		WithAttribute("ppid", ppid).
		WithParent(Host, in.getHostID(), in.hostname)
}
