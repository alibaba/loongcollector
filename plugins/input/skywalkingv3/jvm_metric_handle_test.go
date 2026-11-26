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
	"encoding/json"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	common "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/agent/v3"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestJvmMetrics(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	ctx.InitContext("a", "b", "c")
	collector := helper.NewObservePipelineContext(256).Collector()
	handler := &JVMMetricHandler{
		ctx,
		collector,
		5000,
		-1,
	}
	_, _ = handler.Collect(context.Background(), buildMockJvmMetricRequest())
	validate("./testdata/jvm_metrics.json", collector.ToArray(), t)
}

func validate(expectedResultPath string, groups []*models.PipelineGroupEvents, t *testing.T) {
	t.Helper()
	raw, err := os.ReadFile(filepath.Clean(expectedResultPath))
	require.NoError(t, err)
	var expected []struct {
		Time     uint32
		TimeNs   uint32 `json:"Time_ns"`
		Contents []struct {
			Key   string
			Value string
		}
	}
	require.NoError(t, json.Unmarshal(raw, &expected))
	require.Len(t, groups, len(expected))
	for i, group := range groups {
		require.Len(t, group.Events, 1)
		fields := make(map[string]string, len(expected[i].Contents))
		for _, field := range expected[i].Contents {
			fields[field.Key] = field.Value
		}
		if name := fields["__name__"]; name != "" {
			metric, ok := group.Events[0].(*models.Metric)
			require.True(t, ok, "event %d should be a metric", i)
			sample, err := strconv.ParseFloat(fields["__value__"], 64)
			require.NoError(t, err)
			timestamp, err := strconv.ParseUint(fields["__time_nano__"], 10, 64)
			require.NoError(t, err)
			tags := make(map[string]string)
			for _, label := range strings.Split(fields["__labels__"], "|") {
				if key, value, found := strings.Cut(label, "#$#"); found {
					tags[key] = value
				}
			}
			require.Equal(t, name, metric.GetName())
			require.Equal(t, models.MetricTypeUntyped, metric.GetMetricType())
			require.Equal(t, timestamp, metric.GetTimestamp())
			require.Equal(t, sample, metric.GetValue().GetSingleValue())
			require.Equal(t, tags, metric.GetTags().Iterator())
			continue
		}
		log, ok := group.Events[0].(*models.Log)
		require.True(t, ok, "event %d should be a log", i)
		require.Equal(t, uint64(expected[i].Time)*1e9+uint64(expected[i].TimeNs), log.GetTimestamp())
		got := make(map[string]string, log.GetIndices().Len())
		for key, value := range models.LogContentEntries(log.GetIndices()) {
			got[key] = pipeline.GetStringValue(value)
		}
		require.Equal(t, fields, got)
		require.Empty(t, log.GetTags().Iterator())
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
