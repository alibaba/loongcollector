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

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

type MockParam struct {
	name     string
	num      int
	mockFunc func(int) []*protocol.Log
}

/*
	Test multiple matchs performance
*/

var params = []MockParam{
	{"original", 10, mockData},
	{"original", 100, mockData},
	{"original", 1000, mockData},
	{"original", 10000, mockData},
}

func mockData(num int) []*protocol.Log {
	var Logs []*protocol.Log
	for i := 0; i < num; i++ {
		log := &protocol.Log{
			Time: 1234567890,
			Contents: []*protocol.Log_Content{
				{Key: "labelA", Value: "1"},
				{Key: "labelB", Value: "2"},
				{Key: "labelC", Value: "3"},
				{Key: "nameA", Value: "myname"},
				{Key: "valueA", Value: "1.0"},
				{Key: "nameB", Value: "myname2"},
				{Key: "valueB", Value: "2.0"},
				{Key: "timeKey", Value: "1658806869597190887"},
			},
		}
		Logs = append(Logs, log)
	}
	return Logs
}

// goos: linux
// goarch: amd64
// pkg: github.com/alibaba/ilogtail/plugins/processor/sls_metric
// cpu: Intel(R) Xeon(R) Platinum 8369B CPU @ 2.70GHz
// BenchmarkProcessorLogSlsMetricTest/original10-4              344119              3322 ns/op            1040 B/op         50 allocs/op
// BenchmarkProcessorLogSlsMetricTest/original100-4              37146             32294 ns/op           10400 B/op        500 allocs/op
// BenchmarkProcessorLogSlsMetricTest/original1000-4              3562            326816 ns/op          104000 B/op       5000 allocs/op
// BenchmarkProcessorLogSlsMetricTest/original10000-4              332           3606487 ns/op         1040007 B/op      50000 allocs/op
func BenchmarkProcessorLogSlsMetricTest(b *testing.B) {
	for _, param := range params {
		b.Run(param.name+strconv.Itoa(param.num), func(b *testing.B) {
			// generate mock data
			Logs := param.mockFunc(param.num)
			// Create a ProcessorLogToSlsMetric instance
			processor := pipeline.Processors["processor_log_to_sls_metric"]().(*ProcessorLogToSlsMetric)
			// Init ProcessorLogToSlsMetric
			processor.MetricLabelKeys = []string{}
			processor.Init(mock.NewEmptyContext("p", "l", "c"))

			// Report memory allocation
			b.ReportAllocs()
			// reset timer
			b.ResetTimer()
			// run the benchmark
			for i := 0; i < b.N; i++ {
				processor.ProcessLogs(Logs)
			}
			// stop timer
			b.StopTimer()
		})
	}
}
