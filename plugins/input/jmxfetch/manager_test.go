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

package jmxfetch

import (
	"os"
	"testing"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"github.com/stretchr/testify/assert"
)

func init() {
	logger.InitTestLogger(logger.OptionDebugLevel, logger.OptionOpenConsole, logger.OptionSyncLogger)
}

var expectCfg = `init_config:
  conf:
  - include:
      domain: kafka.producer
      bean_regex: kafka\.producer:type=producer-metrics,client-id=.*
      type: "111"
      attribute:
        response-rate:
          metric_type: gauge
          alias: kafka.producer.response_rate
  is_jmx: true
  new_gc_metrics: false
instances:
- port: 123
  host: 127.0.0.1
  tags:
  - a:b
  name: ""
  collect_default_jvm_metrics: false
`

func TestManager_Register_static_config(t *testing.T) {
	m := createManager("test")
	manager = m
	m.initSuccess = true
	m.manualInstall()
	m.initConfDir()
	m.collector = NewLogCollector("test")
	go m.run()
	go m.collector.Run()
	m.RegisterCollector(mock.NewEmptyContext("", "", "11"), "test1", &test.MockMetricCollector{}, []*FilterInner{
		{
			Domain:    "kafka.producer",
			BeanRegex: "kafka\\.producer:type=producer-metrics,client-id=.*",
			Type:      "111",
			Attribute: map[string]struct {
				MetricType string `yaml:"metric_type,omitempty"`
				Alias      string `yaml:"alias,omitempty"`
			}{
				"response-rate": {
					MetricType: "gauge",
					Alias:      "kafka.producer.response_rate",
				},
			},
		},
	})
	m.Register("test1", map[string]*InstanceInner{
		"11111": {
			Port: 123,
			Host: "127.0.0.1",
			Tags: []string{"a:b"},
		},
	}, false)
	time.Sleep(time.Second * 8)
	bytes, err := os.ReadFile(m.jmxfetchConfPath + "/test1.yaml")
	assert.NoErrorf(t, err, "file not read")
	assert.Equal(t, string(bytes), expectCfg)
}

func TestManager_RegisterCollector_And_Start_Stop(t *testing.T) {
	m := createManager("test")
	manager = m
	m.collector = NewLogCollector("test")
	m.initSuccess = true
	m.manualInstall()
	m.initConfDir()
	go m.run()
	go m.collector.Run()
	m.RegisterCollector(mock.NewEmptyContext("", "", "11"), "test1", &test.MockMetricCollector{}, []*FilterInner{
		{
			Domain:    "kafka.producer",
			BeanRegex: "kafka\\.producer:type=producer-metrics,client-id=.*",
			Type:      "111",
			Attribute: map[string]struct {
				MetricType string `yaml:"metric_type,omitempty"`
				Alias      string `yaml:"alias,omitempty"`
			}{
				"response-rate": {
					MetricType: "gauge",
					Alias:      "kafka.producer.response_rate",
				},
			},
		},
	})
	assert.True(t, len(m.allLoadedCfgs) == 1)
	time.Sleep(time.Second * 7)
	m.UnregisterCollector("test1")
	assert.True(t, len(m.allLoadedCfgs) == 0)
	time.Sleep(time.Second * 7)
	assert.True(t, m.server == nil)
}
