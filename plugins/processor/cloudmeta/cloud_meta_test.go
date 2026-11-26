// Copyright 2023 iLogtail Authors
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

package cloudmeta

import (
	"sync/atomic"
	"testing"
	"time"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/helper/platformmeta"
	_ "github.com/alibaba/ilogtail/pkg/logger/test"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestCloudMetaRefreshesGroupTags(t *testing.T) {
	c := new(ProcessorCloudMeta)
	c.Platform = platformmeta.Mock
	c.RenameMetadata = map[string]string{
		platformmeta.FlagInstanceID:         "__instance_id__",
		platformmeta.FlagInstanceName:       "__instance_name__",
		platformmeta.FlagInstanceZone:       "__zone__",
		platformmeta.FlagInstanceRegion:     "__region__",
		platformmeta.FlagInstanceType:       "__instance_type__",
		platformmeta.FlagInstanceVswitchID:  "__vswitch_id__",
		platformmeta.FlagInstanceVpcID:      "__vpc_id__",
		platformmeta.FlagInstanceImageID:    "__image_id__",
		platformmeta.FlagInstanceMaxIngress: "__max_ingress__",
		platformmeta.FlagInstanceMaxEgress:  "__max_egress__",
		platformmeta.FlagInstanceTags:       "__instance_tags__",
	}
	c.Metadata = []string{
		platformmeta.FlagInstanceID,
		platformmeta.FlagInstanceName,
		platformmeta.FlagInstanceRegion,
		platformmeta.FlagInstanceZone,
		platformmeta.FlagInstanceVpcID,
		platformmeta.FlagInstanceVswitchID,
		platformmeta.FlagInstanceTags,
		platformmeta.FlagInstanceType,
		platformmeta.FlagInstanceImageID,
		platformmeta.FlagInstanceMaxIngress,
		platformmeta.FlagInstanceMaxEgress,
	}
	require.NoError(t, c.Init(mock.NewEmptyContext("a", "b", "c")))

	log := models.NewSimpleLog(nil, models.NewTags(), 1)
	collectTags := func() models.Tags {
		ctx := helper.NewGroupedPipelineContext()
		c.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, ctx)
		groups := ctx.Collector().ToArray()
		require.Len(t, groups, 1)
		require.Equal(t, []models.PipelineEvent{log}, groups[0].Events)
		return groups[0].Group.GetTags()
	}
	t.Cleanup(func() { atomic.StoreInt64(&platformmeta.MockManagerNum, 0) })
	tags := collectTags()
	require.Equal(t, tags.Len(), 11)
	require.Equal(t, tags.Get("__instance_id__"), "id_xxx")
	require.Equal(t, tags.Get("__instance_name__"), "name_xxx")
	require.Equal(t, tags.Get("__zone__"), "zone_xxx")
	require.Equal(t, tags.Get("__region__"), "region_xxx")
	require.Equal(t, tags.Get("__instance_type__"), "type_xxx")
	require.Equal(t, tags.Get("__vswitch_id__"), "vswitch_xxx")
	require.Equal(t, tags.Get("__vpc_id__"), "vpc_xxx")
	require.Equal(t, tags.Get("__image_id__"), "image_xxx")
	require.Equal(t, tags.Get("__max_ingress__"), "0")
	require.Equal(t, tags.Get("__max_egress__"), "0")
	require.Equal(t, tags.Get("__instance_tags___tag_key"), "tag_val")

	atomic.AddInt64(&platformmeta.MockManagerNum, 100)
	c.lastReadTime = time.Now().Add(-time.Second)
	tags = collectTags()
	require.Equal(t, tags.Len(), 11)
	require.Equal(t, tags.Get("__instance_id__"), "id_xxx")
	require.Equal(t, tags.Get("__instance_name__"), "name_xxx")
	require.Equal(t, tags.Get("__zone__"), "zone_xxx")
	require.Equal(t, tags.Get("__region__"), "region_xxx")
	require.Equal(t, tags.Get("__instance_type__"), "type_xxx")
	require.Equal(t, tags.Get("__vswitch_id__"), "vswitch_xxx")
	require.Equal(t, tags.Get("__vpc_id__"), "vpc_xxx")
	require.Equal(t, tags.Get("__image_id__"), "image_xxx")
	require.Equal(t, tags.Get("__max_ingress__"), "100")
	require.Equal(t, tags.Get("__max_egress__"), "1000")
	require.Equal(t, tags.Get("__instance_tags___tag_key"), "tag_val")
	lastRead := c.lastReadTime
	atomic.AddInt64(&platformmeta.MockManagerNum, 100)
	tags = collectTags()
	require.Equal(t, tags.Get("__max_ingress__"), "100")
	require.Equal(t, tags.Get("__max_egress__"), "1000")
	require.Equal(t, c.lastReadTime, lastRead)
}

