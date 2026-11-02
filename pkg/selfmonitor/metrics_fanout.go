// Copyright 2026 iLogtail Authors
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

package selfmonitor

// fanoutCounter increments multiple CounterMetric instances with a single Add.
// It is a write proxy only and must not be registered as a MetricCollector;
// each inner counter is already registered on its own MetricsRecord.
type fanoutCounter struct {
	inners []CounterMetric
}

// NewFanoutCounter returns a CounterMetric that forwards Add to all non-nil inners.
func NewFanoutCounter(inners ...CounterMetric) CounterMetric {
	filtered := make([]CounterMetric, 0, len(inners))
	for _, inner := range inners {
		if inner != nil {
			filtered = append(filtered, inner)
		}
	}
	return &fanoutCounter{inners: filtered}
}

func (f *fanoutCounter) Add(v int64) {
	for _, inner := range f.inners {
		inner.Add(v)
	}
}

func (f *fanoutCounter) Collect() MetricValue[float64] {
	if len(f.inners) == 0 {
		return MetricValue[float64]{}
	}
	return f.inners[0].Collect()
}

func (f *fanoutCounter) Export() map[string]string {
	if len(f.inners) == 0 {
		return nil
	}
	return f.inners[0].Export()
}

func (f *fanoutCounter) Type() SelfMetricType {
	return CounterType
}
