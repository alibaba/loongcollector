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
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"time"

	"github.com/alibaba/ilogtail/pkg/flags"
	"github.com/alibaba/ilogtail/pkg/logger"
	pluginmanager "github.com/alibaba/ilogtail/pluginmanager"
)

var envConfigCacheCleanInterval = time.Hour
var envConfigCacheCleanThresholdSec = 3600 * 6

type Manager struct {
	AllConfigMap     map[string]*AliyunLogConfigSpec
	shutdown         chan struct{}
	operationWrapper *operationWrapper
}

func (decm *Manager) update(config *AliyunLogConfigSpec) *AliyunLogConfigSpec {
	key := config.Key()
	if lastConfig, ok := decm.AllConfigMap[key]; ok {
		// if hash code is same, skip
		if bytes.Equal(lastConfig.HashCode, config.HashCode) {
			logger.Debug(context.Background(), "old env config, key", config.Key(), "detail", *config)
			lastConfig.LastFetchTime = config.LastFetchTime
			if lastConfig.ErrorCount > 0 {
				logger.Info(context.Background(), "old env config but last sync error, key", config.Key(), "error count", lastConfig.ErrorCount, "last fetch time", lastConfig.LastFetchTime)
				return lastConfig
			}
			return nil
		}
	}
	logger.Info(context.Background(), "update new env config, key", config.Key(), "detail", *config)
	decm.AllConfigMap[key] = config
	return config
}

func (decm *Manager) removeUselessCache() {
	nowTime := time.Now().Unix()
	logger.Debug(context.Background(), "start check useless cache", len(decm.AllConfigMap))
	for key, config := range decm.AllConfigMap {
		logger.Debug(context.Background(), "check useless cache, key", key, "last fetch time", config.LastFetchTime)
		if nowTime-config.LastFetchTime > int64(envConfigCacheCleanThresholdSec) {
			logger.Info(context.Background(), "delete useless config cache, key", key, "detail", *config)
			delete(decm.AllConfigMap, key)
		}
	}
	logger.Debug(context.Background(), "end check useless cache", len(decm.AllConfigMap))
}

func (decm *Manager) loadCheckpoint() {
	decm.AllConfigMap = make(map[string]*AliyunLogConfigSpec)
	val, err := pluginmanager.CheckPointManager.GetCheckpoint("docker_env_config", "v1")
	if err != nil {
		return
	}
	if err = json.Unmarshal(val, &decm.AllConfigMap); err != nil {
		logger.Error(context.Background(), "DOCKER_ENV_CONFIG_CHECKPOINT_ALARM", "load checkpoint error, err", err, "content", string(val))
	}
}

func (decm *Manager) saveCheckpoint() {
	checkpoint, err := json.Marshal(decm.AllConfigMap)
	if err != nil {
		logger.Error(context.Background(), "DOCKER_ENV_CONFIG_CHECKPOINT_ALARM", "save checkpoint error, err", err)
	}
	_ = pluginmanager.CheckPointManager.SaveCheckpoint("docker_env_config", "v1", checkpoint)
}

func (decm *Manager) run() {
	decm.shutdown = make(chan struct{})
	var err error
	// always retry when create operator wrapper fail
	for i := 0; i < 100000000; i++ {
		decm.operationWrapper, err = createAliyunLogOperationWrapper(*flags.LogServiceEndpoint, *flags.DefaultLogProject, *flags.DefaultAccessKeyID, *flags.DefaultAccessKeySecret, *flags.DefaultSTSToken, decm.shutdown)
		if err != nil {
			logger.Error(context.Background(), "DOCKER_ENV_CONFIG_INIT_ALARM", "create log operation wrapper, err", err)
			sleepInterval := i * 5
			if sleepInterval > 3600 {
				sleepInterval = 3600
			}
			time.Sleep(time.Second * time.Duration(sleepInterval))
		} else {
			break
		}
	}
	if decm.operationWrapper == nil {
		return
	}
	lastCleanTime := time.Now()
	decm.loadCheckpoint()
	for {
		errorFlag := false
		fetchAllEnvConfig()
		var updateConfigList []*AliyunLogConfigSpec
		for _, config := range logConfigSpecList {
			if config = decm.update(config); config != nil {
				updateConfigList = append(updateConfigList, config)
			}
		}
		for _, config := range updateConfigList {
			nowTime := time.Now().Unix()
			if config.ErrorCount > 0 && config.NextTryTime > nowTime {
				continue
			}
			if err = decm.operationWrapper.updateConfig(config); err != nil {
				errorFlag = true
				config.ErrorCount++
				tryInterval := int64(10 * config.ErrorCount)
				if tryInterval < 10 {
					tryInterval = 10
				}
				if tryInterval > 900 {
					tryInterval = 900
				}
				config.NextTryTime = nowTime + tryInterval
				logger.Error(context.Background(), "DOCKER_ENV_CONFIG_OPERATION_ALARM", "update config failed, config key", config.Key(), "error", err.Error(), "error count", config.ErrorCount, "next try time", config.NextTryTime)
			} else {
				// when update success, set config.ErrorCount = 0
				config.ErrorCount = 0
				logger.Info(context.Background(), "update config success, config key", config.Key(), "detail", fmt.Sprintf("%+v", *config))
			}
		}
		if len(updateConfigList) > 0 {
			decm.saveCheckpoint()
		}
		if !errorFlag && flags.SelfEnvConfigFlag {
			logger.Info(context.Background(), "create config success when self env flag", "prepare exit")
			return
		}
		time.Sleep(time.Second * time.Duration(*flags.DockerEnvUpdateInterval))
		if time.Since(lastCleanTime) > envConfigCacheCleanInterval {
			decm.removeUselessCache()
			lastCleanTime = time.Now()
		}
	}
}
