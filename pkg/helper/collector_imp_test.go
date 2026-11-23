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
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
)

func TestGroupedPipelineCollectorDrainsWithoutRetainingEvents(t *testing.T) {
	collector := NewGroupedPipelineContext().Collector()
	info := &models.GroupInfo{}
	first := &models.Log{Name: "first"}
	second := &models.Log{Name: "second"}

	collector.Collect(info, first)
	drained := collector.ToArray()
	require.Len(t, drained, 1)
	require.Same(t, info, drained[0].Group)
	require.Equal(t, []models.PipelineEvent{first}, drained[0].Events)
	require.Empty(t, collector.ToArray())

	collector.Collect(info, second)
	next := collector.ToArray()
	require.Len(t, next, 1)
	require.Equal(t, []models.PipelineEvent{second}, next[0].Events)
	require.Equal(t, []models.PipelineEvent{first}, drained[0].Events)
}

func TestGroupedPipelineCollectorForwardAndMerge(t *testing.T) {
	collector := NewGroupedPipelineContext().Collector()
	info := &models.GroupInfo{}
	first := &models.Log{Name: "first"}
	second := &models.Log{Name: "second"}
	in := &models.PipelineGroupEvents{Group: info, Events: []models.PipelineEvent{first}}

	collector.CollectList(in)
	forwarded := collector.ToArray()
	require.Len(t, forwarded, 1)
	require.Same(t, in, forwarded[0])
	require.Empty(t, collector.ToArray())

	collector.CollectList(in)
	collector.Collect(info, second)
	merged := collector.ToArray()
	require.Len(t, merged, 1)
	require.Equal(t, []models.PipelineEvent{first, second}, merged[0].Events)
	require.Equal(t, []models.PipelineEvent{first}, in.Events)
}
