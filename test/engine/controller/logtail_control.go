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
	"fmt"
	"os"
	"path/filepath"
	"strconv"

	"gopkg.in/yaml.v3"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/config"
)

type LogtailController struct {
	chain *CancelChain
}

func (l *LogtailController) Init(parent *CancelChain, fullCfg *config.Case) error {
	logger.Info(context.Background(), "ilogtail controller is initializing....")
	l.chain = WithCancelChain(parent)
	// one config set means the set should be loaded on start
	if len(fullCfg.Ilogtail.Config) == 1 {
		cfg := fullCfg.Ilogtail.Config[0]
		if _, err := os.Stat(config.ConfigDir); err == nil {
			_ = os.Remove(config.ConfigDir)
		} else if !os.IsNotExist(err) {
			return err
		}
		if err := os.Mkdir(config.ConfigDir, 0750); err != nil {
			return err
		}
		for idx, detail := range cfg.Detail {
			name := cfg.Name + "_" + strconv.Itoa(idx) + ".yaml"
			if _, ok := detail["inputs"]; !ok {
				return fmt.Errorf("lack of input plugin in the %d config detail under name %s", idx, cfg.Name)
			}
			if _, ok := detail["flushers"]; !ok {
				detail["flushers"] = []map[string]interface{}{
					{
						"Type":    "flusher_grpc",
						"Address": "host.docker.internal:8000",
					},
				}
			}
			bytes, _ := yaml.Marshal(detail)
			if err := os.WriteFile(filepath.Join(config.ConfigDir, name), bytes, 0600); err != nil {
				return err
			}
		}
		fullCfg.Ilogtail.Config = fullCfg.Ilogtail.Config[:0]
	}
	return nil
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
