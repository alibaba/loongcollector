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

// MetricsExample struct implements the MetricInput interface.
// The plugin has a counter field, which would be increment every 5 seconds.
// And, the value of this counter will be sent as metrics data.
type MetricsExample struct {
	counter      int
	gauge        int
	commonLabels helper.MetricLabels
	labels       string
}

// Init method would be triggered before working. In the example plugin, we set the initial
// value of counter to 100. And we return 0 to use the default trigger interval.
func (m *MetricsExample) Init(context pipeline.Context) (int, error) {
	// set the initial value
	m.counter = 100
	// use helper.KeyValues to store metric labels
	m.commonLabels.Append("hostname", util.GetHostName())
	m.commonLabels.Append("ip", util.GetIPAddress())
	// convert the commonLabels to string to reduce memory cost because the labels is the fixed value.
	m.labels = m.commonLabels.String()
	return 0, nil
}

func (m *MetricsExample) Description() string {
	return "This is a metric input example plugin, this plugin would show how to write a simple metric input plugin."
}

// Collect is called every trigger interval to collect the metrics and send them to the collector.
func (m *MetricsExample) Collect(collector pipeline.Collector) error {
	// counter increment
	m.counter++
	// create a random value as gauge value
	m.gauge = rand.Intn(100) //nolint:gosec

	// collect the metrics
	collector.AddRawLog(helper.NewMetricLog("example_counter", time.Now().UnixNano(), float64(m.counter), &m.commonLabels))
	collector.AddRawLog(helper.NewMetricLog("example_gauge", time.Now().UnixNano(), float64(m.gauge), &m.commonLabels))
	return nil
}

// Register the plugin to the MetricInputs array.
func init() {
	pipeline.MetricInputs["metric_input_example"] = func() pipeline.MetricInput {
		return &MetricsExample{
			// here you could set default value.
		}
	}
}
