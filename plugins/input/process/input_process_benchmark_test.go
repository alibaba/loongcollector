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

//go:build linux
// +build linux

package process

import (
	"testing"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

// goos: linux
// goarch: amd64
// pkg: ilogtail/plugins/input/input_process_v2
// cpu: Intel(R) Xeon(R) CPU E5-2682 v4 @ 2.50GHz
// Benchmark_CollectDefault-64    	      22	  51563661 ns/op
func Benchmark_CollectDefault(b *testing.B) {
	cxt := mock.NewEmptyContext("project", "store", "config")
	p := ilogtail.MetricInputs["metric_process_v2"]().(*InputProcess)
	if _, err := p.Init(cxt); err != nil {
		b.Errorf("cannot init the mock process plugin: %v", err)
		return
	}
	c := &test.MockMetricCollector{}
	_ = p.Collect(c)
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		if err := p.Collect(c); err != nil {
			b.Errorf("error in collect process metrics: %v", err)
			return
		}
	}
}
