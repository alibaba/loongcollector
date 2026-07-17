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

package md5

import (
	"crypto/md5" //nolint:gosec
	"fmt"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestProcessorMD5_ProcessV2ComputesMD5(t *testing.T) {
	processor := &ProcessorMD5{SourceKey: "src", MD5Key: "src_md5"}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("src", "hello")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	want := fmt.Sprintf("%x", md5.Sum([]byte("hello"))) //nolint:gosec
	assert.True(t, log.GetIndices().Contains("src_md5"))
	assert.Equal(t, want, log.GetIndices().Get("src_md5"))
}

func TestProcessorMD5_ProcessV2MissingKeyNoOp(t *testing.T) {
	processor := &ProcessorMD5{SourceKey: "src", MD5Key: "src_md5"}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("other", "v")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.False(t, log.GetIndices().Contains("src_md5"), "no MD5 key should be added when source key is absent")
}

// TestProcessorMD5_ProcessV2PassesThroughSpan verifies Span events pass through.
func TestProcessorMD5_ProcessV2PassesThroughSpan(t *testing.T) {
	processor := &ProcessorMD5{SourceKey: "src", MD5Key: "src_md5"}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	span := &models.Span{Name: "s", Tags: models.NewTags()}
	group := &models.PipelineGroupEvents{Events: []models.PipelineEvent{span}}

	context := helper.NewObservePipelineContext(10)
	processor.Process(group, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Span)
	assert.True(t, ok, "span event must pass through unchanged")
}
