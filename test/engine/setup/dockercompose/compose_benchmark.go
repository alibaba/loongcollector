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
	"os"

	"github.com/docker/docker/api/types"
	dockertypes "github.com/docker/docker/api/types"
	"github.com/docker/docker/api/types/filters"
	"github.com/docker/docker/client"
	"github.com/testcontainers/testcontainers-go"
	"gopkg.in/yaml.v3"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/config"
)

const (
	benchmarkIdentifier = "benchmark"
	cadvisorTemplate    = `version: '3.8'
services:
  cadvisor:
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
	cli        *client.Client
	cadvisorID string
}

// NewComposeBooter create a new compose booter.
func NewComposeBenchmarkBooter() *ComposeBenchmarkBooter {
	return &ComposeBenchmarkBooter{}
}

func (c *ComposeBenchmarkBooter) Start(ctx context.Context) error {
	if err := c.createComposeFile(); err != nil {
		return err
	}
	compose := testcontainers.NewLocalDockerCompose([]string{config.CaseHome + finalFileName}, benchmarkIdentifier).WithCommand([]string{"up", "-d", "--build"})
	strategyWrappers := withExposedService(compose)
	execError := compose.Invoke()
	if execError.Error != nil {
		logger.Error(context.Background(), "START_DOCKER_COMPOSE_ERROR",
			"stdout", execError.Error.Error())
		return execError.Error
	}
	cli, err := CreateDockerClient()
	if err != nil {
		return err
	}
	c.cli = cli

	list, err := cli.ContainerList(context.Background(), types.ContainerListOptions{
		Filters: filters.NewArgs(filters.Arg("name", "benchmark-cadvisor")),
	})
	if len(list) != 1 {
		logger.Errorf(context.Background(), "CADVISOR_COMPOSE_ALARM", "cadvisor container size is not equal 1, got %d count", len(list))
		return err
	}
	c.cadvisorID = list[0].ID

	// the docker engine cannot access host on the linux platform, more details please see: https://github.com/docker/for-linux/issues/264
	cmd := []string{
		"sh",
		"-c",
		"env |grep HOST_OS|grep Linux && ip -4 route list match 0/0|awk '{print $3\" host.docker.internal\"}' >> /etc/hosts",
	}
	if err = c.exec(c.cadvisorID, cmd); err != nil {
		return err
	}
	err = registerDockerNetMapping(strategyWrappers)
	logger.Debugf(context.Background(), "registered net mapping: %v", networkMapping)
	return err
}

func (c *ComposeBenchmarkBooter) Stop() error {
	execError := testcontainers.NewLocalDockerCompose([]string{config.CaseHome + finalFileName}, benchmarkIdentifier).Down()
	if execError.Error != nil {
		logger.Error(context.Background(), "STOP_DOCKER_COMPOSE_ERROR",
			"stdout", execError.Stdout.Error(), "stderr", execError.Stderr.Error())
		return execError.Error
	}
	_ = os.Remove(config.CaseHome + finalFileName)
	return nil
}

func (c *ComposeBenchmarkBooter) exec(id string, cmd []string) error {
	cfg := dockertypes.ExecConfig{
		User: "root",
		Cmd:  cmd,
	}
	resp, err := c.cli.ContainerExecCreate(context.Background(), id, cfg)
	if err != nil {
		logger.Errorf(context.Background(), "DOCKER_EXEC_ALARM", "cannot create exec config: %v", err)
		return err
	}
	err = c.cli.ContainerExecStart(context.Background(), resp.ID, dockertypes.ExecStartCheck{
		Detach: false,
		Tty:    false,
	})
	if err != nil {
		logger.Errorf(context.Background(), "DOCKER_EXEC_ALARM", "cannot start exec config: %v", err)
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
	cfg := c.getAdvisorConfig()
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
func (c *ComposeBenchmarkBooter) getAdvisorConfig() map[string]interface{} {
	cfg := make(map[string]interface{})
	f, _ := os.Create(config.CoverageFile)
	_ = f.Close()
	if err := yaml.Unmarshal([]byte(cadvisorTemplate), &cfg); err != nil {
		panic(err)
	}
	bytes, _ := yaml.Marshal(cfg)
	println(string(bytes))
	return cfg
}
