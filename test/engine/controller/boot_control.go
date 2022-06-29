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
	"context"
	"os"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/boot"
)

type BootController struct {
	chain *CancelChain
}

func (c *BootController) Init(parent *CancelChain, cfg *config.Case) error {
	logger.Info(context.Background(), "boot controller is initializing....")
	c.chain = WithCancelChain(parent)
	return boot.Load(cfg)
}

func (c *BootController) Start() error {
	logger.Info(context.Background(), "boot controller is starting....")
	go func() {
		<-c.chain.Done()
		logger.Info(context.Background(), "boot controller is stoping....")
		c.Clean()
		c.chain.CancelChild()
	}()
	return boot.Start()
}

func (c *BootController) Clean() {
	logger.Info(context.Background(), "boot controller is cleaning....")
	if err := boot.ShutDown(); err != nil {
		logger.Error(context.Background(), "BOOT_STOP_ALARM", "err", err)
	}
	_ = os.Remove(config.ConfigFile)
	_ = os.Remove(config.FlusherFile)
}

func (c *BootController) CancelChain() *CancelChain {
	return c.chain
}
