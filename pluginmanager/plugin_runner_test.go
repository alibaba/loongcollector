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

package pluginmanager

import (
	"context"
	"testing"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"github.com/stretchr/testify/suite"
)

func TestPluginRunner(t *testing.T) {
	suite.Run(t, new(pluginRunnerTestSuite))
}

type pluginRunnerTestSuite struct {
	suite.Suite
	Context pipeline.Context
}

func (s *pluginRunnerTestSuite) BeforeTest(suiteName, testName string) {
	logger.Infof(context.Background(), "========== %s %s test start ========================", suiteName, testName)
	s.Context = &ContextImp{}
	s.Context.InitContext("", "", "")
}

func (s *pluginRunnerTestSuite) AfterTest(suiteName, testName string) {
	logger.Infof(context.Background(), "========== %s %s test end =======================", suiteName, testName)
}

func (s *pluginRunnerTestSuite) TestTimerRunner_WithoutInitialDelay() {
	runner := &timerRunner{state: s, interval: time.Millisecond * 600, context: s.Context}
	cc := pipeline.NewAsyncControl()
	ch := make(chan struct{}, 10)
	cc.Run(func(cc *pipeline.AsyncControl) {
		runner.Run(func(state interface{}) error {
			ch <- struct{}{}
			s.Equal(state, s)
			return nil
		}, cc)
	})
	cc.WaitCancel()
	s.Equal(2, len(ch))

	ch = make(chan struct{}, 10)
	cc.Reset()
	cc.Run(func(cc *pipeline.AsyncControl) {
		runner.Run(func(state interface{}) error {
			ch <- struct{}{}
			s.Equal(state, s)
			return nil
		}, cc)
	})
	<-time.After(time.Millisecond * 1000)
	cc.WaitCancel()
	s.Equal(3, len(ch))
}

func (s *pluginRunnerTestSuite) TestTimerRunner_WithInitialDelay() {
	runner := &timerRunner{state: s, initialMaxDelay: time.Second, interval: time.Millisecond * 600, context: s.Context}
	cc := pipeline.NewAsyncControl()
	ch := make(chan struct{}, 10)
	cc.Run(func(cc *pipeline.AsyncControl) {
		runner.Run(func(state interface{}) error {
			ch <- struct{}{}
			s.Equal(state, s)
			return nil
		}, cc)
	})
	cc.WaitCancel()
	s.Equal(1, len(ch))

	ch = make(chan struct{}, 10)
	cc.Reset()
	cc.Run(func(cc *pipeline.AsyncControl) {
		runner.Run(func(state interface{}) error {
			ch <- struct{}{}
			s.Equal(state, s)
			return nil
		}, cc)
	})
	<-time.After(time.Millisecond * 600)
	cc.WaitCancel()
	s.Equal(2, len(ch))
}
