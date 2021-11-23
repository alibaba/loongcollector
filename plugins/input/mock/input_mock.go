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

package mock

import (
	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/helper"

	"strconv"
	"time"
)

type InputMock struct {
	Tags                  map[string]string
	Fields                map[string]string
	Index                 int64
	OpenPrometheusPattern bool

	context  ilogtail.Context
	labelStr string
}

func (r *InputMock) Init(context ilogtail.Context) (int, error) {
	r.context = context
	var labels helper.KeyValues
	if r.OpenPrometheusPattern {
		for k, v := range r.Tags {
			labels.Append(k, v)
		}
		for k, v := range r.Fields {
			labels.Append(k, v)
		}
	}
	labels.Sort()
	r.labelStr = labels.String()
	return 0, nil
}

func (r *InputMock) Description() string {
	return "mock input plugin for logtail"
}

func (r *InputMock) Collect(collector ilogtail.Collector) error {
	r.Index++
	if r.OpenPrometheusPattern {
		helper.AddMetric(collector, "metrics_mock", time.Now(), r.labelStr, float64(r.Index))
	} else {
		// original log pattern.
		r.Fields["Index"] = strconv.FormatInt(r.Index, 10)
		collector.AddData(r.Tags, r.Fields)
	}
	return nil
}

func init() {
	ilogtail.MetricInputs["metric_mock"] = func() ilogtail.MetricInput {
		return &InputMock{
			Index:  0,
			Tags:   make(map[string]string),
			Fields: make(map[string]string),
		}
	}
}
