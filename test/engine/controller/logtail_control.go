// Copyright 2022 iLogtail Authors
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
	"context"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"strconv"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/config"
)

type LogtailController struct {
	chain *CancelChain
}

func (l *LogtailController) Init(parent *CancelChain, fullCfg *config.Case) error {
	logger.Info(context.Background(), "ilogtail controller is initializing....")
	l.chain = WithCancelChain(parent)
	logstoreCfg := make(map[string]interface{})
	// todo currently only support one fixed config with cgo
	if len(fullCfg.Ilogtail.Config) == 1 {
		cfg := fullCfg.Ilogtail.Config[0]
		for idx, con := range cfg.Content {
			name := cfg.Name + "_" + strconv.Itoa(idx)
			content := make(map[string]interface{})
			if err := json.Unmarshal([]byte(con), &content); err != nil {
				return fmt.Errorf("cannot deserizlize %s config by json, content:%s", name, con)
			}
			if cfg.MixedMode {
				content["category"] = E2ELogstoreName
				content["project_name"] = E2EProjectName
				logstoreCfg[name] = content
			} else {
				pluginCfg := make(map[string]interface{})
				pluginCfg["enable"] = true
				pluginCfg["log_type"] = "plugin"
				pluginCfg["category"] = E2ELogstoreName
				pluginCfg["project_name"] = E2EProjectName
				pluginCfg["plugin"] = content
				logstoreCfg[name] = pluginCfg
			}
		}
		fullCfg.Ilogtail.Config = fullCfg.Ilogtail.Config[:0]
	}

	bytes, _ := json.Marshal(map[string]interface{}{
		"metrics": logstoreCfg,
	})
	return ioutil.WriteFile(config.ConfigFile, bytes, 0600)
}

func (l *LogtailController) Start() error {
	go func() {
		<-l.chain.Done()
		logger.Info(context.Background(), "httpcase controller is closing....")
		l.Clean()
		l.chain.CancelChild()
	}()
	return nil
}

func (l *LogtailController) Clean() {
	logger.Info(context.Background(), "ilogtail controller is cleaning....")
}

func (l *LogtailController) CancelChain() *CancelChain {
	return l.chain
}
