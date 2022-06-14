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

package hostmeta

import (
	"fmt"
	"strconv"
	"strings"

	"github.com/prometheus/procfs"

	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
)

func (in *InputNodeMeta) collectProcessMeta() (nodes []*helper.MetaNode, err error) {
	procs, err := procfs.AllProcs()
	if err != nil {
		return nil, fmt.Errorf("error when getting process metadata: %v", err)
	}
	for i := 0; i < len(procs); i++ {
		if procs[i].PID == 0 {
			continue
		}
		if node := in.getProcessNode(&procs[i]); node != nil {
			nodes = append(nodes, node)
		}
	}
	return
}

func (in *InputNodeMeta) getProcessNode(proc *procfs.Proc) (node *helper.MetaNode) {
	cmdline, err := proc.CmdLine()
	if err != nil {
		logger.Debug(in.context.GetRuntimeContext(), "error in getting process cmdline", err)
		return
	}
	if len(cmdline) == 0 {
		return
	}
	exe, err := proc.Executable()
	if err != nil {
		logger.Debug(in.context.GetRuntimeContext(), "error in getting process exe", err)
		return
	}
	cmd := strings.Join(cmdline, " ")
	if !in.regexMatch(cmd) && !in.regexMatch(exe) {
		return
	}
	stat, err := proc.Stat()
	if err != nil {
		logger.Debug(in.context.GetRuntimeContext(), "error in getting process stat", err)
		return
	}
	if stat.RSS == 0 {
		return
	}
	node = helper.NewMetaNode(in.genProcessNodeID(strconv.Itoa(proc.PID), strconv.Itoa(int(stat.Starttime))), Process).
		WithLabels(in.processLabels).
		WithAttribute("pid", proc.PID).
		WithAttribute("command", formatCmd(cmd)).
		WithAttribute("exe", exe).
		WithAttribute("name", stat.Comm).
		WithAttribute("ppid", stat.PPID).
		WithParent(Host, in.getHostID(), in.hostname)
	return
}
