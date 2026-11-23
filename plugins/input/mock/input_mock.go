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
	"time"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

type InputMock struct {
	GroupMeta map[string]string
	GroupTags map[string]string
	Tags      map[string]string
	Fields    map[string]interface{}
	Index     int64
}

func (*InputMock) Init(pipeline.Context) (int, error) {
	return 0, nil
}

func (r *InputMock) Description() string {
	return "mock input plugin for logtail"
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
