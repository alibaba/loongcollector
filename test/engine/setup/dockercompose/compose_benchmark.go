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

package dockercompose

import (
	"context"
	"fmt"
	"os"
	"path/filepath"

	"gopkg.in/yaml.v3"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/test/config"
)

const (
	benchmarkIdentifier = "benchmark"
	cadvisorTemplate    = `
services:
  cadvisor-%s:
    image: gcr.io/cadvisor/cadvisor:v0.49.1
    volumes:
      - /:/rootfs:ro
      - /var/run:/var/run:ro
      - /sys:/sys:ro
      - /var/lib/docker/:/var/lib/docker:ro
      - /dev/disk/:/dev/disk:ro
    ports:
      - "8080:8080"
    privileged: true
    devices:
      - /dev/kmsg
    restart: unless-stopped
`
)

// ComposeBooter control docker-compose to start or stop containers.
type ComposeBenchmarkBooter struct {
	cadvisorID string
}

// NewComposeBooter create a new compose booter.
func NewComposeBenchmarkBooter() *ComposeBenchmarkBooter {
	return &ComposeBenchmarkBooter{}
}

func (c *ComposeBenchmarkBooter) Start(ctx context.Context) error {
	ensureComposeBuildEnv()
	if err := c.createComposeFile(); err != nil {
		return err
	}
	composeFile := config.CaseHome + finalFileName
	if err := runComposeCommandWithTimeout(
		ctx,
		composeStartupTimeout,
		composeFile,
		benchmarkIdentifier,
		"up",
		"-d",
		"--build",
	); err != nil {
		logger.Error(context.Background(), selfmonitor.StartDockerComposeError, "stdout", err.Error())
		return err
	}

	var err error
	c.cadvisorID, err = findSingleContainer(ctx, "benchmark-cadvisor")
	if err != nil {
		logger.Error(context.Background(), selfmonitor.CadvisorComposeAlarm, "err", err)
		return err
	}

	// the docker engine cannot access host on the linux platform, more details please see: https://github.com/docker/for-linux/issues/264
	cmd := []string{
		"sh",
		"-c",
		"if env | grep HOST_OS | grep -q Linux; then ip -4 route list match 0/0 | awk '{print $3\" host.docker.internal\"}' >> /etc/hosts; fi",
	}
	if err = c.exec(c.cadvisorID, cmd); err != nil {
		return err
	}
	err = registerComposePortMappings(ctx, composeFile, benchmarkIdentifier)
	logger.Debugf(context.Background(), "registered net mapping: %v", networkMapping)
	return err
}

func (c *ComposeBenchmarkBooter) Stop() error {
	if err := composeDown(config.CaseHome, benchmarkIdentifier); err != nil {
		logger.Error(context.Background(), selfmonitor.StopDockerComposeError, "err", err)
		return err
	}
	c.cadvisorID = ""
	return nil
}

func (c *ComposeBenchmarkBooter) exec(id string, cmd []string) error {
	if err := execInContainer(id, cmd); err != nil {
		logger.Errorf(context.Background(), selfmonitor.DockerExecAlarm, "cannot exec command: %v", err)
		return err
	}
	return nil
}

func (c *ComposeBenchmarkBooter) CopyCoreLogs() {
}

func (c *ComposeBenchmarkBooter) createComposeFile() error {
	// read the case docker compose file.
	if _, err := os.Stat(config.CaseHome); os.IsNotExist(err) {
		if err = os.MkdirAll(config.CaseHome, 0750); err != nil {
			return err
		}
	}
	_, err := os.Stat(config.CaseHome + config.DockerComposeFileName)
	var bytes []byte
	if err != nil {
		if !os.IsNotExist(err) {
			return err
		}
	} else {
		if bytes, err = os.ReadFile(config.CaseHome + config.DockerComposeFileName); err != nil {
			return err
		}
	}
	cfg := c.getAdvisorConfig(filepath.Base(filepath.Dir(config.CaseHome)))
	services := cfg["services"].(map[string]interface{})
	// merge docker compose file.
	if len(bytes) > 0 {
		caseCfg := make(map[string]interface{})
		if err = yaml.Unmarshal(bytes, &caseCfg); err != nil {
			return err
		}
		newServices := caseCfg["services"].(map[string]interface{})
		for k := range newServices {
			services[k] = newServices[k]
		}
	}
	yml, err := yaml.Marshal(cfg)
	if err != nil {
		return err
	}
	return os.WriteFile(config.CaseHome+finalFileName, yml, 0600)
}

// getLogtailpluginConfig find the docker compose configuration of the ilogtail.
func (c *ComposeBenchmarkBooter) getAdvisorConfig(name string) map[string]interface{} {
	cfg := make(map[string]interface{})
	if err := yaml.Unmarshal([]byte(fmt.Sprintf(cadvisorTemplate, name)), &cfg); err != nil {
		panic(err)
	}
	bytes, _ := yaml.Marshal(cfg)
	println(string(bytes))
	return cfg
}
