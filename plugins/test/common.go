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

package test

import (
	"context"
	"io/ioutil"
	"path/filepath"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/pluginmanager"
	_ "github.com/alibaba/ilogtail/plugins/aggregator/defaultone"
	_ "github.com/alibaba/ilogtail/plugins/flusher/checker"
	_ "github.com/alibaba/ilogtail/plugins/flusher/statistics"
	_ "github.com/alibaba/ilogtail/plugins/flusher/stdout"
)

func GetTestConfig(configName string) string {
	fileName := "./" + configName + ".json"
	byteStr, err := ioutil.ReadFile(filepath.Clean(fileName))
	if err != nil {
		logger.Warning(context.Background(), "read", fileName, "error", err)
	}
	return string(byteStr)
}

func LoadDefaultConfig() *pluginmanager.LogstoreConfig {
	return LoadMockConfig("project", "logstore", "configName", GetTestConfig("config"))
}

// project, logstore, config, jsonStr
func LoadMockConfig(project, logstore, configName, jsonStr string) *pluginmanager.LogstoreConfig {
	err := pluginmanager.LoadLogstoreConfig(project, logstore, configName, 666, jsonStr)
	if err != nil {
		panic(err)
	}
	return pluginmanager.LogtailConfig[configName]
}

func PluginStart() error {
	return pluginmanager.Resume()
}

func PluginStop(forceFlushFlag bool) error {
	return pluginmanager.HoldOn(true)
}

func CreateLogs(kvs ...string) *protocol.Log {
	var slsLog protocol.Log
	for i := 0; i < len(kvs)-1; i += 2 {
		cont := &protocol.Log_Content{Key: kvs[i], Value: kvs[i+1]}
		slsLog.Contents = append(slsLog.Contents, cont)
	}
	slsLog.Time = uint32(time.Now().Unix())
	return &slsLog
}

func CreateLogByFields(fields map[string]string) *protocol.Log {
	var slsLog protocol.Log
	for key, val := range fields {
		cont := &protocol.Log_Content{Key: key, Value: val}
		slsLog.Contents = append(slsLog.Contents, cont)
	}
	slsLog.Time = uint32(time.Now().Unix())
	return &slsLog
}

type MockLog struct {
	Tags   map[string]string
	Fields map[string]string
}

type MockCollector struct {
	Logs    []*MockLog
	RawLogs []*protocol.Log
}

func (c *MockCollector) AddData(
	tags map[string]string, fields map[string]string, t ...time.Time) {
	c.AddDataWithContext(tags, fields, nil, t...)
}

func (c *MockCollector) AddDataArray(
	tags map[string]string, columns []string, values []string, t ...time.Time) {
	c.AddDataArrayWithContext(tags, columns, values, nil, t...)
}

func (c *MockCollector) AddRawLog(log *protocol.Log) {
	c.AddRawLogWithContext(log, nil)
}

func (c *MockCollector) AddDataWithContext(
	tags map[string]string, fields map[string]string, ctx map[string]interface{}, t ...time.Time) {
	c.Logs = append(c.Logs, &MockLog{tags, fields})
}

func (c *MockCollector) AddDataArrayWithContext(
	tags map[string]string, columns []string, values []string, ctx map[string]interface{}, t ...time.Time) {
}

func (c *MockCollector) AddRawLogWithContext(log *protocol.Log, ctx map[string]interface{}) {
	c.RawLogs = append(c.RawLogs, log)
}

type MockMetricCollector struct {
	Logs    []*protocol.Log
	Tags    map[string]string
	RawLogs []*protocol.Log
	// Benchmark switch ignore the cpu and memory performance influences when opening.
	Benchmark bool
}

func (m *MockMetricCollector) AddData(tags map[string]string, fields map[string]string, t ...time.Time) {
	m.AddDataWithContext(tags, fields, nil, t...)
}

func (m *MockMetricCollector) AddDataArray(tags map[string]string, columns []string, values []string, t ...time.Time) {
	m.AddDataArrayWithContext(tags, columns, values, nil, t...)
}

func (m *MockMetricCollector) AddRawLog(log *protocol.Log) {
	m.AddRawLogWithContext(log, nil)
}

func (m *MockMetricCollector) AddDataWithContext(tags map[string]string, fields map[string]string, ctx map[string]interface{}, t ...time.Time) {
	if m.Benchmark {
		return
	}
	var logTime time.Time
	if len(t) == 0 {
		logTime = time.Now()
	} else {
		logTime = t[0]
	}
	slsLog, _ := util.CreateLog(logTime, m.Tags, tags, fields)
	m.Logs = append(m.Logs, slsLog)
}

func (m *MockMetricCollector) AddDataArrayWithContext(tags map[string]string, columns []string, values []string, ctx map[string]interface{}, t ...time.Time) {
	if m.Benchmark {
		return
	}
	var logTime time.Time
	if len(t) == 0 {
		logTime = time.Now()
	} else {
		logTime = t[0]
	}
	slsLog, _ := util.CreateLogByArray(logTime, m.Tags, tags, columns, values)
	m.Logs = append(m.Logs, slsLog)
}

func (m *MockMetricCollector) AddRawLogWithContext(log *protocol.Log, ctx map[string]interface{}) {
	if m.Benchmark {
		return
	}
	m.Logs = append(m.Logs, log)
}
