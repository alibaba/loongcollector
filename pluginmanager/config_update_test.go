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

//go:build linux || windows
// +build linux windows

package pluginmanager

import (
	"context"
	"strings"
	"testing"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/plugins/flusher/checker"

	"github.com/stretchr/testify/suite"
)

var updateConfigName = "update_mock_block"
var noblockUpdateConfigName = "update_mock_noblock"

func TestConfigUpdate(t *testing.T) {
	suite.Run(t, new(configUpdateTestSuite))
}

type configUpdateTestSuite struct {
	suite.Suite
}

func (s *configUpdateTestSuite) BeforeTest(suiteName, testName string) {
	logger.Infof(context.Background(), "========== %s %s test start ========================", suiteName, testName)
	logger.Info(context.Background(), "load logstore config", updateConfigName)
	s.NoError(LoadMockConfig(updateConfigName, updateConfigName, updateConfigName, GetTestConfig(updateConfigName)))
	s.NoError(Resume())
	time.Sleep(time.Second * time.Duration(1))
}

func (s *configUpdateTestSuite) AfterTest(suiteName, testName string) {
	logger.Infof(context.Background(), "========== %s %s test end ========================", suiteName, testName)
	s.NoError(HoldOn(false))
	LogtailConfig = make(map[string]*LogstoreConfig)
	DisabledLogtailConfig = make(map[string]*LogstoreConfig)
}

func (s *configUpdateTestSuite) TestConfigUpdate() {
	// block config
	config := LogtailConfig[updateConfigName]
	s.NotNil(config, "%s logstrore config should exist", updateConfigName)
	checkFlusher, ok := GetConfigFlushers(config.PluginRunner)[0].(*checker.FlusherChecker)
	s.True(ok)
	s.Equal(0, checkFlusher.GetLogCount(), "the block flusher checker doesn't have any logs")

	// update same hang config
	s.NoError(HoldOn(false))
	s.Equal(0, checkFlusher.GetLogCount(), "the hold on block flusher checker doesn't have any logs")
	err := LoadMockConfig(updateConfigName, updateConfigName, updateConfigName, GetTestConfig(updateConfigName))
	s.True(strings.Contains(err.Error(), "failed to create config because timeout stop has happened on it"))
	s.NoError(LoadMockConfig(noblockUpdateConfigName, noblockUpdateConfigName, noblockUpdateConfigName, GetTestConfig(noblockUpdateConfigName)))
	s.NoError(Resume())
	s.Nil(LogtailConfig[updateConfigName], "the stopping config only allow to load same config when stopped")
	s.NotNil(LogtailConfig[noblockUpdateConfigName])

	// unblock old config
	checkFlusher.Block = false
	time.Sleep(time.Second * time.Duration(5))
	s.Equal(10000, checkFlusher.GetLogCount())
	// this magic number(10000) must exceed number of logs can be hold in processor channel(LogsChan) + aggregator buffer(defaultLogGroup) + flusher channel(LogGroupsChan)
	s.Equal(20000, GetConfigFlushers(LogtailConfig[noblockUpdateConfigName].PluginRunner)[0].(*checker.FlusherChecker).GetLogCount())
}

