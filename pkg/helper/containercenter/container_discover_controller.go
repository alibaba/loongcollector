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

package containercenter

import (
	"context"
	"fmt"
	"os"
	"strconv"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"
)

var FetchAllInterval = time.Second * time.Duration(300)

// fetchAllSuccessTimeout controls when to force timeout containers if fetchAll
// failed continuously. By default, 20 times of FetchAllInterval.
// current incremental discovery does not refresh container's lastUpdateTime, so this value must be greater than FetchAllInterval
var fetchAllSuccessTimeout = FetchAllInterval * 20
var ContainerCenterTimeout = time.Second * time.Duration(30)
var MaxFetchOneTriggerPerSecond int32 = 200

type ContainerDiscoverManager struct {
	enableDockerDiscover bool // maybe changed
	enableCRIDiscover    bool
	enableStaticDiscover bool

	fetchOneCount    int32 // only limit the frequency of FetchOne
	lastFetchOneTime int64
	fetchOneLock     sync.Mutex
}

func NewContainerDiscoverManager() *ContainerDiscoverManager {
	return &ContainerDiscoverManager{
		enableDockerDiscover: false,
		enableCRIDiscover:    false,
		enableStaticDiscover: false,
	}
}

// FetchAll
// Currently, there are 3 ways to find containers, which are docker interface, cri interface and static container info file.
func (c *ContainerDiscoverManager) FetchAll() {
	if c.enableStaticDiscover {
		c.fetchStatic()
	}

	var err error
	if c.enableDockerDiscover {
		if err = c.fetchDocker(); err != nil {
			logger.Info(context.Background(), "container docker fetch all", err)
		}
	}

	if c.enableCRIDiscover {
		if err = c.fetchCRI(); err != nil {
			logger.Info(context.Background(), "container CRIRuntime fetch all", err)
		}
	}
}

func (c *ContainerDiscoverManager) FetchOne(containerID string) error {
	logger.Debug(context.Background(), "discover manager fetch one", containerID)
	now := time.Now().Unix()
	c.fetchOneLock.Lock()
	if now > c.lastFetchOneTime {
		c.lastFetchOneTime = now
		c.fetchOneCount = 0
	}
	c.fetchOneCount++
	if c.fetchOneCount > MaxFetchOneTriggerPerSecond {
		logger.Debug(context.Background(), "discover manager reject because of reaching the maximum fetch count", containerID)
		c.fetchOneLock.Unlock()
		return fmt.Errorf("cannot fetch %s because of reaching the maximum fetch count", containerID)
	}
	c.fetchOneLock.Unlock()
	var err error
	if c.enableCRIDiscover {
		err = criRuntimeWrapper.fetchOne(containerID)
		logger.Debug(context.Background(), "discover manager cri fetch one status", err == nil)
		if err == nil {
			return nil
		}
	}
	if c.enableDockerDiscover {
		err = containerCenterInstance.fetchOne(containerID, true)
		logger.Debug(context.Background(), "discover manager docker fetch one status", err == nil)
	}
	return err
}

func (c *ContainerDiscoverManager) fetchDocker() error {
	if containerCenterInstance == nil {
		return nil
	}
	return containerCenterInstance.fetchAll()
}

func (c *ContainerDiscoverManager) fetchStatic() {
	if containerCenterInstance == nil {
		return
	}
	containerCenterInstance.readStaticConfig(true)
}

func (c *ContainerDiscoverManager) fetchCRI() error {
	if criRuntimeWrapper == nil {
		return nil
	}
	return criRuntimeWrapper.fetchAll()
}

