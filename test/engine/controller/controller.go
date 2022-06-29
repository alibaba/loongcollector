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
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/signals"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/validator"
)

type CancelChain struct {
	child *CancelChain
	ch    chan struct{}
}

type Controller interface {
	Init(parent *CancelChain, cfg *config.Case) error
	Start() error
	Clean()
	CancelChain() *CancelChain
}

func (c *CancelChain) CancelChild() {
	close(c.child.ch)
}

func (c *CancelChain) Done() <-chan struct{} {
	return c.ch
}

// Start the E2E test case.
func Start(cfg *config.Case) error {
	duration, err := time.ParseDuration(cfg.TestingInterval)
	if err != nil {
		return err
	}
	retryDuration, err := time.ParseDuration(cfg.Retry.Interval)
	if err != nil {
		return err
	}
	signal := signals.SetupSignalHandler()
	count := 0
	// try config.Retry.Times + 1 times to pass the testing.
	for {
		validator.ClearCounter()
		validatorController := new(ValidatorController)
		var sequence []Controller
		if config.ProfileFlag {
			sequence = append(sequence, new(ProfileController))
		}
		sequence = append(sequence,
			new(HTTPCaseController),
			new(LogtailPluginController),
			new(LogtailController),
			validatorController,
			new(SubscriberController),
			new(BootController),
		)
		root := WithCancelChain(nil)
		parent := root
		// make context cancel chain.
		for i, c := range sequence {
			if err := c.Init(parent, cfg); err != nil {
				logger.Error(context.Background(), "E2E_INIT_ALARM", "err", err)
				for j := 0; j <= i; j++ {
					sequence[j].Clean()
				}
				return err
			}
			parent = c.CancelChain()
		}
		// start with the reverse order.
		for i := len(sequence) - 1; i >= 0; i-- {
			if err := sequence[i].Start(); err != nil {
				logger.Error(context.Background(), "E2E_START_ALARM", "err", err)
				for _, c := range sequence {
					c.Clean()
				}
				return err
			}
		}
		logger.Infof(context.Background(), "testing has started and will last %s", cfg.TestingInterval)
		listener := WithCancelChain(parent)
		select {
		case <-signal:
			root.CancelChild()
			<-listener.Done()
			logger.Info(context.Background(), "Testing is killed")
			return errors.New("e2e test is killed")
		case <-time.After(duration):
			root.CancelChild()
			<-listener.Done()
			logger.Info(context.Background(), "Testing is completed")
		}
		count++
		if !validatorController.report.Pass {
			if count <= cfg.Retry.Times {
				logger.Errorf(context.Background(), "E2E_RESULT_ALARM", "the %d times testing is failed", count)
				time.Sleep(retryDuration)
			} else {
				logger.Error(context.Background(), "E2E_RESULT_ALARM", "the E2E testing is failed, more details please see report directory")
				return errors.New("e2e test failed")
			}
		} else {
			logger.Info(context.Background(), "the E2E testing is passed")
			return nil
		}
	}
}

func WithCancelChain(parent *CancelChain) *CancelChain {
	c := &CancelChain{
		ch: make(chan struct{}),
	}
	if parent != nil {
		parent.child = c
	}
	return c
}
