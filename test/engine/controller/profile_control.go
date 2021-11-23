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

package controller

import (
	"context"
	"errors"
	"os"
	"os/exec"
	"strconv"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/boot"
)

type ProfileController struct {
	chain   *CancelChain
	seconds int
}

func (p *ProfileController) Init(parent *CancelChain, cfg *config.Case) error {
	logger.Info(context.Background(), "profile controller is initializing....")
	p.chain = WithCancelChain(parent)
	duration, err := time.ParseDuration(cfg.TestingInterval)
	if err != nil {
		return err
	}
	p.seconds = int(duration.Seconds()) - 5
	if p.seconds < 5 {
		return errors.New("the testing interval must be over 10 seconds when profile flag is open")
	}
	return nil
}

// nolint:gosec
func (p *ProfileController) Start() error {
	logger.Info(context.Background(), "profile controller is starting....")
	physicalAddress := boot.GetPhysicalAddress("ilogtail:18689")
	cpuURL := "http://" + physicalAddress + "/debug/pprof/profile?seconds=" + strconv.Itoa(p.seconds)
	memURL := "http://" + physicalAddress + "/debug/pprof/heap?seconds=" + strconv.Itoa(p.seconds)
	cpuCmd := exec.Command("go", "tool", "pprof", "-text", cpuURL)
	memCmd := exec.Command("go", "tool", "pprof", "-text", memURL)
	ouputEnv := "PPROF_TMPDIR=" + config.PprofDir
	_ = os.RemoveAll(config.PprofDir)
	cpuCmd.Env = []string{ouputEnv}
	memCmd.Env = []string{ouputEnv}
	go func() {
		cpudone := make(chan struct{})
		memdone := make(chan struct{})
		go func() {
			output, err := cpuCmd.CombinedOutput()
			logger.Debugf(context.Background(), "\n%s", string(output))
			if err != nil {
				logger.Error(context.Background(), "PROFILE_CONTROLLER_ALARM", "stage", "cpu_profile", "err", err)
			}
			cpudone <- struct{}{}
		}()
		go func() {
			output, err := memCmd.CombinedOutput()
			logger.Debugf(context.Background(), "\n%s", string(output))
			if err != nil {
				logger.Error(context.Background(), "PROFILE_CONTROLLER_ALARM", "stage", "mem_profile", "err", err)
			}
			memdone <- struct{}{}
		}()
		var cpuFinished, memFinished bool
		for {
			select {
			case <-cpudone:
				logger.Info(context.Background(), "cpu pprof is finished")
				cpuFinished = true
			case <-memdone:
				logger.Info(context.Background(), "memeory pprof is finished")
				memFinished = true
			case <-p.chain.Done():
				logger.Info(context.Background(), "profile controller is stopping....")
				if !cpuFinished {
					_ = cpuCmd.Process.Kill()
				}
				if !memFinished {
					_ = memCmd.Process.Kill()
				}
				p.chain.CancelChild()
				return
			}
		}
	}()
	return nil
}

func (p *ProfileController) Clean() {
	logger.Info(context.Background(), "profile controller is cleaning....")
}

func (p *ProfileController) CancelChain() *CancelChain {
	return p.chain
}