func (c *ContainerDiscoverManager) StartSyncContainers() {
	if c.enableCRIDiscover {
		logger.Debug(context.Background(), "discover manager start sync containers goroutine", "cri")
		go criRuntimeWrapper.loopSyncContainers()
	}
	if c.enableStaticDiscover {
		logger.Debug(context.Background(), "discover manager start sync containers goroutine", "static")
		go containerCenterInstance.flushStaticConfig()
	}
	if c.enableDockerDiscover {
		logger.Debug(context.Background(), "discover manager start sync containers goroutine", "docker")
		go containerCenterInstance.eventListener()
	}
}

func (c *ContainerDiscoverManager) Clean() {
	if criRuntimeWrapper != nil {
		criRuntimeWrapper.sweepCache()
		logger.Debug(context.Background(), "discover manager clean", "cri")
	}
	if containerCenterInstance != nil {
		containerCenterInstance.sweepCache()
		logger.Debug(context.Background(), "discover manager clean", "docker")
	}
}

func (c *ContainerDiscoverManager) LogAlarm(err error, msg string) {
	if err != nil {
		logger.Warning(context.Background(), "DOCKER_CENTER_ALARM", "message", msg, "error found", err)
	} else {
		logger.Debug(context.Background(), "message", msg)
	}
}

