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
	"strconv"
	"testing"
	"time"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/input"
	"github.com/alibaba/ilogtail/plugins/processor/regex"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/stretchr/testify/suite"
)

func TestLogstroreConfig(t *testing.T) {
	suite.Run(t, new(logstoreConfigTestSuite))
}

type logstoreConfigTestSuite struct {
	suite.Suite
}

func (s *logstoreConfigTestSuite) BeforeTest(suiteName, testName string) {
	logger.Infof(context.Background(), "========== %s %s test start ========================", suiteName, testName)
	LogtailConfig = make(map[string]*LogstoreConfig)
}

func (s *logstoreConfigTestSuite) AfterTest(suiteName, testName string) {
	logger.Infof(context.Background(), "========== %s %s test end =======================", suiteName, testName)

}

func (s *logstoreConfigTestSuite) TestPluginGlobalConfig() {
	str := `{
		"global": {
			"InputIntervalMs": 9999,
			"AggregatIntervalMs": 369,
			"FlushIntervalMs": 323,
			"DefaultLogQueueSize": 21,
			"DefaultLogGroupQueueSize": 31
		},
		"inputs" : [
			{
				"type" : "metric_http",
				"detail" : {
					"Addresses" : [
						"http://config.sls.aliyun.com"
					],
					"IncludeBody" : true
				}
			},
			{
				"type" : "service_canal",
				"detail" : {
					"Host" : "localhost",
					"Password" : "111",
					"User" : "111",
					"DataBases" : ["zc"],
					"IncludeTables" : [".*\\..*"],
					"ServerID" : 12312,
					"TextToString" : true
				}
			}
		],
		"flushers" : [
			{
				"type" : "flusher_stdout",
				"detail" : {

				}
			}
		]
	}`
	s.NoError(LoadMockConfig("project", "logstore", "1", str), "load config fail")
	s.Equal(len(LogtailConfig), 1)
	s.Equal(LogtailConfig["1"].ConfigName, "1")
	config := LogtailConfig["1"]
	s.Equal(config.ProjectName, "project")
	s.Equal(config.LogstoreName, "logstore")
	s.Equal(config.LogstoreKey, int64(666))
	// InputIntervalMs          int
	// AggregatIntervalMs       int
	// FlushIntervalMs          int
	// DefaultLogQueueSize      int
	// DefaultLogGroupQueueSize int
	s.Equal(config.GlobalConfig.AggregatIntervalMs, 369)
	s.Equal(config.GlobalConfig.DefaultLogGroupQueueSize, 31)
	s.Equal(config.GlobalConfig.DefaultLogQueueSize, 21)
	s.Equal(config.GlobalConfig.InputIntervalMs, 9999)
	s.Equal(config.GlobalConfig.FlushIntervalMs, 323)
	s.Equal(config.PluginRunner.(*pluginv1Runner).MetricPlugins[0].Interval, time.Duration(9999)*time.Millisecond)
	s.Equal(config.PluginRunner.(*pluginv1Runner).AggregatorPlugins[0].Interval, time.Duration(369)*time.Millisecond)
	s.Equal(config.PluginRunner.(*pluginv1Runner).FlusherPlugins[0].Interval, time.Duration(323)*time.Millisecond)
}

