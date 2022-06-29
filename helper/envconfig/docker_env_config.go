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
	"flag"
	"os"
	"runtime"

	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"

	docker "github.com/fsouza/go-dockerclient"
)

// DockerConfigInitFlag is the alibaba log docker env config flag, set yes if you want to use it. And it is also a special flag to control enable go part in ilogtail. If you just want to
// enable logtail plugin and off the env config, set the env called ALICLOUD_LOG_PLUGIN_ENV_CONFIG with false.
var DockerConfigInitFlag = flag.Bool("ALICLOUD_LOG_DOCKER_ENV_CONFIG", false, "alibaba log docker env config flag, set true if you want to use it")
var DockerConfigPluginInitFlag = flag.Bool("ALICLOUD_LOG_PLUGIN_ENV_CONFIG", true, "alibaba log docker env config flag, set true if you want to use it")

// AliCloudECSFlag set true if your docker is on alicloud ECS, so we can use ECS meta
var AliCloudECSFlag = flag.Bool("ALICLOUD_LOG_ECS_FLAG", false, "set true if your docker is on alicloud ECS, so we can use ECS meta")

// DockerConfigPrefix docker env config prefix
var DockerConfigPrefix = flag.String("ALICLOUD_LOG_DOCKER_CONFIG_PREFIX", "aliyun_logs_", "docker env config prefix")

// LogServiceEndpoint default project to create config
// https://www.alibabacloud.com/help/doc-detail/29008.htm
var LogServiceEndpoint = flag.String("ALICLOUD_LOG_ENDPOINT", "cn-hangzhou.log.aliyuncs.com", "log service endpoint of your project's region")

// DefaultLogProject default project to create config
var DefaultLogProject = flag.String("ALICLOUD_LOG_DEFAULT_PROJECT", "", "default project to create config")

// DefaultLogMachineGroup default project to create config
var DefaultLogMachineGroup = flag.String("ALICLOUD_LOG_DEFAULT_MACHINE_GROUP", "", "default project to create config")

// LogResourceCacheExpireSec log service's resources cache expire seconds
var LogResourceCacheExpireSec = flag.Int("ALICLOUD_LOG_CACHE_EXPIRE_SEC", 600, "log service's resources cache expire seconds")

// LogOperationMaxRetryTimes log service's operation max retry times
var LogOperationMaxRetryTimes = flag.Int("ALICLOUD_LOG_OPERATION_MAX_TRY", 3, "log service's operation max retry times")

// DefaultAccessKeyID your log service's access key id
var DefaultAccessKeyID = flag.String("ALICLOUD_LOG_ACCESS_KEY_ID", "xxxxxxxxx", "your log service's access key id")

// DefaultAccessKeySecret your log service's access key secret
var DefaultAccessKeySecret = flag.String("ALICLOUD_LOG_ACCESS_KEY_SECRET", "xxxxxxxxx", "your log service's access key secret")

// DefaultSTSToken your sts token
var DefaultSTSToken = flag.String("ALICLOUD_LOG_STS_TOKEN", "", "set sts token if you use sts")

// LogConfigPrefix config prefix
var LogConfigPrefix = flag.String("ALICLOUD_LOG_CONFIG_PREFIX", "aliyun_logs_", "config prefix")

// DockerEnvUpdateInterval docker env config update interval seconds
var DockerEnvUpdateInterval = flag.Int("ALICLOUD_LOG_ENV_CONFIG_UPDATE_INTERVAL", 10, "docker env config update interval seconds")

// ProductAPIDomain product domain
var ProductAPIDomain = flag.String("ALICLOUD_LOG_PRODUCT_DOMAIN", "sls.aliyuncs.com", "product domain config")

// DefaultRegion default log region"
var DefaultRegion = flag.String("ALICLOUD_LOG_REGION", "", "default log region")

var dockerEnvConfigManager = &Manager{}

var selfEnvConfig *helper.DockerInfoDetail
var selfEnvConfigFlag bool