// Test_cloudMeta_Process_EnrichesGroupTags verifies that cloud
// metadata is added ONCE to the group-level Tags (not into each event), and all
// event types (Log AND Metric) pass through unchanged and are not dropped.
func Test_cloudMeta_Process_EnrichesGroupTags(t *testing.T) {
	c := new(ProcessorCloudMeta)
	c.Platform = platformmeta.Mock
	c.RenameMetadata = map[string]string{
		platformmeta.FlagInstanceID:   "__instance_id__",
		platformmeta.FlagInstanceName: "__instance_name__",
		platformmeta.FlagInstanceTags: "__instance_tags__",
	}
	c.Metadata = []string{
		platformmeta.FlagInstanceID,
		platformmeta.FlagInstanceName,
		platformmeta.FlagInstanceTags,
	}
	require.NoError(t, c.Init(mock.NewEmptyContext("a", "b", "c")))

	// Build a group carrying both a Log and a Metric event.
	logEvent := models.NewLog("test_log", []byte("body"), "", "", "", models.NewTags(), 0)
	logEvent.GetIndices().Add("orig_key", "orig_val")
	metricEvent := models.NewSingleValueMetric("test_metric", models.MetricTypeCounter, models.NewTags(), 0, 1)
	group := &models.PipelineGroupEvents{
		Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
		Events: []models.PipelineEvent{logEvent, metricEvent},
	}

	context := helper.NewObservePipelineContext(10)
	c.Process(group, context)

	results := context.Collector().ToArray()
	require.Equal(t, 1, len(results))
	collected := results[0]

	// Cloud meta appears in the GROUP tags (respecting RenameMetadata), not per-event.
	tags := collected.Group.GetTags()
	require.True(t, tags.Contains("__instance_id__"))
	require.Equal(t, "id_xxx", tags.Get("__instance_id__"))
	require.True(t, tags.Contains("__instance_name__"))
	require.Equal(t, "name_xxx", tags.Get("__instance_name__"))
	require.True(t, tags.Contains("__instance_tags___tag_key"))
	require.Equal(t, "tag_val", tags.Get("__instance_tags___tag_key"))

	// All events pass through unchanged and none are dropped.
	require.Equal(t, 2, len(collected.Events))
	require.Equal(t, logEvent, collected.Events[0])
	require.Equal(t, metricEvent, collected.Events[1])
	// Log contents are unchanged.
	require.False(t, logEvent.GetIndices().Contains("__instance_id__"))
	require.False(t, logEvent.GetIndices().Contains("__instance_name__"))
	require.True(t, logEvent.GetIndices().Contains("orig_key"))
}

// Test_cloudMeta_Process_InitializesUnwritableTags asserts that cloud metadata is
// still written when the input group has no writable Tags: a nil Group, a nil
// Tags field, or Tags set to the shared immutable NilStringValues sentinel. All
// three must end up carrying the cloud meta tags rather than silently dropping
// them into a no-op sentinel.
func Test_cloudMeta_Process_InitializesUnwritableTags(t *testing.T) {
	newProcessor := func(t *testing.T) *ProcessorCloudMeta {
		c := new(ProcessorCloudMeta)
		c.Platform = platformmeta.Mock
		c.RenameMetadata = map[string]string{
			platformmeta.FlagInstanceID: "__instance_id__",
		}
		c.Metadata = []string{platformmeta.FlagInstanceID}
		require.NoError(t, c.Init(mock.NewEmptyContext("a", "b", "c")))
		return c
	}

	tests := []struct {
		name  string
		group func() *models.GroupInfo
	}{
		{
			name:  "nil Group",
			group: func() *models.GroupInfo { return nil },
		},
		{
			name:  "nil Tags",
			group: func() *models.GroupInfo { return &models.GroupInfo{} },
		},
		{
			name:  "NilStringValues sentinel Tags",
			group: func() *models.GroupInfo { return &models.GroupInfo{Tags: models.NilStringValues} },
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			c := newProcessor(t)
			// A group needs at least one event to be collected downstream.
			logEvent := models.NewLog("test_log", []byte("body"), "", "", "", models.NewTags(), 0)
			group := &models.PipelineGroupEvents{
				Group:  tt.group(),
				Events: []models.PipelineEvent{logEvent},
			}
			context := helper.NewObservePipelineContext(10)
			c.Process(group, context)

			results := context.Collector().ToArray()
			require.Equal(t, 1, len(results))
			tags := results[0].Group.GetTags()
			require.True(t, tags.Contains("__instance_id__"))
			require.Equal(t, "id_xxx", tags.Get("__instance_id__"))
			// Event still passes through unchanged.
			require.Equal(t, 1, len(results[0].Events))
			require.Equal(t, logEvent, results[0].Events[0])
		})
	}
}

// Test_cloudMeta_Process_NoMetaPassThrough asserts the guard: when the platform
// meta provider is unavailable (no manager), group tags are left untouched and
// events still pass through.
func Test_cloudMeta_Process_NoMetaPassThrough(t *testing.T) {
	c := new(ProcessorCloudMeta)
	// Auto platform on a non-cloud host yields no manager, so meta stays empty.
	c.Platform = platformmeta.Auto
	c.Metadata = []string{platformmeta.FlagInstanceID}
	require.NoError(t, c.Init(mock.NewEmptyContext("a", "b", "c")))
	// Use a processor without a cloud metadata provider.
	c.manager = nil
	c.meta = make(map[string]string)

	logEvent := models.NewLog("test_log", []byte("body"), "", "", "", models.NewTags(), 0)
	group := &models.PipelineGroupEvents{
		Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
		Events: []models.PipelineEvent{logEvent},
	}

	context := helper.NewObservePipelineContext(10)
	c.Process(group, context)

	results := context.Collector().ToArray()
	require.Equal(t, 1, len(results))
	require.Equal(t, 0, results[0].Group.GetTags().Len())
	require.Equal(t, 1, len(results[0].Events))
	require.Equal(t, logEvent, results[0].Events[0])
}
