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
	"os/exec"

	"gopkg.in/yaml.v3"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/test/config"
)

const (
	composeCategory = "docker-compose"
	finalFileName   = "testcase-compose.yaml"
	template        = `
services:
  loongcollectorC:
    image: aliyun/loongcollector:0.0.1
    hostname: loongcollector
    privileged: true
    pid: host
    volumes:
      - %s:/usr/local/loongcollector/conf/default_flusher.json
      - %s:/usr/local/loongcollector/conf/continuous_pipeline_config/local
      - %s:/usr/local/loongcollector/conf/onetime_pipeline_config/local
      - /:/logtail_host
      - /var/run/docker.sock:/var/run/docker.sock
      - /sys/:/sys/
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
    healthcheck:
      test: "cat /usr/local/loongcollector/log/loongcollector.LOG"
      interval: 15s
      timeout: 5s
`
)

// ComposeBooter control docker-compose to start or stop containers.
type ComposeBooter struct {
	logtailID string
}

// NewComposeBooter create a new compose booter.
func NewComposeBooter() *ComposeBooter {
	return &ComposeBooter{}
}

func (c *ComposeBooter) Start(ctx context.Context) (err error) {
	ensureComposeBuildEnv()
	if err = c.createComposeFile(ctx); err != nil {
		return err
	}
	defer func() {
		if err != nil {
			_ = c.Stop()
		}
	}()
	projectName := ComposeProjectName(config.CaseHome)
	composeFile := config.CaseHome + finalFileName
	// retry 3 times
	for i := 0; i < 3; i++ {
		execErr := runComposeCommandWithTimeout(
			ctx,
			composeStartupTimeout,
			composeFile,
			projectName,
			"up",
			"-d",
			"--build",
		)
		if execErr == nil {
			break
		}
		logger.Error(context.Background(), selfmonitor.StartDockerComposeError, "stdout", execErr.Error())
		if i == 2 {
			_ = ComposeDown(config.CaseHome)
			_ = RemoveLeftoverE2EContainers()
			return execErr
		}
		if _, downErr := runComposeCommand(
			ctx,
			composeFile,
			projectName,
			"down",
			"--volumes",
			"--remove-orphans",
		); downErr != nil {
			logger.Error(context.Background(), selfmonitor.DownDockerComposeError, "stdout", downErr.Error())
			return downErr
		}
	}
	c.logtailID, err = findSingleContainer(
		ctx,
		fmt.Sprintf("%s_loongcollectorC", projectName),
		fmt.Sprintf("%s-loongcollectorC", projectName),
	)
	if err != nil {
		logger.Error(context.Background(), selfmonitor.LoongcollectorComposeAlarm, "err", err)
		return err
	}

	// the docker engine cannot access host on the linux platform, more details please see: https://github.com/docker/for-linux/issues/264
	cmd := []string{
		"sh",
		"-c",
		"if env | grep HOST_OS | grep -q Linux; then ip -4 route list match 0/0 | awk '{print $3\" host.docker.internal\"}' >> /etc/hosts; fi",
	}
	if err = c.exec(c.logtailID, cmd); err != nil {
		logger.Error(context.Background(), selfmonitor.ExecAlarm, "err", err)
		return err
	}
	err = registerComposePortMappings(ctx, composeFile, projectName)
	logger.Debugf(context.Background(), "registered net mapping: %v", networkMapping)
	return err
}

func (c *ComposeBooter) Stop() error {
	if err := ComposeDown(config.CaseHome); err != nil {
		return err
	}
	c.logtailID = ""
	return RemoveLeftoverE2EContainers()
}

func (c *ComposeBooter) exec(id string, cmd []string) error {
	if err := execInContainer(id, cmd); err != nil {
		logger.Errorf(context.Background(), selfmonitor.DockerExecAlarm, "cannot exec command: %v", err)
		return err
	}
	return nil
}

func (c *ComposeBooter) CopyCoreLogs() {
	if c.logtailID != "" {
		_ = os.Remove(config.LogDir)
		_ = os.Mkdir(config.LogDir, 0750)
		cmd := exec.Command("docker", "cp", c.logtailID+":/usr/local/loongcollector/log/loongcollector.LOG", config.LogDir)
		output, err := cmd.CombinedOutput()
		logger.Debugf(context.Background(), "\n%s", string(output))
		if err != nil {
			logger.Error(context.Background(), selfmonitor.CopyLogAlarm, "type", "main", "err", err)
		}
		cmd = exec.Command("docker", "cp", c.logtailID+":/usr/local/loongcollector/log/go_plugin.LOG", config.LogDir)
		output, err = cmd.CombinedOutput()
		logger.Debugf(context.Background(), "\n%s", string(output))
		if err != nil {
			logger.Error(context.Background(), selfmonitor.CopyLogAlarm, "type", "plugin", "err", err)
		}
	}
}

func (c *ComposeBooter) createComposeFile(ctx context.Context) error {
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
	cfg := c.getLogtailpluginConfig()
	services := cfg["services"].(map[string]interface{})
	loongcollector := services["loongcollectorC"].(map[string]interface{})
	// merge docker compose file.
	if len(bytes) > 0 {
		caseCfg := make(map[string]interface{})
		if err = yaml.Unmarshal(bytes, &caseCfg); err != nil {
			return err
		}
		// depend on
		loongcollectorDependOn := map[string]interface{}{}
		if dependOnContainers, ok := ctx.Value(config.DependOnContainerKey).([]string); ok {
			for _, container := range dependOnContainers {
				loongcollectorDependOn[container] = map[string]string{
					"condition": "service_healthy",
				}
			}
		}
		newServices := caseCfg["services"].(map[string]interface{})
		for k := range newServices {
			services[k] = newServices[k]
		}
		loongcollector["depends_on"] = loongcollectorDependOn

		// merge top-level volumes from case compose to support named volumes in mounts
		if vols, ok := caseCfg["volumes"]; ok {
			cfg["volumes"] = vols
		}
	}
	// volume
	loongcollectorMount := services["loongcollectorC"].(map[string]interface{})["volumes"].([]interface{})
	if volumes, ok := ctx.Value(config.MountVolumeKey).([]string); ok {
		for _, volume := range volumes {
			loongcollectorMount = append(loongcollectorMount, volume)
		}
	}
	// ports
	loongcollectorPort := services["loongcollectorC"].(map[string]interface{})["ports"].([]interface{})
	if ports, ok := ctx.Value(config.ExposePortKey).([]string); ok {
		for _, port := range ports {
			loongcollectorPort = append(loongcollectorPort, port)
		}
	}
	loongcollector["volumes"] = loongcollectorMount
	loongcollector["ports"] = loongcollectorPort
	yml, err := yaml.Marshal(cfg)
	if err != nil {
		return err
	}
	return os.WriteFile(config.CaseHome+finalFileName, yml, 0600)
}

// getLogtailpluginConfig find the docker compose configuration of the loongcollector.
func (c *ComposeBooter) getLogtailpluginConfig() map[string]interface{} {
	cfg := make(map[string]interface{})
	str := fmt.Sprintf(template, config.FlusherFile, config.ConfigDir, config.OnetimeConfigDir)
	if err := yaml.Unmarshal([]byte(str), &cfg); err != nil {
		panic(err)
	}
	bytes, _ := yaml.Marshal(cfg)
	println(string(bytes))
	return cfg
}
