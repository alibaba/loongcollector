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
	"errors"
	"fmt"
	"net/http"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/boot"
)

type HTTPCaseController struct {
	chain       *CancelChain
	cfg         *config.Trigger
	duration    time.Duration
	virtualAddr string
	path        string
}

func (c *HTTPCaseController) Init(parent *CancelChain, cfg *config.Case) error {
	logger.Info(context.Background(), "httpcase controller is initializing....")
	c.chain = WithCancelChain(parent)
	c.cfg = &cfg.Trigger
	if c.cfg.Times == 0 {
		return nil
	}
	if !strings.HasPrefix(cfg.Trigger.URL, "http://") {
		return errors.New("the trigger URL must start with `http://`")
	}
	index := strings.Index(cfg.Trigger.URL[7:], "/")
	if index == -1 {
		c.virtualAddr = cfg.Trigger.URL[7:]
	} else {
		c.virtualAddr = cfg.Trigger.URL[7:][:index]
		c.path = cfg.Trigger.URL[7+index:]
	}
	if !strings.Contains(c.virtualAddr, ":") {
		c.virtualAddr += ":80"
	}

	duration, err := time.ParseDuration(c.cfg.Interval)
	if err != nil {
		return fmt.Errorf("error in parsing trigger interval: %v", err)
	}
	c.duration = duration

	return nil
}

func (c *HTTPCaseController) Start() error {
	logger.Info(context.Background(), "httpcase controller is starting....")
	if c.cfg.Times > 0 {
		physicalAddress := boot.GetPhysicalAddress(c.virtualAddr)
		if physicalAddress == "" {
			return fmt.Errorf("cannot find the physical address of the %s virtual address in the exported ports", c.virtualAddr)
		}
		realAddress := "http://" + physicalAddress + c.path

		request, err := http.NewRequest(c.cfg.Method, realAddress, strings.NewReader(c.cfg.Body))
		if err != nil {
			return fmt.Errorf("error in creating http trigger request: %v", err)
		}
		go func() {
			ticker := time.NewTicker(c.duration)
			defer ticker.Stop()
			client := &http.Client{}
			count := 1
			for range ticker.C {
				logger.Infof(context.Background(), "request URL %s the %d time", request.URL, count)
				resp, err := client.Do(request)
				if err != nil {
					logger.Debugf(context.Background(), "error in triggering the http request: %s, err: %s", request.URL, err)
					continue
				}
				_ = resp.Body.Close()
				logger.Debugf(context.Background(), "request URL %s got response code: %d", request.URL, resp.StatusCode)
				if resp.StatusCode == http.StatusOK {
					logger.Debugf(context.Background(), "request URL %s success", request.URL)
				} else {
					logger.Debugf(context.Background(), "request URL %s failed", request.URL)
				}
				count++
				if count > c.cfg.Times {
					return
				}
			}
		}()
	}

	go func() {
		<-c.chain.Done()
		logger.Info(context.Background(), "httpcase controller is closing....")
		c.Clean()
		c.chain.CancelChild()
	}()
	return nil
}

func (c *HTTPCaseController) CancelChain() *CancelChain {
	return c.chain
}

func (c *HTTPCaseController) Clean() {
	logger.Info(context.Background(), "httpcase controller is cleaning....")
}
