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
	"fmt"
	"strconv"
	"time"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

type InputMock struct {
	GroupMeta             map[string]string
	GroupTags             map[string]string
	Tags                  map[string]string
	Fields                map[string]interface{}
	Index                 int64
	OpenPrometheusPattern bool

	context pipeline.Context
	labels  helper.MetricLabels
}

func (r *InputMock) Init(context pipeline.Context) (int, error) {
	r.context = context
	if r.OpenPrometheusPattern {
		for k, v := range r.Tags {
			r.labels.Append(k, v)
		}
		for k, v := range r.Fields {
			r.labels.Append(k, fmt.Sprint(v))
		}
	}
	return 0, nil
}

func (r *InputMock) Description() string {
	return "mock input plugin for logtail"
}

func (r *InputMock) Collect(collector pipeline.Collector) error {
	r.Index++
	if r.OpenPrometheusPattern {
		collector.AddRawLog(helper.NewMetricLog("metrics_mock", time.Now().UnixNano(), float64(r.Index), &r.labels))
	} else {
		// original log pattern.
		fields := make(map[string]string)
		fields["Index"] = strconv.FormatInt(r.Index, 10)
		for k, v := range r.Fields {
			fields[k] = fmt.Sprint(v)
		}
		collector.AddData(r.Tags, fields)
	}
	return nil
}

func (r *InputMock) Read(context pipeline.PipelineContext) error {
	r.Index++
	group := models.NewGroup(models.NewMetadataWithMap(r.GroupMeta), models.NewTagsWithMap(r.GroupTags))
	singleValue := models.NewSingleValueMetric("single_metrics_mock", models.MetricTypeCounter, models.NewTagsWithMap(r.Tags), time.Now().UnixNano(), r.Index)
	values := models.NewMetricMultiValue()
	values.Add("Index", float64(r.Index))
	typedValues := models.NewMetricTypedValues()
	for k, v := range r.Fields {
		if fv, ok := v.(float64); ok {
			values.Add(k, fv)
		} else if bv, ok := v.(bool); ok {
			typedValues.Add(k, &models.TypedValue{Type: models.ValueTypeBoolean, Value: bv})
		} else {
			typedValues.Add(k, &models.TypedValue{Type: models.ValueTypeString, Value: fmt.Sprint(v)})
		}
	}
	multiValues := models.NewMetric("multi_values_metrics_mock", models.MetricTypeUntyped, models.NewTagsWithMap(r.Tags), time.Now().UnixNano(), values, typedValues)
	context.Collector().Collect(group, singleValue, multiValues)
	return nil
}

func init() {
	pipeline.MetricInputs["metric_mock"] = func() pipeline.MetricInput {
		return &InputMock{
			Index:     0,
			GroupMeta: make(map[string]string),
			GroupTags: make(map[string]string),
			Tags:      make(map[string]string),
			Fields:    make(map[string]interface{}),
		}
	}
}