func (s *configUpdateTestSuite) TestConfigUpdateMany() {
	config := LogtailConfig[updateConfigName]
	s.NotNil(config, "%s logstrore config should exist", updateConfigName)
	checkFlusher, ok := GetConfigFlushers(config.PluginRunner)[0].(*checker.FlusherChecker)
	s.True(ok)

	s.Equal(0, checkFlusher.GetLogCount(), "the hold on block flusher checker doesn't have any logs")
	// load block config
	for i := 0; i < 5; i++ {
		s.NoError(HoldOn(false))
		err := LoadMockConfig(updateConfigName, updateConfigName, updateConfigName, GetTestConfig(updateConfigName))
		s.True(strings.Contains(err.Error(), "failed to create config because timeout stop has happened on it"))
		s.NoError(Resume())
		s.Nil(LogtailConfig[updateConfigName], "the stopping config only allow to load same config when stopped")
	}
	s.Equal(0, checkFlusher.GetLogCount(), "the hold on block flusher checker doesn't have any logs")
	checkFlusher.Block = false
	time.Sleep(time.Second * time.Duration(5))
	s.Equal(checkFlusher.GetLogCount(), 10000)

	// load normal config
	for i := 0; i < 5; i++ {
		s.NoError(HoldOn(false))
		s.NoError(LoadMockConfig(noblockUpdateConfigName, noblockUpdateConfigName, noblockUpdateConfigName, GetTestConfig(noblockUpdateConfigName)))
		s.NoError(Resume())
		s.NotNil(LogtailConfig[noblockUpdateConfigName])
		time.Sleep(time.Millisecond)
	}
	checkFlusher, ok = GetConfigFlushers(LogtailConfig[noblockUpdateConfigName].PluginRunner)[0].(*checker.FlusherChecker)
	s.True(ok)
	time.Sleep(time.Second * time.Duration(5))
	s.Equal(checkFlusher.GetLogCount(), 20000)
}

func (s *configUpdateTestSuite) TestConfigUpdateName() {
	time.Sleep(time.Second * time.Duration(1))
	config := LogtailConfig[updateConfigName]
	s.NotNil(config)
	checkFlusher, ok := GetConfigFlushers(config.PluginRunner)[0].(*checker.FlusherChecker)
	defer func() {
		checkFlusher.Block = false
		time.Sleep(time.Second * 5)
		s.Equal(checkFlusher.GetLogCount(), 10000)
	}()
	s.True(ok)

	s.NoError(HoldOn(false))
	s.Equal(0, checkFlusher.GetLogCount(), "the hold on blocking flusher checker doesn't have any logs")
	s.NoError(LoadMockConfig(updateConfigName+"_", updateConfigName+"_", updateConfigName+"_", GetTestConfig(updateConfigName)))
	s.NoError(Resume())

	{
		s.Nil(LogtailConfig[updateConfigName])
		s.NotNil(LogtailConfig[updateConfigName+"_"])
		checkFlusher, ok := GetConfigFlushers(LogtailConfig[updateConfigName+"_"].PluginRunner)[0].(*checker.FlusherChecker)
		s.True(ok)
		s.Equal(checkFlusher.GetLogCount(), 0)
		checkFlusher.Block = false
		time.Sleep(time.Second * 5)
		s.Equal(checkFlusher.GetLogCount(), 20000)
	}
}

func (s *configUpdateTestSuite) TestHoldOnExit() {
	config := LogtailConfig[updateConfigName]
	s.NotNil(config)
	checkFlusher, ok := GetConfigFlushers(config.PluginRunner)[0].(*checker.FlusherChecker)
	s.True(ok)
	checkFlusher.Block = false
	time.Sleep(time.Second * time.Duration(5))
	s.NoError(HoldOn(true))
	s.Equal(20000, checkFlusher.GetLogCount())
	s.NoError(Resume())
}

func (s *configUpdateTestSuite) TestHoldOnExitTimeout() {
	time.Sleep(time.Second * time.Duration(1))
	config := LogtailConfig[updateConfigName]
	s.NotNil(config)
	checkFlusher, ok := GetConfigFlushers(config.PluginRunner)[0].(*checker.FlusherChecker)
	s.True(ok)
	s.Equal(0, checkFlusher.GetLogCount())
	s.NoError(HoldOn(true))
	time.Sleep(time.Second)
	s.Equal(0, checkFlusher.GetLogCount())
	checkFlusher.Block = false
	time.Sleep(time.Second * time.Duration(5))
	s.Equal(10000, checkFlusher.GetLogCount())
	time.Sleep(time.Second * 10)
	s.NoError(Resume())
}
