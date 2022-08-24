package jmxfetch

import (
	"io/ioutil"
	"testing"
	"time"

	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
	"github.com/stretchr/testify/assert"
)

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
instances:
- port: 123
  host: 127.0.0.1
  tags:
  - a:b
`

func TestManager_Register_static_config(t *testing.T) {
	m := createManager("test")
	m.initSuccess = true
	go m.run()
	m.RegisterCollector("test1", &test.MockMetricCollector{}, []*Filter{
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
	m.Register(mock.NewEmptyContext("", "", ""), "test1", map[string]*InstanceInner{
		"11111": {
			Port: 123,
			Host: "127.0.0.1",
			Tags: []string{"a:b"},
		},
	})
	time.Sleep(time.Second * 8)
	bytes, err := ioutil.ReadFile(m.jmxfetchConfPath + "/test1.yaml")
	assert.NoErrorf(t, err, "file not read")
	assert.Equal(t, string(bytes), expectCfg)
}

func TestManager_Register_append_config(t *testing.T) {

}

func TestManager_RegisterCollector_And_Start_Stop(t *testing.T) {
	m := createManager("test")
	m.initSuccess = true
	go m.run()
	m.RegisterCollector("test1", &test.MockMetricCollector{}, []*Filter{
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
	m.UnregisterCollector("test1")
	assert.True(t, len(m.allLoadedCfgs) == 0)
	time.Sleep(time.Second * 5)
	assert.True(t, m.server == nil)
}
