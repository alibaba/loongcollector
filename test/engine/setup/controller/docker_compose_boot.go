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
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/setup/dockercompose"
)

type BootController struct {
}

func (c *BootController) Init(bootType dockercompose.BootType) error {
	logger.Info(context.Background(), "boot controller is initializing....")
	if err := dockercompose.Load(bootType); err != nil {
		logger.Error(context.Background(), selfmonitor.BootLoadAlarm, "err", err)
		return err
	}
	return nil
}

func (c *BootController) Start(ctx context.Context) error {
	logger.Info(context.Background(), "boot controller is starting....")
	if _, err := os.Stat(config.ConfigDir); os.IsNotExist(err) {
		if err = os.Mkdir(config.ConfigDir, 0750); err != nil {
			logger.Error(context.Background(), selfmonitor.BootStartAlarm, "err", err)
			return err
		}
	} else if err != nil {
		logger.Error(context.Background(), selfmonitor.BootStartAlarm, "err", err)
		return err
	}
	if _, err := os.Stat(config.OnetimeConfigDir); os.IsNotExist(err) {
		if err = os.Mkdir(config.OnetimeConfigDir, 0750); err != nil {
			logger.Error(context.Background(), selfmonitor.BootStartAlarm, "err", err)
			return err
		}
	} else if err != nil {
		logger.Error(context.Background(), selfmonitor.BootStartAlarm, "err", err)
		return err
	}
	if err := dockercompose.Start(ctx); err != nil {
		logger.Error(context.Background(), selfmonitor.BootStartAlarm, "err", err)
		return err
	}
	return nil
}

func (c *BootController) Clean() {
	logger.Info(context.Background(), "boot controller is cleaning....")
	if err := dockercompose.ShutDown(); err != nil {
		logger.Error(context.Background(), selfmonitor.BootStopAlarm, "err", err)
	}
	_ = os.RemoveAll(config.FlusherFile)
	_ = os.RemoveAll(config.ConfigDir)
	_ = os.RemoveAll(config.OnetimeConfigDir)
	time.Sleep(time.Second * 3)
}
