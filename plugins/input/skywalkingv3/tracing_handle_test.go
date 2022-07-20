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

	commonv3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/agent/v3"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"github.com/stretchr/testify/require"
)

func TestTrace(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	ctx.InitContext("a", "b", "c")
	collector := &test.MockCollector{}
	resourcePropertiesCache := &ResourcePropertiesCache{
		cache:    make(map[string]map[string]string),
		cacheKey: ctx.GetConfigName() + "#" + CheckpointKey,
	}
	resourcePropertiesCache.load(ctx)
	handler := TracingHandler{ctx, collector, resourcePropertiesCache, make(map[int32]string)}
	_, _ = handler.CollectInSync(context.Background(), buildMockTraceRequest())
	require.Equal(t, resourcePropertiesCache.put("service-a", "service-instance-a", map[string]string{
		"a": "b",
	}), true)
	require.Equal(t, resourcePropertiesCache.put("service-a", "service-instance-a", map[string]string{
		"a": "b",
	}), false)
	require.Equal(t, resourcePropertiesCache.put("service-a", "service-instance-a", map[string]string{
		"a": "b",
		"c": "d",
	}), true)
	resourcePropertiesCache.save(ctx)
	resourcePropertiesCache.cache = make(map[string]map[string]string)
	require.Equal(t, resourcePropertiesCache.load(ctx), true)
	require.Equal(t, len(resourcePropertiesCache.cache), 1)
	_, _ = handler.CollectInSync(context.Background(), buildMockTraceRequest())
	validate("./testdata/trace.json", collector.RawLogs, t)
}

func buildMockTraceRequest() *v3.SegmentCollection {
	req := &v3.SegmentCollection{}
	var segs []*v3.SegmentObject
	segObject := &v3.SegmentObject{
		TraceId:         "trace-id-a",
		TraceSegmentId:  "trace-seg-id-a",
		Service:         "service-a",
		ServiceInstance: "service-instance-a",
	}
	segObject.Spans = append(segObject.Spans, &v3.SpanObject{
		SpanId:        123,
		ParentSpanId:  456,
		StartTime:     1234567890123,
		EndTime:       1234567890125,
		OperationName: "name",
		Peer:          "abc:1234",
		Tags: []*commonv3.KeyStringValuePair{
			{
				Key:   "key-a",
				Value: "value-a",
			},
			{
				Key:   "key-b",
				Value: "value-b",
			},
		},
	})
	segs = append(segs, segObject)
	req.Segments = segs
	return req
}
