// Copyright 2024 iLogtail Authors
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
	"sort"
	"testing"

	pb "github.com/VictoriaMetrics/VictoriaMetrics/lib/prompbmarshal"
	"github.com/stretchr/testify/assert"
)

// 场景：Prometheus label names 字典序排序
// 因子：乱序的 Prometheus label names
// 预期：Prometheus label names 按字典序排序
func TestLexicographicalSort_ShouldSortedInLexicographicalOrder(t *testing.T) {
	// given
	labels := []pb.Label{
		{Name: "Tutorial", Value: "tutorial"},
		{Name: "Point", Value: "point"},
		{Name: "Java", Value: "java"},
		{Name: "C++", Value: "c++"},
		{Name: "Golang", Value: "golang"},
		{Name: metricNameKey, Value: "test_metric_name"},
	}
	ans := []pb.Label{
		{Name: "C++", Value: "c++"},
		{Name: "Golang", Value: "golang"},
		{Name: "Java", Value: "java"},
		{Name: "Point", Value: "point"},
		{Name: "Tutorial", Value: "tutorial"},
		{Name: metricNameKey, Value: "test_metric_name"},
	}
	assert.Equal(t, len(ans), len(labels))

	// when
	got := lexicographicalSort(labels)

	// then
	assert.Equal(t, ans, got)
}

// 场景：性能测试，确定 lexicographicalSort 字典序排序方法的性能
// 因子：利用 lexicographicalSort（底层基于sort.Sort()）对 Prometheus label names 进行字典序排序
// 预期：lexicographicalSort 和 sort.Strings 对 Prometheus label names 的字典序排序性能相当（数量级相同）
// goos: darwin
// goarch: arm64
// pkg: github.com/alibaba/ilogtail/pkg/protocol/encoder/prometheus
// BenchmarkLexicographicalSort
// BenchmarkLexicographicalSort/lexicographicalSort
// BenchmarkLexicographicalSort/lexicographicalSort-12         	23059904	        47.51 ns/op
// BenchmarkLexicographicalSort/sort.Strings
// BenchmarkLexicographicalSort/sort.Strings-12                	25321753	        47.30 ns/op
// PASS
func BenchmarkLexicographicalSort(b *testing.B) {
	prometheusLabels := []pb.Label{
		{Name: "Tutorial", Value: "tutorial"},
		{Name: "Point", Value: "point"},
		{Name: "Java", Value: "java"},
		{Name: "C++", Value: "c++"},
		{Name: "Golang", Value: "golang"},
		{Name: metricNameKey, Value: "test_metric_name"},
	}
	stringLabels := []string{
		"Tutorial",
		"Point",
		"Java",
		"C++",
		"Golang",
		metricNameKey,
	}

	b.Run("lexicographicalSort", func(b *testing.B) {
		for i := 0; i < b.N; i++ {
			lexicographicalSort(prometheusLabels)
		}
	})

	b.Run("sort.Strings", func(b *testing.B) {
		for i := 0; i < b.N; i++ {
			sort.Strings(stringLabels)
		}
	})
}
