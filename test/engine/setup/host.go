// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package setup

import (
	"context"
	"fmt"

	"github.com/melbahja/goph"
	"golang.org/x/crypto/ssh"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/config"
)

type HostEnv struct {
	sshClient *goph.Client
}

func NewHostEnv() *HostEnv {
	env := &HostEnv{}
	env.initSSHClient()
	return env
}

func (h *HostEnv) GetType() string {
	return "host"
}

func (h *HostEnv) ExecOnLogtail(command string) error {
	return h.exec(command)
}

func (h *HostEnv) ExecOnSource(command string) error {
	return h.exec(command)
}

func (h *HostEnv) exec(command string) error {
	if h.sshClient == nil {
		return fmt.Errorf("ssh client init failed")
	}
	result, err := h.sshClient.Run(command)
	if err != nil {
		return fmt.Errorf("%v, %v", string(result), err)
	}
	return nil
}

func (h *HostEnv) initSSHClient() {
	client, err := goph.NewConn(&goph.Config{
		User: config.TestConfig.SSHUsername,
		Addr: config.TestConfig.SSHIP,
		Port: 22,
		Auth: goph.Password(config.TestConfig.SSHPassword),
		// #nosec G106
		Callback: ssh.InsecureIgnoreHostKey(),
	})
	if err != nil {
		logger.Errorf(context.TODO(), "SSHExec", "error in create ssh client: %v", err)
		return
	}
	h.sshClient = client
}
