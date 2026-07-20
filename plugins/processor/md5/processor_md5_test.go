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

package md5

import (
	"crypto/md5" //nolint:gosec
	"fmt"
	"reflect"
	"strings"
	"testing"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func init() {
	logger.InitTestLogger(logger.OptionOpenMemoryReceiver)
}

func newProcessor() (*ProcessorMD5, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorMD5{
		SourceKey: "src",
		MD5Key:    "dest",
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestSourceKey(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	value := "123"
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "src", Value: value})
	processor.ProcessLogs([]*protocol.Log{log})
	assert.Equal(t, "123", log.Contents[0].Value)
	assert.Equal(t, "202cb962ac59075b964b07152d234b70", log.Contents[1].Value)
}

func TestNoKeyError(t *testing.T) {
	logger.ClearMemoryLog()
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorMD5{
		SourceKey:  "src",
		MD5Key:     "dest",
		NoKeyError: true,
	}
	err := processor.Init(ctx)
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "test_key", Value: "test_value"})
	processor.ProcessLogs([]*protocol.Log{log})
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.Equal(t, 1, logger.GetMemoryLogCount())
	assert.True(t, strings.Contains(memoryLog, "AlarmType:MD5_FIND_ALARM\tcannot find key:src"))
}

func TestDescription(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	assert.Equal(t, processor.Description(), "md5 processor for logtail")
}

func TestInit(t *testing.T) {
	p := pipeline.Processors["processor_md5"]()
	assert.Equal(t, reflect.TypeOf(p).String(), "*md5.ProcessorMD5")
}

// ---- v2 (PipelineEvent / SendPb) Process path tests ----

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
