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

package cloudmeta

import (
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/helper/platformmeta"
	_ "github.com/alibaba/ilogtail/pkg/logger/test"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

// Test_cloudMeta_Process_EnrichesGroupTags asserts the v2 semantics: cloud
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
	// The Log's own contents were NOT enriched with cloud meta in v2.
	require.False(t, logEvent.GetIndices().Contains("__instance_id__"))
	require.False(t, logEvent.GetIndices().Contains("__instance_name__"))
	require.True(t, logEvent.GetIndices().Contains("orig_key"))
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
	// Force the v1 no-op condition regardless of the test host.
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
