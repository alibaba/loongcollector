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

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/stretchr/testify/suite"

	global_config "github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/extension/basicauth"
	"github.com/alibaba/ilogtail/plugins/input"
	"github.com/alibaba/ilogtail/plugins/processor/regex"
)

func TestLogstroreConfig(t *testing.T) {
	suite.Run(t, new(logstoreConfigTestSuite))
}

type logstoreConfigTestSuite struct {
	suite.Suite
}

func (s *logstoreConfigTestSuite) BeforeTest(suiteName, testName string) {
	logger.Infof(context.Background(), "========== %s %s test start ========================", suiteName, testName)
	LogtailConfigLock.Lock()
	LogtailConfig = make(map[string]*LogstoreConfig)
	LogtailConfigLock.Unlock()
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
	s.NoError(LoadAndStartMockConfig("project", "logstore", "1", str), "load config fail")
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
	s.Equal(config.PluginRunner.(*pluginRunner).MetricPlugins[0].Interval, time.Duration(9999)*time.Millisecond)
	s.Empty(config.PluginRunner.(*pluginRunner).AggregatorPlugins)
	s.Equal(config.PluginRunner.(*pluginRunner).FlusherPlugins[0].Interval, time.Duration(323)*time.Millisecond)
}

func (s *logstoreConfigTestSuite) TestLoadConfig() {
	s.NoError(LoadAndStartMockConfig("project", "logstore", "1"))
	s.NoError(LoadAndStartMockConfig("project", "logstore", "3"))
	s.NoError(LoadAndStartMockConfig("project", "logstore", "2"))
	s.Equal(len(LogtailConfig), 3)
	s.Equal(LogtailConfig["1"].ConfigName, "1")
	s.Equal(LogtailConfig["2"].ConfigName, "2")
	s.Equal(LogtailConfig["3"].ConfigName, "3")
	for i := 0; i < 3; i++ {
		config := LogtailConfig[strconv.Itoa(i+1)]
		s.Equal(config.ProjectName, "project")
		s.Equal(config.LogstoreName, "logstore")
		s.Equal(config.LogstoreKey, int64(666))
		s.Equal(len(config.PluginRunner.(*pluginRunner).MetricPlugins), 0)
		s.Equal(len(config.PluginRunner.(*pluginRunner).ServicePlugins), 1)
		s.Equal(len(config.PluginRunner.(*pluginRunner).ProcessorPlugins), 1)
		s.Equal(len(config.PluginRunner.(*pluginRunner).AggregatorPlugins), 1)
		s.Equal(len(config.PluginRunner.(*pluginRunner).FlusherPlugins), 2)
		// global config
		s.Equal(config.GlobalConfig, &global_config.LoongcollectorGlobalConfig)

		// check plugin inner info
		reg, ok := config.PluginRunner.(*pluginRunner).ProcessorPlugins[0].Processor.(*regex.ProcessorRegex)
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

func (s *logstoreConfigTestSuite) TestLoadConfigWithExtension() {
	jsonStr := `
	{
		"inputs": [
			{
				"type": "service_mock",
				"detail": {
					"LogsPerSecond": 100,
					"Fields": {
						"content": "Active connections: 1\nserver accepts handled requests\n 6079 6079 11596\n Reading: 0 Writing: 1 Waiting: 0"
					}
				}
			}
		],
		"processors": [
			{
				"type": "processor_regex",
				"detail": {
					"SourceKey": "content",
					"Regex": "Active connections: (\\d+)\\s+server accepts handled requests\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+Reading: (\\d+) Writing: (\\d+) Waiting: (\\d+).*",
					"Keys": [
						"connection",
						"accepts",
						"handled",
						"requests",
						"reading",
						"writing",
						"waiting"
					],
					"FullMatch": true,
					"NoKeyError": true,
					"NoMatchError": true,
					"KeepSource": true
				}
			}
		],
		"aggregators": [
			{
				"type": "aggregator_default"
			}
		],
		"flushers": [
			{
				"type": "flusher_statistics",
				"detail": {
					"GeneratePB": true
				}
			},
			{
				"type": "flusher_checker"
			}
		],
		"extensions": [
			{
				"type": "ext_basicauth/basicauth_user1",
				"detail": {
					"Username": "user1",
					"Password": "pwd1"
				}
			}
		]
	}
`

	s.NoError(LoadAndStartMockConfig("project", "logstore", "test", jsonStr))
	s.Equal(len(LogtailConfig), 1)
	config := LogtailConfig["test"]
	s.Equal(config.ProjectName, "project")
	s.Equal(config.LogstoreName, "logstore")
	s.Equal(config.ConfigName, "test")
	s.Equal(config.LogstoreKey, int64(666))
	s.Equal(len(config.PluginRunner.(*pluginRunner).MetricPlugins), 0)
	s.Equal(len(config.PluginRunner.(*pluginRunner).ServicePlugins), 1)
	s.Equal(len(config.PluginRunner.(*pluginRunner).ProcessorPlugins), 1)
	s.Equal(len(config.PluginRunner.(*pluginRunner).AggregatorPlugins), 1)
	s.Equal(len(config.PluginRunner.(*pluginRunner).FlusherPlugins), 2)
	s.Equal(len(config.PluginRunner.(*pluginRunner).ExtensionPlugins), 1)
	// global config
	s.Equal(config.GlobalConfig, &global_config.LoongcollectorGlobalConfig)

	// check plugin inner info
	_, ok := config.PluginRunner.(*pluginRunner).ProcessorPlugins[0].Processor.(*regex.ProcessorRegex)
	s.True(ok)
	_, ok = config.PluginRunner.(*pluginRunner).ExtensionPlugins["ext_basicauth/basicauth_user1"].(*basicauth.ExtensionBasicAuth)
	s.True(ok)
}

func (s *logstoreConfigTestSuite) TestGetExtension() {
	jsonStr := `
	{
		"inputs": [
			{
				"type": "service_mock",
				"detail": {
					"LogsPerSecond": 100,
					"Fields": {
						"content": "Active connections: 1\nserver accepts handled requests\n 6079 6079 11596\n Reading: 0 Writing: 1 Waiting: 0"
					}
				}
			}
		],
		"processors": [
			{
				"type": "processor_regex",
				"detail": {
					"SourceKey": "content",
					"Regex": "Active connections: (\\d+)\\s+server accepts handled requests\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+Reading: (\\d+) Writing: (\\d+) Waiting: (\\d+).*",
					"Keys": [
						"connection",
						"accepts",
						"handled",
						"requests",
						"reading",
						"writing",
						"waiting"
					],
					"FullMatch": true,
					"NoKeyError": true,
					"NoMatchError": true,
					"KeepSource": true
				}
			}
		],
		"aggregators": [
			{
				"type": "aggregator_default"
			}
		],
		"flushers": [
			{
				"type": "flusher_statistics",
				"detail": {
					"GeneratePB": true
				}
			},
			{
				"type": "flusher_checker"
			}
		],
		"extensions": [
			{
				"type": "ext_basicauth/basicauth_user1",
				"detail": {
					"Username": "user1",
					"Password": "pwd1"
				}
			}
		]
	}
`

	s.NoError(LoadAndStartMockConfig("project", "logstore", "test", jsonStr))
	s.Equal(len(LogtailConfig), 1)
	config := LogtailConfig["test"]
	s.Equal(config.ProjectName, "project")
	s.Equal(config.LogstoreName, "logstore")
	s.Equal(config.ConfigName, "test")
	s.Equal(config.LogstoreKey, int64(666))
	s.Equal(len(config.PluginRunner.(*pluginRunner).MetricPlugins), 0)
	s.Equal(len(config.PluginRunner.(*pluginRunner).ServicePlugins), 1)
	s.Equal(len(config.PluginRunner.(*pluginRunner).ProcessorPlugins), 1)
	s.Equal(len(config.PluginRunner.(*pluginRunner).AggregatorPlugins), 1)
	s.Equal(len(config.PluginRunner.(*pluginRunner).FlusherPlugins), 2)
	s.Equal(len(config.PluginRunner.(*pluginRunner).ExtensionPlugins), 1)
	// global config
	s.Equal(config.GlobalConfig, &global_config.LoongcollectorGlobalConfig)

	// check plugin inner info
	_, ok := config.PluginRunner.(*pluginRunner).ProcessorPlugins[0].Processor.(*regex.ProcessorRegex)
	s.True(ok)
	_, ok = config.PluginRunner.(*pluginRunner).ExtensionPlugins["ext_basicauth/basicauth_user1"].(*basicauth.ExtensionBasicAuth)
	s.True(ok)

	ext, err := config.Context.GetExtension("ext_basicauth/basicauth_user1", nil)
	s.Nil(err)
	s.NotNil(ext)

	ext2, err := config.Context.GetExtension("ext_basicauth", map[string]interface{}{"Username": "user2", "Password": "pwd2"})
	s.Nil(err)
	s.NotNil(ext)
	s.NotEqual(ext, ext2)
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

func Test_extractTagsToLogTags(t *testing.T) {
	logTag := extractTagsToLogTags([]byte("k1~=~v1^^^k2~=~v2"))
	assert.Equal(t, logTag[0].Key, "k1")
	assert.Equal(t, logTag[0].Value, "v1")
	assert.Equal(t, logTag[1].Key, "k2")
	assert.Equal(t, logTag[1].Value, "v2")

	logTag = extractTagsToLogTags([]byte("^^^k2~=~v2"))
	assert.Equal(t, logTag[0].Key, "k2")
	assert.Equal(t, logTag[0].Value, "v2")

	logTag = extractTagsToLogTags([]byte("^^^k2"))
	assert.Equal(t, logTag[0].Key, "__tag__:__prefix__0")
	assert.Equal(t, logTag[0].Value, "k2")
}

func TestLogstoreConfig_ProcessRawLogV2(t *testing.T) {
	for _, rawTags := range []string{"", "k1~=~v1^^^k2~=~v2", "^^^k2~=~v2", "^^^k2^^^k3"} {
		t.Run(rawTags, func(t *testing.T) {
			runner := &rawLogCaptureRunner{LogsChan: make(chan *pipeline.LogWithContext, 1)}
			config := &LogstoreConfig{PluginRunner: runner}
			assert.Equal(t, 0, config.ProcessRawLogV2([]byte("12345"), "pack", "topic", []byte(rawTags)))
			in := <-runner.LogsChan
			require.Len(t, in.Log.Contents, 2)
			assert.Equal(t, "12345", in.Log.Contents[0].Value)
			assert.Equal(t, "topic", in.Log.Contents[1].Value)
			assert.Equal(t, "pack", in.Context["source"])
			assert.Equal(t, "topic", in.Context["topic"])
			assert.Equal(t, extractTagsToLogTags([]byte(rawTags)), in.Context["tags"])
		})
	}
}

func Test_genPluginMeta(t *testing.T) {
	l := new(LogstoreConfig)
	{
		result := l.genPluginMeta("testPlugin")
		assert.Equal(t, "testPlugin", result.PluginType)
		assert.Regexp(t, `testPlugin/\d+`, result.PluginTypeWithID)
		assert.Regexp(t, `\d+`, result.PluginID)
	}
	{
		result := l.genPluginMeta("testPlugin")
		assert.Equal(t, "testPlugin", result.PluginType)
		assert.Regexp(t, `testPlugin/\d+`, result.PluginTypeWithID)
		assert.Regexp(t, `\d+`, result.PluginID)
	}
	{
		result := l.genPluginMeta("testPlugin")
		assert.Equal(t, "testPlugin", result.PluginType)
		assert.Regexp(t, `testPlugin/\d+`, result.PluginTypeWithID)
		assert.Regexp(t, `\d+`, result.PluginID)
	}
	{
		result := l.genPluginMeta("testPlugin/customID")
		assert.Equal(t, "testPlugin", result.PluginType)
		assert.Equal(t, "testPlugin/customID", result.PluginTypeWithID)
		assert.Equal(t, "customID", result.PluginID)
	}
	{
		result := l.genPluginMeta("testPlugin/customID")
		assert.Equal(t, "testPlugin", result.PluginType)
		assert.Equal(t, "testPlugin/customID", result.PluginTypeWithID)
		assert.Equal(t, "customID", result.PluginID)
	}
	{
		result := l.genPluginMeta("testPlugin/customID")
		assert.Equal(t, "testPlugin", result.PluginType)
		assert.Equal(t, "testPlugin/customID", result.PluginTypeWithID)
		assert.Equal(t, "customID", result.PluginID)
	}
	{
		result := l.genPluginMeta("testPlugin/customID/123")
		assert.Equal(t, "testPlugin", result.PluginType)
		assert.Equal(t, "testPlugin/customID", result.PluginTypeWithID)
		assert.Equal(t, "customID", result.PluginID)
	}
}

// Capture the public raw-log input boundary independently of its runtime.
type rawLogCaptureRunner struct {
	PluginRunner
	LogstoreConfig *LogstoreConfig
	LogsChan       chan *pipeline.LogWithContext
}

func (r *rawLogCaptureRunner) ReceiveRawLog(log *pipeline.LogWithContext) { r.LogsChan <- log }

func TestLogstoreConfig_ProcessPipelineEventGroup_Topic(t *testing.T) {
	tests := []struct {
		name string
		tags map[string]string
	}{
		{name: "absent topic", tags: map[string]string{"host": "h1"}},
		{name: "empty topic", tags: map[string]string{"__topic__": "", "host": "h1"}},
		{name: "nonempty topic", tags: map[string]string{"__topic__": "access", "host": "h1"}},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			groupInfo := models.NewGroup(models.NewMetadata(), models.NewTagsWithMap(tt.tags))
			log := models.NewLog("", []byte("hello"), "", "", "", models.NewTags(), 1_000_000_000)
			pbGroup, err := helper.TransferPipelineEventGroupToPB(groupInfo, []models.PipelineEvent{log})
			require.NoError(t, err)
			pbBytes, err := pbGroup.Marshal()
			require.NoError(t, err)

			ctx := &mockContect{}
			l := &LogstoreConfig{ConfigName: "test-topic", Context: &ContextImp{}}
			l.Context.InitContext("", "", l.ConfigName)
			l.PluginRunner = &pluginRunner{LogstoreConfig: l, InputPipeContext: ctx}
			require.Equal(t, 0, l.ProcessPipelineEventGroup(pbBytes, "pack-1"))
			require.Len(t, ctx.logs, 1)
			// The typed protocol retains the reserved topic for native SLS serialization.
			tags := make(map[string]string)
			for k, v := range ctx.logs[0].Group.Tags.Iterator() {
				tags[k] = v
			}
			assert.Equal(t, tt.tags, tags)
			assert.Equal(t, "pack-1", ctx.logs[0].Group.Metadata.Get(ctxKeySource))
			require.Len(t, ctx.logs[0].Events, 1)
			assert.IsType(t, &models.Log{}, ctx.logs[0].Events[0])
		})
	}
}
