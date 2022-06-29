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

package controller

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"strconv"
	"time"

	"github.com/alibaba/ilogtail/pkg"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/boot"
)

const lotailpluginHTTPAddress = "ilogtail:18689"
const E2EProjectName = "e2e-test-project"
const E2ELogstoreName = "e2e-test-logstore"

type LogtailPluginController struct {
	chain        *CancelChain
	cfg          *config.Ilogtail
	waitDuration time.Duration
	loadDuration time.Duration
}

func (c *LogtailPluginController) Init(parent *CancelChain, cfg *config.Case) error {
	logger.Info(context.Background(), "ilogtail plugin controller is initializing....")
	c.chain = WithCancelChain(parent)
	c.cfg = &cfg.Ilogtail
	if len(c.cfg.Config) != 0 {
		duration, err := time.ParseDuration(c.cfg.CloseWait)
		if err != nil {
			return err
		}
		c.waitDuration = duration
		loadDuration, err := time.ParseDuration(c.cfg.LoadConfigWait)
		if err != nil {
			return err
		}
		c.loadDuration = loadDuration
	}
	return nil
}

func (c *LogtailPluginController) Start() error {
	logger.Info(context.Background(), "ilogtail plugin controller is starting....")
	address := boot.GetPhysicalAddress(lotailpluginHTTPAddress)
	if address == "" {
		return errors.New("ilogtail export virtual address should be " + lotailpluginHTTPAddress)
	}
	endpointPrefix := "http://" + address

	for i, cfgs := range c.cfg.Config {
		logger.Infof(context.Background(), "the %d times load config operation is starting ...", i+1)
		var loadConfigs []*pkg.LoadedConfig
		for j := 0; j < len(cfgs.Content); j++ {
			loadConfigs = append(loadConfigs, &pkg.LoadedConfig{
				Project:     E2EProjectName,
				Logstore:    E2ELogstoreName,
				ConfigName:  cfgs.Name + "_" + strconv.Itoa(j),
				LogstoreKey: 1,
				JSONStr:     cfgs.Content[j],
			})
		}
		cfg, _ := json.Marshal(loadConfigs)
		// load test case configuration
		resp, err := http.Post(endpointPrefix+pkg.EnpointLoadconfig, "application/json", bytes.NewReader(cfg))
		if err != nil {
			return fmt.Errorf("error when posting the new logtail plugin configuration: %v", err)
		}
		_ = resp.Body.Close()
		if resp.StatusCode != http.StatusOK {
			return fmt.Errorf("failed load ilogtail configuration, the response code is %d", resp.StatusCode)
		}
		if i != len(c.cfg.Config)-1 {
			time.Sleep(c.loadDuration)
			logger.Infof(context.Background(), "the next load config operation would be started in %s", c.cfg.LoadConfigWait)
		}
	}
	// unload test case configuration
	go func() {
		<-c.chain.Done()
		logger.Info(context.Background(), "ilogtail plugin controller is closing....")
		c.Clean()
		resp, err := http.Get("http://" + boot.GetPhysicalAddress(lotailpluginHTTPAddress) + "/holdon")
		if err != nil {
			logger.Error(context.Background(), "HOLDON_LOGTAILPLUGIN_ALARM", "err", err)
			return
		}
		_ = resp.Body.Close()
		if resp.StatusCode != http.StatusOK {
			logger.Error(context.Background(), "HOLDON_LOGTAILPLUGIN_ALARM", "statusCode", resp.StatusCode)
		}
		logger.Infof(context.Background(), "ilogtail controller would wait %s to deal with the logs on the way", c.cfg.CloseWait)
		time.Sleep(c.waitDuration)
		c.chain.CancelChild()
	}()
	return nil
}

func (c *LogtailPluginController) CancelChain() *CancelChain {
	return c.chain
}

func (c *LogtailPluginController) Clean() {
	logger.Info(context.Background(), "ilogtail plugin controller is cleaning....")
}
