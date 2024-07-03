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
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/setup/dockercompose"
)

type BootController struct {
}

func (c *BootController) Init() error {
	logger.Info(context.Background(), "boot controller is initializing....")
	return dockercompose.Load()
}

func (c *BootController) Start(ctx context.Context) error {
	logger.Info(context.Background(), "boot controller is starting....")
	if _, err := os.Stat(config.ConfigDir); os.IsNotExist(err) {
		if err = os.Mkdir(config.ConfigDir, 0750); err != nil {
			return err
		}
	} else if err != nil {
		return err
	}
	return dockercompose.Start(ctx)
}

func (c *BootController) Clean() {
	logger.Info(context.Background(), "boot controller is cleaning....")
	if err := dockercompose.ShutDown(); err != nil {
		logger.Error(context.Background(), "BOOT_STOP_ALARM", "err", err)
	}
	_ = os.RemoveAll(config.FlusherFile)
	_ = os.RemoveAll(config.ConfigDir)
	time.Sleep(time.Second * 3)
}
