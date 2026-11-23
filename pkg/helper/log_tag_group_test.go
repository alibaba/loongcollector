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

package helper

import (
	"errors"
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
)

func TestForEachLogTagGroup(t *testing.T) {
	for _, tagged := range []bool{false, true} {
		group := &models.PipelineGroupEvents{Group: models.NewGroup(models.NewMetadataWithKeyValues("source", "pack"), models.NewTagsWithKeyValues("shared", "group"))}
		for _, value := range []string{"a", "a", "b", "a"} {
			tags := models.NewTags()
			if tagged {
				tags.Add("container", value)
				tags.Add("shared", "event")
			}
			group.Events = append(group.Events, models.NewSimpleLog([]byte(value), tags, 1))
		}
		var groups []*models.PipelineGroupEvents
		require.NoError(t, ForEachLogTagGroup(group, func(out *models.PipelineGroupEvents) error { groups = append(groups, out); return nil }))
		if !tagged {
			require.Len(t, groups, 1)
			require.Same(t, group, groups[0])
			continue
		}
		require.Len(t, groups, 3)
		var events []models.PipelineEvent
		for i, out := range groups {
			require.Equal(t, []string{"a", "b", "a"}[i], out.Group.Tags.Get("container"))
			require.Equal(t, "group", out.Group.Tags.Get("shared"))
			require.Same(t, group.Group.Metadata, out.Group.Metadata)
			events = append(events, out.Events...)
		}
		require.Equal(t, group.Events, events)
		require.False(t, group.Group.Tags.Contains("container"))
		require.Equal(t, "event", group.Events[0].GetTags().Get("shared"))
	}
}

func TestForEachLogTagGroupStopsOnError(t *testing.T) {
	expected := errors.New("send failure")
	calls := 0
	visit := func(*models.PipelineGroupEvents) error { calls++; return expected }
	require.NoError(t, ForEachLogTagGroup(nil, visit))
	require.NoError(t, ForEachLogTagGroup(&models.PipelineGroupEvents{}, visit))
	group := &models.PipelineGroupEvents{Events: []models.PipelineEvent{models.NewSimpleLog(nil, models.NewTagsWithKeyValues("tag", "a"), 1), models.NewSimpleLog(nil, models.NewTagsWithKeyValues("tag", "b"), 1)}}
	require.ErrorIs(t, ForEachLogTagGroup(group, visit), expected)
	require.Equal(t, 1, calls)
}

func TestForEachLogTagGroupDoesNotPromoteMetricTags(t *testing.T) {
	log := models.NewSimpleLog(nil, models.NewTagsWithKeyValues("container", "a"), 1)
	metric := models.NewSingleValueMetric("cpu", models.MetricTypeGauge, models.NewTagsWithKeyValues("city", "hz"), 1, 1)
	group := &models.PipelineGroupEvents{Group: models.NewGroup(models.NewMetadata(), models.NewTags()), Events: []models.PipelineEvent{log, metric}}
	var groups []*models.PipelineGroupEvents
	require.NoError(t, ForEachLogTagGroup(group, func(out *models.PipelineGroupEvents) error { groups = append(groups, out); return nil }))
	require.Len(t, groups, 2)
	require.Equal(t, "a", groups[0].Group.Tags.Get("container"))
	require.Zero(t, groups[1].Group.Tags.Len())
	require.Equal(t, "hz", groups[1].Events[0].GetTags().Get("city"))
}

func BenchmarkForEachLogTagGroup(b *testing.B) {
	for _, mode := range []string{"untagged", "same-tags", "different-tags"} {
		b.Run(mode, func(b *testing.B) {
			group := &models.PipelineGroupEvents{Group: models.NewGroup(models.NewMetadata(), models.NewTags())}
			for i := 0; i < 100; i++ {
				tags := models.NewTags()
				if mode != "untagged" {
					value := "a"
					if mode == "different-tags" && i%2 == 1 {
						value = "b"
					}
					tags.Add("container", value)
				}
				group.Events = append(group.Events, models.NewSimpleLog(nil, tags, 1))
			}
			visit := func(*models.PipelineGroupEvents) error { return nil }
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				if err := ForEachLogTagGroup(group, visit); err != nil {
					b.Fatal(err)
				}
			}
		})
	}
}
