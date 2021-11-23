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

package skywalkingv3

import (
	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/agent/v3"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"testing"
)

func TestHandleSingleValue(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	ctx.InitContext("a", "b", "c")
	collector := &test.MockCollector{}
	handleMeterData(ctx, collector, buildMockSingleValueRequest(), "service_111", "instance_222", 123456789)
	validate("./testdata/meter_singlevalue.json", collector.RawLogs, t)
}

func TestHandleHistogram(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	ctx.InitContext("a", "b", "c")
	collector := &test.MockCollector{}
	handleMeterData(ctx, collector, buildMockHistogramRequest(), "service_111", "instance_222", 123456789)
	validate("./testdata/meter_histogram.json", collector.RawLogs, t)
}

func buildMockSingleValueRequest() *v3.MeterData {
	meterData := &v3.MeterData{}
	labels := make([]*v3.Label, 0)
	labels = append(labels, &v3.Label{Name: "ip", Value: "1.2.3.4"})
	labels = append(labels, &v3.Label{Name: "Hahaha", Value: "test"})
	labels = append(labels, &v3.Label{Name: "a", Value: "aaa"})
	meterData.Metric = &v3.MeterData_SingleValue{
		SingleValue: &v3.MeterSingleValue{
			Name:   "i_am_singleValue_metric",
			Labels: labels,
			Value:  123,
		},
	}
	return meterData
}

func buildMockHistogramRequest() *v3.MeterData {
	meterData := &v3.MeterData{}
	labels := make([]*v3.Label, 0)
	labels = append(labels, &v3.Label{Name: "ip", Value: "1.2.3.4"})
	labels = append(labels, &v3.Label{Name: "Hahaha", Value: "test"})
	labels = append(labels, &v3.Label{Name: "a", Value: "aaa"})
	values := make([]*v3.MeterBucketValue, 0)
	values = append(values, &v3.MeterBucketValue{Bucket: 0.1, Count: 5})
	values = append(values, &v3.MeterBucketValue{Bucket: 50, Count: 4})
	values = append(values, &v3.MeterBucketValue{Bucket: 88.8, Count: 3})
	values = append(values, &v3.MeterBucketValue{Bucket: 90, Count: 2})
	values = append(values, &v3.MeterBucketValue{Bucket: 100, Count: 1})
	meterData.Metric = &v3.MeterData_Histogram{
		Histogram: &v3.MeterHistogram{
			Name:   "i_am_histogram_metric",
			Labels: labels,
			Values: values,
		},
	}
	return meterData
}
