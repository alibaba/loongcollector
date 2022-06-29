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
	"io/ioutil"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/subscriber"
)

var globalSubscriberChan <-chan *protocol.LogGroup

type SubscriberController struct {
	chain *CancelChain
	sub   subscriber.Subscriber
}

func (c *SubscriberController) Init(parent *CancelChain, cfg *config.Case) error {
	logger.Info(context.Background(), "subscriber controller is initializing....")
	c.chain = WithCancelChain(parent)
	s, err := subscriber.New(cfg.Subscriber.Name, cfg.Subscriber.Config)
	if err != nil {
		return err
	}
	c.sub = s
	globalSubscriberChan = c.sub.SubscribeChan()
	return WriteDefaultFlusherConfig(c.sub.FlusherConfig())
}

func (c *SubscriberController) Start() error {
	logger.Info(context.Background(), "subscriber controller is starting....")
	go func() {
		<-c.chain.Done()
		logger.Info(context.Background(), "subscriber controller is closing....")
		c.Clean()
		c.chain.CancelChild()
	}()
	return c.sub.Start()
}

func (c *SubscriberController) CancelChain() *CancelChain {
	return c.chain
}

func WriteDefaultFlusherConfig(cfg string) error {
	return ioutil.WriteFile(config.FlusherFile, []byte(cfg), 0600)
}

func (c *SubscriberController) Clean() {
	logger.Info(context.Background(), "subscriber controller is cleaning....")
	c.sub.Stop()
}
