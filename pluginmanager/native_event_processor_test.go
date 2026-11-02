// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package pluginmanager

import (
	"testing"
	"time"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

type nativeEventProcessor struct{ observed chan models.PipelineEvent }

func (*nativeEventProcessor) Init(pipeline.Context) error { return nil }
func (*nativeEventProcessor) Description() string         { return "native event processor regression" }
func (p *nativeEventProcessor) Process(in *models.PipelineGroupEvents, ctx pipeline.PipelineContext) {
	for _, event := range in.Events {
		p.observed <- event
	}
	ctx.Collector().CollectList(in)
}

func TestNativeMetricAndSpanReachGoProcessor(t *testing.T) {
	processor := &nativeEventProcessor{observed: make(chan models.PipelineEvent, 2)}
	const pluginName = "processor_native_event_test"
	pipeline.Processors[pluginName] = func() pipeline.Processor { return processor }
	t.Cleanup(func() { delete(pipeline.Processors, pluginName) })
	config, err := createLogstoreConfig("project", "store", "native-event", 0,
		`{"global":{"StructureType":"v2"},"processors":[{"type":"processor_native_event_test"}],"flushers":[{"type":"flusher_stdout"}]}`)
	require.NoError(t, err)
	config.PluginRunner.Run()
	defer func() { require.NoError(t, config.PluginRunner.Stop(false)) }()
	for _, event := range []models.PipelineEvent{
		models.NewSingleValueMetric("native_metric", models.MetricTypeCounter, models.NewTagsWithKeyValues("source", "native"), 123, 42),
		models.NewSpan("native_span", "trace", "span", models.SpanKindServer, 1, 2, models.NewTags(), nil, nil),
	} {
		group := models.NewGroup(models.NewMetadata(), models.NewTags())
		pb, err := helper.TransferPipelineEventGroupToPB(group, []models.PipelineEvent{event})
		require.NoError(t, err)
		data, err := pb.Marshal()
		require.NoError(t, err)
		require.Zero(t, config.ProcessPipelineEventGroup(data, "pack"))
		select {
		case received := <-processor.observed:
			require.Equal(t, event.GetType(), received.GetType())
			switch value := received.(type) {
			case *models.Metric:
				require.Equal(t, "native_metric", value.Name)
				require.Equal(t, float64(42), value.Value.GetSingleValue())
			case *models.Span:
				require.Equal(t, "trace", value.TraceID)
			}
		case <-time.After(3 * time.Second):
			t.Fatal("native event did not reach the Go processor")
		}
	}
}
