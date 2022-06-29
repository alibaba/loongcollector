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

package boot

import (
	"context"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"strconv"
	"strings"

	"github.com/docker/docker/api/types"
	dockertypes "github.com/docker/docker/api/types"
	"github.com/docker/docker/api/types/filters"
	"github.com/docker/docker/client"
	"github.com/docker/go-connections/nat"
	"github.com/testcontainers/testcontainers-go"
	"github.com/testcontainers/testcontainers-go/wait"
	"gopkg.in/yaml.v3"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/config"
)

const (
	composeCategory = "docker-compose"
	finalFileName   = "testcase-compose.yaml"
	identifier      = "ilogtail-e2e"
	template        = `version: '3.8'
services:
  goc:
    image: goc-server:latest
    hostname: goc
    ports:
      - 7777:7777
    volumes:
      - %s:/coverage.log
    healthcheck:
      test: [ "CMD-SHELL", "curl -O /dev/null http://localhost:7777 || exit 1" ]
      timeout: 5s
      interval: 1s
      retries: 10
  ilogtail:
    image: aliyun/ilogtail:1.1.0
    hostname: ilogtail
    volumes:
      - %s:/ilogtail/default_flusher.json
      - %s:/ilogtail/user_local_config.json
      - /:/logtail_host
      - /var/run/docker.sock:/var/run/docker.sock
    ports:
      - 18689:18689
    environment:
      - LOGTAIL_FORCE_COLLECT_SELF_TELEMETRY=true
      - LOGTAIL_DEBUG_FLAG=true
      - LOGTAIL_AUTO_PROF=false
      - LOGTAIL_HTTP_LOAD_CONFIG=true
      - ALICLOUD_LOG_DOCKER_ENV_CONFIG=true
      - ALICLOUD_LOG_PLUGIN_ENV_CONFIG=false
      - ALIYUN_LOGTAIL_USER_DEFINED_ID=1111
`
)

// ComposeBooter control docker-compose to start or stop containers.
type ComposeBooter struct {
	cfg       *config.Case
	cli       *client.Client
	gocID     string
	logtailID string
}

// StrategyWrapper caches the real wait.StrategyTarget to get the metadata of the running containers.
type StrategyWrapper struct {
	original    wait.Strategy
	target      wait.StrategyTarget
	service     string
	virtualPort nat.Port
}

// NewComposeBooter create a new compose booter.
func NewComposeBooter(c *config.Case) *ComposeBooter {
	return &ComposeBooter{cfg: c}
}

func (c *ComposeBooter) Start() error {
	if err := c.createComposeFile(); err != nil {
		return err
	}
	compose := testcontainers.NewLocalDockerCompose([]string{config.CaseHome + finalFileName}, identifier).WithCommand([]string{"up", "-d", "--build"})
	strategyWrappers := withExposedService(compose)
	execError := compose.Invoke()
	if execError.Error != nil {
		logger.Error(context.Background(), "START_DOCKER_COMPOSE_ERROR",
			"stdout", execError.Error.Error())
		return execError.Error
	}
	cli, err := client.NewClientWithOpts(client.FromEnv)
	if err != nil {
		return err
	}
	c.cli = cli

	list, err := cli.ContainerList(context.Background(), types.ContainerListOptions{
		Filters: filters.NewArgs(filters.Arg("name", "ilogtail-e2e_ilogtail")),
	})
	if len(list) != 1 {
		logger.Errorf(context.Background(), "LOGTAIL_COMPOSE_ALARM", "logtail container size is not equal 1, got %d count", len(list))
		return err
	}
	c.logtailID = list[0].ID
	gocList, err := cli.ContainerList(context.Background(), types.ContainerListOptions{
		Filters: filters.NewArgs(filters.Arg("name", "goc")),
	})
	if len(gocList) != 1 {
		logger.Errorf(context.Background(), "LOGTAIL_COMPOSE_ALARM", "goc container size is not equal 1, got %d count", len(list))
		return err
	}
	c.gocID = gocList[0].ID

	// the docker engine cannot access host on the linux platform, more details please see: https://github.com/docker/for-linux/issues/264
	cmd := []string{
		"sh",
		"-c",
		"env |grep HOST_OS|grep Linux && ip -4 route list match 0/0|awk '{print $3\" host.docker.internal\"}' >> /etc/hosts",
	}
	if err := c.exec(c.logtailID, cmd); err != nil {
		return err
	}
	return registerDockerNetMapping(strategyWrappers)
}

func (c *ComposeBooter) Stop() error {
	// fetch logtail code coverage
	if c.gocID != "" {
		f := strings.Join(c.cfg.CoveragePackages, "|")
		cmd := "goc profile -o /coverage-raw.log"
		if f == "" {
			cmd += " && cat /coverage-raw.log >> /coverage.log"
		} else {
			cmd += " && head -n 1 /coverage-raw.log >> /coverage.log && cat /coverage-raw.log|grep -E \"" + f + "\" >> /coverage.log"
		}
		execCmd := []string{"sh", "-c", cmd}
		if err := c.exec(c.gocID, execCmd); err != nil {
			logger.Error(context.Background(), "FETCH_COVERAGE_ALARM", "err", err)
		}
	}
	execError := testcontainers.NewLocalDockerCompose([]string{config.CaseHome + finalFileName}, identifier).Down()
	if execError.Error != nil {
		logger.Error(context.Background(), "STOP_DOCKER_COMPOSE_ERROR",
			"stdout", execError.Stdout.Error(), "stderr", execError.Stderr.Error())
		return execError.Error
	}
	_ = os.Remove(config.CaseHome + finalFileName)
	return nil
}

