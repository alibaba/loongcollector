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

package example

import (
	"math/rand"
	"time"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
)

// MetricsCheckpointExample struct implements the MetricInput interface.
// The plugin has a counter field, which would be increment every 5 seconds.
// And, the value of this counter will be sent as metrics data.
type MetricsCheckpointExample struct {
	counter      int
	gauge        int
	commonLabels helper.MetricLabels

	context pipeline.Context
}

// Init method would be triggered before working. In the example plugin, we set the initial
// value of counter to 100. And we return 0 to use the default trigger interval.
func (m *MetricsCheckpointExample) Init(context pipeline.Context) (int, error) {
	// set the initial value
	m.context = context
	checkpointExist := context.GetCheckPointObject("metric_checkpoint_example", &m.counter)
	if !checkpointExist {
		m.counter = 100
	}
	// use helper.KeyValues to store metric labels
	m.commonLabels.Append("hostname", util.GetHostName())
	m.commonLabels.Append("ip", util.GetIPAddress())
	// convert the commonLabels to string to reduce memory cost because the labels is the fixed value.
	return 0, nil
}

func (m *MetricsCheckpointExample) Description() string {
	return "This is a metric input example plugin, this plugin would show how to write a simple metric input plugin."
}

// Collect is called every trigger interval to collect the metrics and send them to the collector.
func (m *MetricsCheckpointExample) Collect(collector pipeline.Collector) error {
	// counter increment
	m.counter++
	// create a random value as gauge value
	//nolint:gosec
	m.gauge = rand.Intn(100)

	// collect the metrics
	collector.AddRawLog(helper.NewMetricLog("example_counter", time.Now().UnixNano(), float64(m.counter), &m.commonLabels))
	collector.AddRawLog(helper.NewMetricLog("example_gauge", time.Now().UnixNano(), float64(m.gauge), &m.commonLabels))
	_ = m.context.SaveCheckPointObject("metric_checkpoint_example", &m.counter)
	return nil
}

// Register the plugin to the MetricInputs array.
func init() {
	pipeline.MetricInputs["metric_checkpoint_example"] = func() pipeline.MetricInput {
		return &MetricsCheckpointExample{
			// here you could set default value.
		}
	}
}
