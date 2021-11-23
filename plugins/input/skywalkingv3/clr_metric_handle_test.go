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
	"context"
	"testing"

	common "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/agent/v3"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestClrMetrics(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	ctx.InitContext("a", "b", "c")
	collector := &test.MockCollector{}
	handler := &CLRMetricHandler{
		ctx,
		collector,
		5000,
		-1,
	}
	_, _ = handler.Collect(context.Background(), buildMockClrMetricRequest())
	validate("./testdata/clr_metrics.json", collector.RawLogs, t)
}

func buildMockClrMetricRequest() *v3.CLRMetricCollection {
	req := &v3.CLRMetricCollection{}
	req.Service = "service_1"
	req.ServiceInstance = "instance_1"
	metrics := make([]*v3.CLRMetric, 0)
	metric := &v3.CLRMetric{
		Time: 15000,
		Cpu:  &common.CPU{UsagePercent: 0.5},
		Gc: &v3.ClrGC{
			Gen0CollectCount: 1,
			Gen1CollectCount: 2,
			Gen2CollectCount: 3,
			HeapMemory:       123456,
		},
		Thread: &v3.ClrThread{
			AvailableCompletionPortThreads: 999,
			AvailableWorkerThreads:         888,
			MaxCompletionPortThreads:       777,
			MaxWorkerThreads:               666,
		},
	}
	metrics = append(metrics, metric)
	req.Metrics = metrics
	return req
}