func (s *logstoreConfigTestSuite) TestLoadConfig() {
	s.NoError(LoadMockConfig("project", "logstore", "1"))
	s.NoError(LoadMockConfig("project", "logstore", "3"))
	s.NoError(LoadMockConfig("project", "logstore", "2"))
	s.Equal(len(LogtailConfig), 3)
	s.Equal(LogtailConfig["1"].ConfigName, "1")
	s.Equal(LogtailConfig["2"].ConfigName, "2")
	s.Equal(LogtailConfig["3"].ConfigName, "3")
	for i := 0; i < 3; i++ {
		config := LogtailConfig[strconv.Itoa(i+1)]
		s.Equal(config.ProjectName, "project")
		s.Equal(config.LogstoreName, "logstore")
		s.Equal(config.LogstoreKey, int64(666))
		s.Equal(len(config.PluginRunner.(*pluginv1Runner).MetricPlugins), 0)
		s.Equal(len(config.PluginRunner.(*pluginv1Runner).ServicePlugins), 1)
		s.Equal(len(config.PluginRunner.(*pluginv1Runner).ProcessorPlugins), 1)
		s.Equal(len(config.PluginRunner.(*pluginv1Runner).AggregatorPlugins), 1)
		s.Equal(len(config.PluginRunner.(*pluginv1Runner).FlusherPlugins), 2)
		// global config
		s.Equal(config.GlobalConfig, &LogtailGlobalConfig)

		// check plugin inner info
		reg, ok := config.PluginRunner.(*pluginv1Runner).ProcessorPlugins[0].Processor.(*regex.ProcessorRegex)
		s.True(ok)
		// "SourceKey": "content",
		// "Regex": "Active connections: (\\d+)\\s+server accepts handled requests\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+Reading: (\\d+) Writing: (\\d+) Waiting: (\\d+).*",
		// "Keys": [
		// 	"connection",
		// 	"accepts",
		// 	"handled",
		// 	"requests",
		// 	"reading",
		// 	"writing",
		// 	"waiting"
		// ],
		// "FullMatch": true,
		// "NoKeyError": true,
		// "NoMatchError": true,
		// "KeepSource": true
		s.True(reg.KeepSource)
		s.True(reg.NoKeyError)
		s.True(reg.NoMatchError)
		s.True(reg.KeepSource)
		s.Equal(reg.SourceKey, "content")
		s.Equal(reg.Regex, "Active connections: (\\d+)\\s+server accepts handled requests\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+Reading: (\\d+) Writing: (\\d+) Waiting: (\\d+).*")
		s.Equal(reg.Keys, []string{
			"connection",
			"accepts",
			"handled",
			"requests",
			"reading",
			"writing",
			"waiting",
		})
	}
}

func Test_hasDockerStdoutInput(t *testing.T) {
	{
		plugins := map[string]interface{}{
			"inputs": []interface{}{
				map[string]interface{}{
					"type": input.ServiceDockerStdoutPluginName,
				},
			},
		}
		require.True(t, hasDockerStdoutInput(plugins))
	}

	{
		// No field inputs.
		plugins := map[string]interface{}{
			"processors": []interface{}{},
		}
		require.False(t, hasDockerStdoutInput(plugins))
	}

	{
		// Empty inputs.
		plugins := map[string]interface{}{
			"inputs": []interface{}{},
		}
		require.False(t, hasDockerStdoutInput(plugins))
	}

	{
		// Invalid inputs.
		plugins := map[string]interface{}{
			"inputs": "xxxx",
		}
		require.False(t, hasDockerStdoutInput(plugins))
	}
}

func Test_extractTags(t *testing.T) {
	l := &protocol.Log{}
	extractTags([]byte("k1~=~v1^^^k2~=~v2"), l)
	assert.Equal(t, l.Contents[0].Key, "k1")
	assert.Equal(t, l.Contents[0].Value, "v1")
	assert.Equal(t, l.Contents[1].Key, "k2")
	assert.Equal(t, l.Contents[1].Value, "v2")

	l = &protocol.Log{}
	extractTags([]byte("^^^k2~=~v2"), l)
	assert.Equal(t, l.Contents[0].Key, "k2")
	assert.Equal(t, l.Contents[0].Value, "v2")

	l = &protocol.Log{}
	extractTags([]byte("^^^k2"), l)
	assert.Equal(t, l.Contents[0].Key, "__tag__:__prefix__0")
	assert.Equal(t, l.Contents[0].Value, "k2")
}