func (c *ComposeBooter) exec(id string, cmd []string) error {
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

func (c *ComposeBooter) CopyCoreLogs() {
	if c.logtailID != "" {
		_ = os.Remove(config.LogDir)
		_ = os.Mkdir(config.LogDir, 0750)
		cmd := exec.Command("docker", "cp", c.logtailID+":/ilogtail/ilogtail.LOG", config.LogDir) //nolint:gosec
		output, err := cmd.CombinedOutput()
		logger.Debugf(context.Background(), "\n%s", string(output))
		if err != nil {
			logger.Error(context.Background(), "COPY_LOG_ALARM", "type", "main", "err", err)
		}
		cmd = exec.Command("docker", "cp", c.logtailID+":/ilogtail/logtail_plugin.LOG", config.LogDir) //nolint:gosec
		output, err = cmd.CombinedOutput()
		logger.Debugf(context.Background(), "\n%s", string(output))
		if err != nil {
			logger.Error(context.Background(), "COPY_LOG_ALARM", "type", "plugin", "err", err)
		}
	}
}

func (s *StrategyWrapper) WaitUntilReady(ctx context.Context, target wait.StrategyTarget) error {
	s.target = target
	return s.original.WaitUntilReady(ctx, target)
}

func (c *ComposeBooter) createComposeFile() error {
	// read the case docker compose file.
	_, err := os.Stat(config.CaseHome + config.DockerComposeFileName)
	var bytes []byte
	if err != nil {
		if !os.IsNotExist(err) {
			return err
		}
	} else {
		if bytes, err = ioutil.ReadFile(config.CaseHome + config.DockerComposeFileName); err != nil {
			return err
		}
	}
	cfg := c.getLogtailpluginConfig()
	// merge docker compose file.
	if len(bytes) > 0 {
		caseCfg := make(map[string]interface{})
		if err = yaml.Unmarshal(bytes, &caseCfg); err != nil {
			return err
		}
		if caseServices, ok := caseCfg["services"].(map[string]interface{}); ok {
			services := cfg["services"].(map[string]interface{})
			for name, spec := range caseServices {
				services[name] = spec
			}
		}
	}
	yml, err := yaml.Marshal(cfg)
	if err != nil {
		return err
	}
	return ioutil.WriteFile(config.CaseHome+finalFileName, yml, 0600)
}

// getLogtailpluginConfig find the docker compose configuration of the ilogtail.
func (c *ComposeBooter) getLogtailpluginConfig() map[string]interface{} {
	var envs []string
	for k, v := range c.cfg.Ilogtail.ENV {
		envs = append(envs, k+"="+v)
	}
	cfg := make(map[string]interface{})
	f, _ := os.Create(config.CoverageFile)
	_ = f.Close()
	str := fmt.Sprintf(template, config.CoverageFile, config.FlusherFile, config.ConfigFile)
	if err := yaml.Unmarshal([]byte(str), &cfg); err != nil {
		panic(err)
	}
	services := cfg["services"].(map[string]interface{})
	ilogtail := services["ilogtail"].(map[string]interface{})
	if len(envs) > 0 {
		ilogtail["environment"] = envs
	}
	if c.cfg.Ilogtail.DependsOn == nil {
		c.cfg.Ilogtail.DependsOn = make(map[string]interface{})
	}
	c.cfg.Ilogtail.DependsOn["goc"] = map[string]string{
		"condition": "service_healthy",
	}
	ilogtail["depends_on"] = c.cfg.Ilogtail.DependsOn
	for _, m := range c.cfg.Ilogtail.MountFiles {
		ilogtail["volumes"] = append(ilogtail["volumes"].([]interface{}), m)
	}
	bytes, _ := yaml.Marshal(cfg)
	println(string(bytes))
	return cfg
}

// registerDockerNetMapping
func registerDockerNetMapping(wrappers []*StrategyWrapper) error {
	for _, wrapper := range wrappers {
		host, err := wrapper.target.Host(context.Background())
		if err != nil {
			return err
		}
		physicalPort, err := wrapper.target.MappedPort(context.Background(), wrapper.virtualPort)
		if err != nil {
			return err
		}
		physicalPort.Int()
		registerNetMapping(wrapper.service+":"+wrapper.virtualPort.Port(), host+":"+physicalPort.Port())
	}
	return nil
}

// withExposedService add wait.Strategy to the docker compose.
func withExposedService(compose testcontainers.DockerCompose) (wrappers []*StrategyWrapper) {
	localCompose := compose.(*testcontainers.LocalDockerCompose)
	for serv, rawCfg := range localCompose.Services {
		cfg := rawCfg.(map[interface{}]interface{})
		rawPorts, ok := cfg["ports"]
		if !ok {
			continue
		}
		ports := rawPorts.([]interface{})
		for i := 0; i < len(ports); i++ {
			var wrapper StrategyWrapper
			var portNum int
			var np nat.Port
			switch p := ports[i].(type) {
			case int:
				portNum = p
				np = nat.Port(fmt.Sprintf("%d/tcp", p))
			case string:
				relations := strings.Split(p, ":")
				proto, port := nat.SplitProtoPort(relations[len(relations)-1])
				np = nat.Port(fmt.Sprintf("%s/%s", port, proto))
				portNum, _ = strconv.Atoi(port)
			}
			wrapper = StrategyWrapper{
				original:    wait.ForListeningPort(np),
				service:     serv,
				virtualPort: np,
			}
			localCompose.WithExposedService(serv, portNum, &wrapper)
			wrappers = append(wrappers, &wrapper)
		}
	}
	return
}
