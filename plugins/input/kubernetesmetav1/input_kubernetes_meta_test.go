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

package kubernetesmeta

import (
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"testing"
	"time"
)

// Although the benchmark test maybe influenced by the network and cluster size, we also cloud
// find 32% CPU times used in json serialization and 51% CPU times used in Collect method.
// Also, about 43% memory allocs comes from the helper.makeMetaLog method on the memory side.
// So the core mem and CPU costs comes from the json serialization.

// goos: darwin
// goarch: amd64
// pkg: ilogtail/plugins/input/input_kubernets_meta
// cpu: Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz
// BenchmarkInputKubernetesMeta_Collect-12              493           2352996 ns/op         1111542 B/op      12629 allocs/op

func BenchmarkInputKubernetesMeta_Collect(b *testing.B) {
	cxt := mock.NewEmptyContext("project", "store", "config")
	p := pipeline.MetricInputs[pluginType]().(*InputKubernetesMeta)
	p.KubeConfigPath = "default"
	if _, err := p.Init(cxt); err != nil {
		b.Errorf("cannot init the mock process plugin: %v", err)
		return
	}
	p.informerFactory.Start(p.informerStopChan)
	c := &test.MockMetricCollector{Benchmark: true}
	defer func() {
		p.informerStopChan <- struct{}{}
	}()
	time.Sleep(time.Second * 5)
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		p.Collect(c)
	}
}