func TestLogstoreConfig_ProcessRawLogV2(t *testing.T) {
	rawLogs := []byte("12345")
	topic := "topic"
	tags := []byte("")
	str := helper.ZeroCopyString(rawLogs)
	l := new(LogstoreConfig)
	l.PluginRunner = &pluginv1Runner{
		LogsChan: make(chan *ilogtail.LogWithContext, 10),
	}

	{
		assert.Equal(t, 0, l.ProcessRawLogV2(rawLogs, "", topic, tags))
		assert.Equal(t, 1, len(l.PluginRunner.(*pluginv1Runner).LogsChan))
		log := <-l.PluginRunner.(*pluginv1Runner).LogsChan
		assert.Equal(t, log.Log.Contents[0].GetValue(), str)
		assert.Equal(t, log.Log.Contents[1].GetValue(), topic)
		assert.True(t, helper.IsSafeString(log.Log.Contents[0].GetValue(), str))
		assert.False(t, helper.IsSafeString(log.Log.Contents[1].GetValue(), topic))
	}

	{
		tags = []byte("k1~=~v1^^^k2~=~v2")
		tagsStr := helper.ZeroCopyString(tags)
		assert.Equal(t, 0, l.ProcessRawLogV2(rawLogs, "", topic, tags))
		assert.Equal(t, 1, len(l.PluginRunner.(*pluginv1Runner).LogsChan))
		log := <-l.PluginRunner.(*pluginv1Runner).LogsChan
		assert.Equal(t, log.Log.Contents[0].GetValue(), str)
		assert.Equal(t, log.Log.Contents[1].GetValue(), topic)
		assert.Equal(t, 4, len(log.Log.Contents))

		assert.Equal(t, log.Log.Contents[0].GetValue(), str)
		assert.Equal(t, log.Log.Contents[1].GetValue(), topic)
		assert.Equal(t, log.Log.Contents[2].GetKey(), "k1")
		assert.Equal(t, log.Log.Contents[2].GetValue(), "v1")
		assert.Equal(t, log.Log.Contents[3].GetKey(), "k2")
		assert.Equal(t, log.Log.Contents[3].GetValue(), "v2")

		assert.True(t, helper.IsSafeString(log.Log.Contents[0].GetValue(), str))
		assert.False(t, helper.IsSafeString(log.Log.Contents[1].GetValue(), topic))
		assert.True(t, helper.IsSafeString(log.Log.Contents[2].GetKey(), tagsStr))
		assert.True(t, helper.IsSafeString(log.Log.Contents[2].GetValue(), tagsStr))
		assert.True(t, helper.IsSafeString(log.Log.Contents[3].GetKey(), tagsStr))
		assert.True(t, helper.IsSafeString(log.Log.Contents[3].GetValue(), tagsStr))
	}

	{
		tags = []byte("^^^k2~=~v2")
		tagsStr := helper.ZeroCopyString(tags)
		assert.Equal(t, 0, l.ProcessRawLogV2(rawLogs, "", topic, tags))
		assert.Equal(t, 1, len(l.PluginRunner.(*pluginv1Runner).LogsChan))
		log := <-l.PluginRunner.(*pluginv1Runner).LogsChan
		assert.Equal(t, log.Log.Contents[0].GetValue(), str)
		assert.Equal(t, log.Log.Contents[1].GetValue(), topic)
		assert.Equal(t, 3, len(log.Log.Contents))

		assert.Equal(t, log.Log.Contents[0].GetValue(), str)
		assert.Equal(t, log.Log.Contents[1].GetValue(), topic)
		assert.Equal(t, log.Log.Contents[2].GetKey(), "k2")
		assert.Equal(t, log.Log.Contents[2].GetValue(), "v2")

		assert.True(t, helper.IsSafeString(log.Log.Contents[0].GetValue(), str))
		assert.False(t, helper.IsSafeString(log.Log.Contents[1].GetValue(), topic))
		assert.True(t, helper.IsSafeString(log.Log.Contents[2].GetKey(), tagsStr))
		assert.True(t, helper.IsSafeString(log.Log.Contents[2].GetValue(), tagsStr))
	}

	{
		tags = []byte("^^^k2^^^k3")
		tagsStr := helper.ZeroCopyString(tags)
		assert.Equal(t, 0, l.ProcessRawLogV2(rawLogs, "", topic, tags))
		assert.Equal(t, 1, len(l.PluginRunner.(*pluginv1Runner).LogsChan))
		log := <-l.PluginRunner.(*pluginv1Runner).LogsChan
		assert.Equal(t, log.Log.Contents[0].GetValue(), str)
		assert.Equal(t, log.Log.Contents[1].GetValue(), topic)
		assert.Equal(t, 4, len(log.Log.Contents))

		assert.Equal(t, log.Log.Contents[0].GetValue(), str)
		assert.Equal(t, log.Log.Contents[1].GetValue(), topic)
		assert.Equal(t, log.Log.Contents[2].GetKey(), "__tag__:__prefix__0")
		assert.Equal(t, log.Log.Contents[2].GetValue(), "k2")
		assert.Equal(t, log.Log.Contents[3].GetKey(), "__tag__:__prefix__1")
		assert.Equal(t, log.Log.Contents[3].GetValue(), "k3")

		assert.True(t, helper.IsSafeString(log.Log.Contents[0].GetValue(), str))
		assert.False(t, helper.IsSafeString(log.Log.Contents[1].GetValue(), topic))
		assert.True(t, helper.IsSafeString(log.Log.Contents[2].GetKey(), tagsStr))
		assert.True(t, helper.IsSafeString(log.Log.Contents[2].GetValue(), tagsStr))
		assert.True(t, helper.IsSafeString(log.Log.Contents[3].GetKey(), tagsStr))
		assert.True(t, helper.IsSafeString(log.Log.Contents[3].GetValue(), tagsStr))
	}
}
