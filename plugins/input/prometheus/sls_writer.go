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

package prometheus

import (
	"github.com/VictoriaMetrics/VictoriaMetrics/lib/prompbmarshal"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"strings"
)

func appendTSDataToSlsLog(c pipeline.Collector, wr *prompbmarshal.WriteRequest) {
	for i := range wr.Timeseries {
		ts := &wr.Timeseries[i]
		var name string
		var labels helper.MetricLabels
		for _, label := range ts.Labels {
			if label.Name == "__name__" {
				// Prometheus scrape operation reuse bytes to enhance performance, must clone the unsafe string.
				name = strings.Clone(label.Value)
				continue
			}
			// Because AddRawLog operation would join labels to a new string, pass unsafe string would be safe.
			labels.Append(label.Name, label.Value)
		}
		for _, sample := range ts.Samples {
			c.AddRawLog(helper.NewMetricLog(name, sample.Timestamp, sample.Value, &labels))
		}
	}

}
