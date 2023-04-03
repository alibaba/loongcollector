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

package envconfig

import (
	"context"
	"os"
	"runtime"

	"github.com/alibaba/ilogtail/pkg/flags"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"

	"github.com/docker/docker/api/types"
	"github.com/docker/docker/api/types/container"

	"github.com/alibaba/ilogtail/pkg/helper"
	k8s_event "github.com/alibaba/ilogtail/pkg/helper/eventrecorder"
)

var dockerEnvConfigManager = &Manager{}

var selfEnvConfig *helper.DockerInfoDetail

func panicRecover() {
	if err := recover(); err != nil {
		trace := make([]byte, 2048)
		runtime.Stack(trace, true)
		logger.Error(context.Background(), "PLUGIN_RUNTIME_ALARM", "docker env conifg runtime error", err, "stack", string(trace))
	}
}

func runDockerEnvConfig() {
	defer panicRecover()
	helper.SetEnvConfigPrefix(*flags.LogConfigPrefix)
	helper.ContainerCenterInit()
	dockerEnvConfigManager.run()
}

func initSelfEnvConfig() {
	dockerInfo := types.ContainerJSON{}
	dockerInfo.Name = "logtail"
	dockerInfo.Config = &container.Config{}
	dockerInfo.Config.Env = os.Environ()
	logger.Debug(context.Background(), "load self env config", dockerInfo.Config.Env)
	selfEnvConfig = helper.CreateContainerInfoDetail(dockerInfo, *flags.LogConfigPrefix, true)
}

func initConfig() {

	if *flags.DockerConfigInitFlag && *flags.DockerConfigPluginInitFlag {
		// init docker config
		logger.Info(context.Background(), "init docker env config, ECS flag", *flags.AliCloudECSFlag, "prefix", *flags.DockerConfigPrefix, "project", *flags.DefaultLogProject, "machine group", *flags.DefaultLogMachineGroup, "id", *flags.DefaultAccessKeyID)

		if *flags.K8sFlag {
			logger.Info(context.Background(), "init event_recorder", "")
			nodeIP := ""
			nodeName := ""
			podName := ""
			podNamespace := ""
			_ = util.InitFromEnvString("_node_ip_", &nodeIP, nodeIP)
			_ = util.InitFromEnvString("_node_name_", &nodeName, nodeName)
			_ = util.InitFromEnvString("_pod_name_", &podName, podName)
			_ = util.InitFromEnvString("_pod_namespace_", &podNamespace, podNamespace)
			k8s_event.Init(nodeIP, nodeName, podName, podNamespace)
		}

		if flags.SelfEnvConfigFlag {
			initSelfEnvConfig()
			if selfEnvConfig != nil {
				logger.Info(context.Background(), "init self env config, config count", len(selfEnvConfig.EnvConfigInfoMap))
				for key := range selfEnvConfig.EnvConfigInfoMap {
					logger.Info(context.Background(), "find self env config", key)
				}
			} else {
				logger.Info(context.Background(), "init self env config failed", "")
			}
		}
		go runDockerEnvConfig()
	}
}

func init() {
	initConfig()
}
