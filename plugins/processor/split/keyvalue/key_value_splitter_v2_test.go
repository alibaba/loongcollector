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

package kvsplitter

import (
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	pm "github.com/alibaba/ilogtail/pluginmanager"
)

func newSplitterV2(t *testing.T) *KeyValueSplitter {
	s := newKeyValueSplitter()
	s.KeepSource = true
	s.SourceKey = "content"
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	require.NoError(t, s.Init(ctx))
	return s
}

func TestProcessLogEventSplitV2(t *testing.T) {
	s := newSplitterV2(t)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("content", "class:main\tuserid:123456\tmethod:get\tkey with empty:xxxxx\tmessage:\"wrong user\"")

	s.processLogEvent(log)

	// Source is kept (KeepSource=true) plus 5 parsed pairs.
	require.Equal(t, "main", contents.Get("class"))
	require.Equal(t, "123456", contents.Get("userid"))
	require.Equal(t, "get", contents.Get("method"))
	require.Equal(t, "xxxxx", contents.Get("key with empty"))
	require.Equal(t, "\"wrong user\"", contents.Get("message"))
	require.True(t, contents.Contains("content"))
}

func TestProcessLogEventDropSourceV2(t *testing.T) {
	s := newSplitterV2(t)
	s.KeepSource = false

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("content", "a:1\tb:2")

	s.processLogEvent(log)

	require.False(t, contents.Contains("content"))
	require.Equal(t, "1", contents.Get("a"))
	require.Equal(t, "2", contents.Get("b"))
}

func TestProcessMetricPassThroughV2(t *testing.T) {
	s := newSplitterV2(t)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "a:1\tb:2")
	metric := models.NewMetric("cpu", models.MetricTypeCounter, models.NewTags(), 0, &models.MetricSingleValue{Value: 1}, nil)

	group := &models.PipelineGroupEvents{
		Events: []models.PipelineEvent{log, metric},
	}
	context := helper.NewObservePipelineContext(10)
	s.Process(group, context)

	results := context.Collector().ToArray()
	require.Equal(t, 1, len(results))
	require.Equal(t, 2, len(results[0].Events))
	// Metric event passes through unchanged (not dropped).
	require.Equal(t, models.EventTypeMetric, results[0].Events[1].GetType())
	// Log event was split.
	require.Equal(t, "1", log.GetIndices().Get("a"))
}
