// Copyright 2023 iLogtail Authors
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

package logtoslsmetric

import (
	"strconv"
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/processor/internal/processortest"

	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func BenchmarkProcessorLogSlsMetricTest(b *testing.B) {
	for _, count := range []int{10, 100, 1000, 10000} {
		b.Run(strconv.Itoa(count), func(b *testing.B) {
			processor := &ProcessorLogToSlsMetric{
				MetricTimeKey:   "timeKey",
				MetricValues:    map[string]string{"nameA": "valueA", "nameB": "valueB"},
				MetricLabelKeys: []string{"labelA", "labelB", "labelC"},
			}
			require.NoError(b, processor.Init(mock.NewEmptyContext("p", "l", "c")))
			group := &models.PipelineGroupEvents{Events: make([]models.PipelineEvent, count)}
			for i := range group.Events {
				group.Events[i] = processortest.Log(
					"labelA", "1", "labelB", "2", "labelC", "3",
					"nameA", "myname", "valueA", "1.0", "nameB", "myname2",
					"valueB", "2.0", "timeKey", "1658806869597190887",
				)
			}

			context := helper.NewNoopPipelineContext()
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				processor.Process(group, context)
			}
		})
	}
}