func (c *ContainerDiscoverManager) Init() bool {
	defer containerCenterRecover()

	// discover which runtime is valid
	if wrapper, err := NewCRIRuntimeWrapper(containerCenterInstance); err != nil {
		logger.Warningf(context.Background(), "DOCKER_CENTER_ALARM", "[CRIRuntime] creare cri-runtime client error: %v", err)
		criRuntimeWrapper = nil
	} else {
		logger.Infof(context.Background(), "[CRIRuntime] create cri-runtime client successfully")
		criRuntimeWrapper = wrapper
	}
	if ok, err := util.PathExists(DefaultLogtailMountPath); err == nil {
		if !ok {
			logger.Info(context.Background(), "no docker mount path", "set empty")
			DefaultLogtailMountPath = ""
		}
	} else {
		logger.Warning(context.Background(), "check docker mount path error", err.Error())
	}
	c.enableCRIDiscover = criRuntimeWrapper != nil
	c.enableDockerDiscover = containerCenterInstance.initClient() == nil
	c.enableStaticDiscover = isStaticContainerInfoEnabled()
	discoverdRuntime := c.enableCRIDiscover || c.enableDockerDiscover || c.enableStaticDiscover
	if !discoverdRuntime {
		return false
	}

	// try to connect to runtime
	logger.Info(context.Background(), "input", "param", "docker discover", c.enableDockerDiscover, "cri discover", c.enableCRIDiscover, "static discover", c.enableStaticDiscover)
	listenLoopIntervalSec := 0
	// Get env in the same order as in C Logtail
	listenLoopIntervalStr := os.Getenv("docker_config_update_interval")
	if len(listenLoopIntervalStr) > 0 {
		listenLoopIntervalSec, _ = strconv.Atoi(listenLoopIntervalStr)
	}
	listenLoopIntervalStr = os.Getenv("ALIYUN_LOGTAIL_DOCKER_CONFIG_UPDATE_INTERVAL")
	if len(listenLoopIntervalStr) > 0 {
		listenLoopIntervalSec, _ = strconv.Atoi(listenLoopIntervalStr)
	}
	// Keep this env var for compatibility
	listenLoopIntervalStr = os.Getenv("CONTAINERD_LISTEN_LOOP_INTERVAL")
	if len(listenLoopIntervalStr) > 0 {
		listenLoopIntervalSec, _ = strconv.Atoi(listenLoopIntervalStr)
	}
	if listenLoopIntervalSec > 0 {
		DefaultSyncContainersPeriod = time.Second * time.Duration(listenLoopIntervalSec)
	}
	// @note config for Fetch All Interval
	fetchAllSec := (int)(FetchAllInterval.Seconds())
	if err := util.InitFromEnvInt("DOCKER_FETCH_ALL_INTERVAL", &fetchAllSec, fetchAllSec); err != nil {
		c.LogAlarm(err, "initialize env DOCKER_FETCH_ALL_INTERVAL error")
	}
	if fetchAllSec > 0 && fetchAllSec < 3600*24 {
		FetchAllInterval = time.Duration(fetchAllSec) * time.Second
	}
	logger.Info(context.Background(), "init docker center, fetch all seconds", FetchAllInterval.String())
	{
		timeoutSec := int(fetchAllSuccessTimeout.Seconds())
		if err := util.InitFromEnvInt("DOCKER_FETCH_ALL_SUCCESS_TIMEOUT", &timeoutSec, timeoutSec); err != nil {
			c.LogAlarm(err, "initialize env DOCKER_FETCH_ALL_SUCCESS_TIMEOUT error")
		}
		if timeoutSec > int(FetchAllInterval.Seconds()) && timeoutSec <= 3600*24 {
			fetchAllSuccessTimeout = time.Duration(timeoutSec) * time.Second
		}
	}
	logger.Info(context.Background(), "init docker center, fecth all success timeout", fetchAllSuccessTimeout.String())
	{
		timeoutSec := int(ContainerCenterTimeout.Seconds())
		if err := util.InitFromEnvInt("DOCKER_CLIENT_REQUEST_TIMEOUT", &timeoutSec, timeoutSec); err != nil {
			c.LogAlarm(err, "initialize env DOCKER_CLIENT_REQUEST_TIMEOUT error")
		}
		if timeoutSec > 0 {
			ContainerCenterTimeout = time.Duration(timeoutSec) * time.Second
		}
	}
	logger.Info(context.Background(), "init docker center, client request timeout", ContainerCenterTimeout.String())
	{
		count := int(MaxFetchOneTriggerPerSecond)
		if err := util.InitFromEnvInt("CONTAINER_FETCH_ONE_MAX_COUNT_PER_SECOND", &count, count); err != nil {
			c.LogAlarm(err, "initialize env CONTAINER_FETCH_ONE_MAX_COUNT_PER_SECOND error")
		}
		if count > 0 {
			MaxFetchOneTriggerPerSecond = int32(count)
		}
	}
	logger.Info(context.Background(), "init docker center, max fetchOne count per second", MaxFetchOneTriggerPerSecond)

	var err error
	if c.enableDockerDiscover {
		if err = c.fetchDocker(); err != nil {
			c.enableDockerDiscover = false
			logger.Warningf(context.Background(), "DOCKER_CENTER_ALARM", "fetch docker containers error, close docker discover, will retry")
		}
	}
	if c.enableCRIDiscover {
		if err = c.fetchCRI(); err != nil {
			c.enableCRIDiscover = false
			logger.Warningf(context.Background(), "DOCKER_CENTER_ALARM", "fetch cri containers error, close cri discover, will retry")
		}
	}
	if c.enableStaticDiscover {
		c.fetchStatic()
	}
	logger.Info(context.Background(), "final", "param", "docker discover", c.enableDockerDiscover, "cri discover", c.enableCRIDiscover, "static discover", c.enableStaticDiscover)
	return c.enableCRIDiscover || c.enableDockerDiscover || c.enableStaticDiscover
}

func (c *ContainerDiscoverManager) TimerFetch() {
	timerFetch := func() {
		defer containerCenterRecover()
		lastFetchAllTime := time.Now()
		for {
			time.Sleep(time.Duration(10) * time.Second)
			logger.Debug(context.Background(), "container clean timeout container info", "start")
			containerCenterInstance.cleanTimeoutContainer()
			logger.Debug(context.Background(), "container clean timeout container info", "done")
			if time.Since(lastFetchAllTime) >= FetchAllInterval {
				logger.Info(context.Background(), "container fetch all", "start")
				c.FetchAll()
				lastFetchAllTime = time.Now()
				c.Clean()
				logger.Info(context.Background(), "container fetch all", "end")
			}

		}
	}
	go timerFetch()
}
