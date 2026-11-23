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

package protocol

import (
	"fmt"
	"testing"
	"time"

	"github.com/smartystreets/goconvey/convey"

	"github.com/alibaba/ilogtail/pkg/models"
)

func TestConverter_ConvertToInfluxdbProtocolStream(t *testing.T) {
	date := time.Now()
	convey.Convey("Given a PipelineGroupEvents", t, func() {
		cases := []struct {
			name        string
			groupEvents *models.PipelineGroupEvents
			wantStream  interface{}
			wantErr     bool
		}{
			{
				name: "contains none MetricsEvents type",
				groupEvents: &models.PipelineGroupEvents{
					Events: []models.PipelineEvent{
						models.NewMetric("cpu", models.MetricTypeCounter, models.NewTags(), time.Now().UnixNano(), &models.MetricSingleValue{Value: 1}, nil),
						models.NewByteArray([]byte("test")),
					},
				},
				wantErr: true,
			},
			{
				name: "contains single value metric only",
				groupEvents: &models.PipelineGroupEvents{
					Events: []models.PipelineEvent{
						models.NewMetric("cpu1", models.MetricTypeCounter, models.NewTagsWithKeyValues("k2", "v2", "k1", "v1"), date.Add(1).UnixNano(), &models.MetricSingleValue{Value: 1}, nil),
						models.NewMetric("cpu2", models.MetricTypeCounter, models.NewTagsWithKeyValues("k3", "v3", "k4", "v4"), date.Add(2).UnixNano(), &models.MetricSingleValue{Value: 10}, nil),
						models.NewMetric("cpu3", models.MetricTypeCounter, models.NewTagsWithKeyValues("k5", "v5", "k6", "v6"), date.Add(3).UnixNano(), &models.MetricSingleValue{Value: 20}, nil),
					},
				},
				wantStream: [][]byte{[]byte(fmt.Sprintf("cpu1,k1=v1,k2=v2 value=1 %d\ncpu2,k3=v3,k4=v4 value=10 %d\ncpu3,k5=v5,k6=v6 value=20 %d\n",
					date.Add(1).UnixNano(), date.Add(2).UnixNano(), date.Add(3).UnixNano()))},
			},
			{
				name: "contains multi fields value metric only",
				groupEvents: &models.PipelineGroupEvents{
					Events: []models.PipelineEvent{
						models.NewMetric("cpu1", models.MetricTypeCounter, models.NewTagsWithKeyValues("k2", "v2", "k1", "v1"), date.Add(1).UnixNano(), models.NewMetricMultiValueWithMap(map[string]float64{"f1": 1}), nil),
						models.NewMetric("cpu2", models.MetricTypeCounter, models.NewTagsWithKeyValues("k3", "v3", "k4", "v4"), date.Add(2).UnixNano(), models.NewMetricMultiValueWithMap(map[string]float64{"f1": 3}), nil),
						models.NewMetric("cpu3", models.MetricTypeCounter, models.NewTagsWithKeyValues("k5", "v5", "k6", "v6"), date.Add(3).UnixNano(), models.NewMetricMultiValueWithMap(map[string]float64{"f1": 5}), nil),
					},
				},
				wantStream: [][]byte{[]byte(fmt.Sprintf("cpu1,k1=v1,k2=v2 f1=1 %d\ncpu2,k3=v3,k4=v4 f1=3 %d\ncpu3,k5=v5,k6=v6 f1=5 %d\n",
					date.Add(1).UnixNano(), date.Add(2).UnixNano(), date.Add(3).UnixNano()))},
			},
			{
				name: "contains multi fields value metric and type value",
				groupEvents: &models.PipelineGroupEvents{
					Events: []models.PipelineEvent{
						models.NewMetric("cpu1", models.MetricTypeCounter, models.NewTagsWithKeyValues("k2", "v2", "k1", "v1"), date.Add(1).UnixNano(), models.NewMetricMultiValueWithMap(map[string]float64{"f1": 1}), nil),
						models.NewMetric("cpu2", models.MetricTypeCounter, models.NewTagsWithKeyValues("k3", "v3", "k4", "v4"), date.Add(2).UnixNano(), &models.MetricSingleValue{Value: 1},
							models.NewMetricTypedValueWithMap(map[string]*models.TypedValue{"f1": {Type: models.ValueTypeString, Value: "f1v"}})),
						models.NewMetric("cpu3", models.MetricTypeCounter, models.NewTagsWithKeyValues("k3", "v3", "k4", "v4"), date.Add(2).UnixNano(), nil,
							models.NewMetricTypedValueWithMap(map[string]*models.TypedValue{"f2": {Type: models.ValueTypeBoolean, Value: true}})),
					},
				},
				wantStream: [][]byte{[]byte(fmt.Sprintf("cpu1,k1=v1,k2=v2 f1=1 %d\ncpu2,k3=v3,k4=v4 value=1,f1=\"f1v\" %d\ncpu3,k3=v3,k4=v4 f2=true %d\n",
					date.Add(1).UnixNano(), date.Add(2).UnixNano(), date.Add(2).UnixNano()))},
			},
		}

		converter, err := NewConverter("influxdb", "custom", nil, nil)
		convey.So(err, convey.ShouldBeNil)

		for _, test := range cases {
			convey.Convey(fmt.Sprintf("When %s, result should match", test.name), func() {
				stream, _, err := converter.ConvertToInfluxdbProtocolStream(test.groupEvents, nil)
				if test.wantErr {
					convey.So(err, convey.ShouldNotBeNil)
				} else {
					convey.So(err, convey.ShouldBeNil)
					convey.So(stream, convey.ShouldResemble, test.wantStream)
				}
			})
		}
	})
}
