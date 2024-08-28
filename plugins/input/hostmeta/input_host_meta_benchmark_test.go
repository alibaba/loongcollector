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

package hostmeta

import (
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"testing"
)

// goos: linux
// goarch: amd64
// pkg: ilogtail/plugins/input/input_node_meta
// cpu: Intel(R) Xeon(R) CPU E5-2682 v4 @ 2.50GHz
// Benchmark_CollectNoProcess-64      	     314	   3775773 ns/op
func Benchmark_CollectNoProcess(b *testing.B) {
	cxt := mock.NewEmptyContext("project", "store", "config")
	p := pipeline.MetricInputs[pluginType]().(*InputNodeMeta)
	p.CPU = true
	p.Memory = true
	p.Net = true
	p.Disk = true
	p.Process = false
	if _, err := p.Init(cxt); err != nil {
		b.Errorf("cannot init the mock process plugin: %v", err)
		return
	}
	c := &test.MockMetricCollector{}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		if err := p.Collect(c); err != nil {
			b.Errorf("error in collect process metrics: %v", err)
			return
		}
	}
}

// goos: linux
// goarch: amd64
// pkg: ilogtail/plugins/input/input_node_meta
// cpu: Intel(R) Xeon(R) CPU E5-2682 v4 @ 2.50GHz
// Benchmark_CollectWithProcess-64    	     249	   4781412 ns/op
func Benchmark_CollectWithProcess(b *testing.B) {
	cxt := mock.NewEmptyContext("project", "store", "config")
	p := pipeline.MetricInputs[pluginType]().(*InputNodeMeta)
	p.CPU = true
	p.Memory = true
	p.Net = true
	p.Disk = true
	p.Process = true
	if _, err := p.Init(cxt); err != nil {
		b.Errorf("cannot init the mock process plugin: %v", err)
		return
	}
	c := &test.MockMetricCollector{}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		if err := p.Collect(c); err != nil {
			b.Errorf("error in collect process metrics: %v", err)
			return
		}
	}
}
