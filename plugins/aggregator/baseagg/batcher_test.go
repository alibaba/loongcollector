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

package baseagg

import (
	"strings"
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestBaseAggregatorEmitsOnEventAndByteLimits(t *testing.T) {
	for _, tt := range []struct {
		name      string
		maxEvents int
		field     string
	}{
		{name: "count", maxEvents: 1, field: "payload"},
		{name: "bytes", maxEvents: MaxLogCount, field: strings.Repeat("a", MaxLogGroupSize/2+1)},
	} {
		t.Run(tt.name, func(t *testing.T) {
			aggregator := NewAggregatorBase()
			aggregator.MaxLogGroupCount = 1
			aggregator.MaxLogCount = tt.maxEvents
			aggregator.Topic = "telemetry"
			aggregator.PackFlag = false
			_, err := aggregator.Init(mock.NewEmptyContext("project", "store", "config"))
			require.NoError(t, err)
			ctx := helper.NewObservePipelineContext(10)
			first := models.NewSimpleLog(nil, models.NewTags(), 100)
			second := models.NewSimpleLog(nil, models.NewTags(), 200)
			for _, event := range []*models.Log{first, second} {
				event.Contents.Add("non_body_field", tt.field)
			}
			info := models.NewGroup(nil, models.NewTagsWithKeyValues("host", "node"))
			require.NoError(t, aggregator.Record(&models.PipelineGroupEvents{Group: info, Events: []models.PipelineEvent{first, second}}, ctx))
			groups := ctx.Collector().ToArray()
			require.Len(t, groups, 1)
			require.Equal(t, []models.PipelineEvent{first}, groups[0].Events)
			require.Equal(t, "telemetry", groups[0].Group.GetTags().Get(pipeline.TopicTagKey))
			require.False(t, groups[0].Group.GetTags().Contains(util.PackIDTagKey))
			require.Equal(t, "node", groups[0].Group.GetTags().Get("host"))
			require.NoError(t, aggregator.GetResult(ctx))
			groups = ctx.Collector().ToArray()
			require.Len(t, groups, 1)
			require.Equal(t, []models.PipelineEvent{second}, groups[0].Events)
			require.False(t, info.GetTags().Contains(pipeline.TopicTagKey))
		})
	}
}