func panicRecover() {
	if err := recover(); err != nil {
		trace := make([]byte, 2048)
		runtime.Stack(trace, true)
		logger.Error(context.Background(), "PLUGIN_RUNTIME_ALARM", "docker env conifg runtime error", err, "stack", string(trace))
	}
}

func runDockerEnvConfig() {
	defer panicRecover()
	helper.SetEnvConfigPrefix(*LogConfigPrefix)
	helper.ContainerCenterInit()
	dockerEnvConfigManager.run()
}

func initSelfEnvConfig() {
	dockerInfo := &docker.Container{}
	dockerInfo.Name = "logtail"
	dockerInfo.Config = &docker.Config{}
	dockerInfo.Config.Env = os.Environ()
	logger.Debug(context.Background(), "load self env config", dockerInfo.Config.Env)
	selfEnvConfig = helper.CreateContainerInfoDetail(dockerInfo, *LogConfigPrefix, true)
}

func initConfig() {
	_ = util.InitFromEnvBool("ALICLOUD_LOG_DOCKER_ENV_CONFIG", DockerConfigInitFlag, *DockerConfigInitFlag)
	_ = util.InitFromEnvBool("ALICLOUD_LOG_ECS_FLAG", AliCloudECSFlag, *AliCloudECSFlag)
	_ = util.InitFromEnvString("ALICLOUD_LOG_DOCKER_CONFIG_PREFIX", DockerConfigPrefix, *DockerConfigPrefix)
	_ = util.InitFromEnvString("ALICLOUD_LOG_DEFAULT_PROJECT", DefaultLogProject, *DefaultLogProject)
	_ = util.InitFromEnvString("ALICLOUD_LOG_DEFAULT_MACHINE_GROUP", DefaultLogMachineGroup, *DefaultLogMachineGroup)
	_ = util.InitFromEnvString("ALICLOUD_LOG_ENDPOINT", LogServiceEndpoint, *LogServiceEndpoint)
	_ = util.InitFromEnvString("ALICLOUD_LOG_ACCESS_KEY_ID", DefaultAccessKeyID, *DefaultAccessKeyID)
	_ = util.InitFromEnvString("ALICLOUD_LOG_ACCESS_KEY_SECRET", DefaultAccessKeySecret, *DefaultAccessKeySecret)
	_ = util.InitFromEnvString("ALICLOUD_LOG_STS_TOKEN", DefaultSTSToken, *DefaultSTSToken)
	_ = util.InitFromEnvString("ALICLOUD_LOG_CONFIG_PREFIX", LogConfigPrefix, *LogConfigPrefix)
	_ = util.InitFromEnvString("ALICLOUD_LOG_PRODUCT_DOMAIN", ProductAPIDomain, *ProductAPIDomain)
	_ = util.InitFromEnvString("ALICLOUD_LOG_REGION", DefaultRegion, *DefaultRegion)
	_ = util.InitFromEnvBool("ALICLOUD_LOG_PLUGIN_ENV_CONFIG", DockerConfigPluginInitFlag, *DockerConfigPluginInitFlag)

	if len(*DefaultRegion) == 0 {
		*DefaultRegion = util.GuessRegionByEndpoint(*LogServiceEndpoint, "cn-hangzhou")
		logger.Info(context.Background(), "guess region by endpoint, endpoint", *LogServiceEndpoint, "region", *DefaultRegion)
	}

	_ = util.InitFromEnvInt("ALICLOUD_LOG_ENV_CONFIG_UPDATE_INTERVAL", DockerEnvUpdateInterval, *DockerEnvUpdateInterval)

	if *DockerConfigInitFlag && *DockerConfigPluginInitFlag {
		// init docker config
		logger.Info(context.Background(), "init docker env config, ECS flag", *AliCloudECSFlag, "prefix", *DockerConfigPrefix, "project", *DefaultLogProject, "machine group", *DefaultLogMachineGroup, "id", *DefaultAccessKeyID)

		_ = util.InitFromEnvBool("ALICLOUD_LOG_DOCKER_ENV_CONFIG_SELF", &selfEnvConfigFlag, false)
		if selfEnvConfigFlag {
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
