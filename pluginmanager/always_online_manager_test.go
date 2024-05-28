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

package pluginmanager

import (
	"strconv"
	"testing"
	"time"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/config"
	_ "github.com/alibaba/ilogtail/pkg/logger/test"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

// init change the logtail config dir to avoid change the config on the production when testing.
func init() {
	config.LogtailGlobalConfig.LogtailSysConfDir = "."
}

func TestAlwaysOnlineManager(t *testing.T) {
	aom := GetAlwaysOnlineManager()
	newLogstoreConfig := func(name string, hash string) *LogstoreConfig {
		conf := &LogstoreConfig{}
		conf.ConfigName = config.GetRealConfigName(name)
		conf.ConfigNameWithSuffix = name
		conf.configDetailHash = hash
		conf.Context = mock.NewEmptyContext("p", "l", "c")
		conf.PluginRunner = &pluginv1Runner{LogstoreConfig: conf, FlushOutStore: NewFlushOutStore[protocol.LogGroup]()}
		conf.pauseChan = make(chan struct{})
		conf.resumeChan = make(chan struct{})
		conf.PluginRunner.Init(1, 1)
		return conf
	}
	for i := 0; i < 1000; i++ {
		aom.AddCachedConfig(newLogstoreConfig(strconv.Itoa(i)+"/1", strconv.Itoa(i)), time.Minute)
	}

	config := newLogstoreConfig("x/1", "x")
	aom.AddCachedConfig(config, time.Second*time.Duration(5))
	require.Equal(t, len(aom.configMap), 1001)

	time.Sleep(time.Second)
	for i := 0; i < 500; i++ {
		var ok bool
		config, ok = aom.GetCachedConfig(strconv.Itoa(i) + "/1")
		require.Equal(t, ok, true)
		require.Equal(t, config.ConfigName, config.configDetailHash)
		require.Equal(t, config.ConfigName, strconv.Itoa(i))
	}
	require.Equal(t, len(aom.configMap), 501)

	time.Sleep(time.Second * time.Duration(8))
	config, ok := aom.GetCachedConfig("x/1")
	require.True(t, config == nil)
	require.Equal(t, ok, false)
}
