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
	"fmt"
	"time"

	aliyunlog "github.com/aliyun/aliyun-log-go-sdk"

	"github.com/alibaba/ilogtail/pkg/flags"
	"github.com/alibaba/ilogtail/pkg/logger"
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

func (decm *Manager) run() {
	decm.shutdown = make(chan struct{})
	var err error
	var clientInterface aliyunlog.ClientInterface
	// always retry when create client interface fail
	sleepInterval := 0
	for {
		clientInterface, err = createClientInterface(*flags.LogServiceEndpoint, *flags.DefaultAccessKeyID, *flags.DefaultAccessKeySecret, *flags.DefaultSTSToken, decm.shutdown)
		if err != nil {
			logger.Error(context.Background(), "DOCKER_ENV_CONFIG_INIT_ALARM", "create log clien interface, err", err)
			sleepInterval += 5
			if sleepInterval > 3600 {
				sleepInterval = 3600
			}
			time.Sleep(time.Second * time.Duration(sleepInterval))
		} else {
			break
		}
	}
	if clientInterface == nil {
		return
	}
	logger.Info(context.Background(), "create client interface success", "")
	// always retry when create operator wrapper fail
	sleepInterval = 0
	for {
		decm.operationWrapper, err = createAliyunLogOperationWrapper(*flags.DefaultLogProject, clientInterface)
		if err != nil {
			logger.Error(context.Background(), "DOCKER_ENV_CONFIG_INIT_ALARM", "create log operation wrapper, err", err)
			sleepInterval += 5
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
	decm.AllConfigMap = make(map[string]*AliyunLogConfigSpec)
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
