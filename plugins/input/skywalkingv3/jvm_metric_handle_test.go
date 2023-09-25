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
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"testing"

	"github.com/alibaba/ilogtail/pkg/protocol"
	common "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/agent/v3"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestJvmMetrics(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	ctx.InitContext("a", "b", "c")
	collector := &test.MockCollector{}
	handler := &JVMMetricHandler{
		ctx,
		collector,
		5000,
		-1,
	}
	_, _ = handler.Collect(context.Background(), buildMockJvmMetricRequest())
	validate("./testdata/jvm_metrics.json", collector.RawLogs, t)
}

func validate(expectedResultPath string, result []*protocol.Log, t *testing.T) {
	jsonBytes, _ := json.MarshalIndent(result, "", "    ")
	fmt.Println(string(jsonBytes))
	expected, _ := os.ReadFile(filepath.Clean(expectedResultPath))
	temp := make([]*protocol.Log, 0, 16)
	json.Unmarshal(expected, &temp)
	expected, _ = json.MarshalIndent(temp, "", "    ")
	if !bytes.Equal(jsonBytes, expected) {
		t.Fail()
	}
}

func buildMockJvmMetricRequest() *v3.JVMMetricCollection {
	req := &v3.JVMMetricCollection{}
	req.ServiceInstance = "instance_1"
	req.Service = "service_1"
	req.Metrics = make([]*v3.JVMMetric, 0)

	memory := make([]*v3.Memory, 0)
	memory = append(memory, &v3.Memory{
		IsHeap:    false,
		Init:      1,
		Max:       9,
		Used:      7,
		Committed: 4,
	})
	memory = append(memory, &v3.Memory{
		IsHeap:    true,
		Init:      1,
		Max:       9,
		Used:      7,
		Committed: 4,
	})
	memoryPool := make([]*v3.MemoryPool, 0)
	memoryPool = append(memoryPool, &v3.MemoryPool{
		Type:      v3.PoolType_NEWGEN_USAGE,
		Init:      1,
		Max:       9,
		Used:      4,
		Committed: 7,
	})
	memoryPool = append(memoryPool, &v3.MemoryPool{
		Type:      v3.PoolType_OLDGEN_USAGE,
		Init:      1,
		Max:       9,
		Used:      4,
		Committed: 7,
	})
	gc := make([]*v3.GC, 0)
	gc = append(gc, &v3.GC{
		Phrase: v3.GCPhrase_NEW,
		Count:  12,
		Time:   123,
	})
	gc = append(gc, &v3.GC{
		Phrase: v3.GCPhrase_OLD,
		Count:  12,
		Time:   123,
	})
	req.Metrics = append(req.Metrics, &v3.JVMMetric{
		Time:       10000,
		Cpu:        &common.CPU{UsagePercent: 50},
		Memory:     memory,
		MemoryPool: memoryPool,
		Gc:         gc,
		Thread: &v3.Thread{
			LiveCount:   1,
			DaemonCount: 2,
			PeakCount:   3,
		},
	})
	return req
}
