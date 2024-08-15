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

package rename

import (
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func init() {
	logger.InitTestLogger(logger.OptionOpenMemoryReceiver)
}

func TestInit(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "c", "l")
	processor := &ProcessorRename{
		SourceKeys: []string{
			"a",
			"b",
		},
		DestKeys: []string{
			"c",
		},
		NoKeyError: true,
	}
	err := processor.Init(ctx)
	require.Error(t, err)
}

func newProcessor() (*ProcessorRename, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorRename{
		SourceKeys: []string{
			"a",
			"b",
			"same_key",
		},
		DestKeys: []string{
			"c",
			"d",
			"same_key",
		},
		NoKeyError: true,
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestSourceKey(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "same_key", Value: "test_value"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "a", Value: "test_value"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "b", Value: "test_value"})
	processor.processLog(log)
	assert.Equal(t, "same_key", log.Contents[0].Key)
	assert.Equal(t, "c", log.Contents[1].Key)
	assert.Equal(t, "d", log.Contents[2].Key)
}

func TestNoKeyError(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "a", Value: "test_value"})
	processor.processLog(log)
	assert.Equal(t, "c", log.Contents[0].Key)
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.True(t, strings.Contains(memoryLog, "RENAME_FIND_ALARM\tcannot find key [b]"), "got: %s", memoryLog)
}

func TestProcessLogEvent(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	// construct test log
	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("same_key", "test_value")
	contents.Add("a", "test_value")
	contents.Add("b", "test_value")
	// run test function
	processor.processLogEvent(log)
	// test assertions
	assert.True(t, contents.Contains("same_key"))
	assert.Equal(t, "test_value", contents.Get("same_key"))
	assert.True(t, contents.Contains("c"))
	assert.Equal(t, "test_value", contents.Get("c"))
	assert.True(t, contents.Contains("d"))
	assert.Equal(t, "test_value", contents.Get("d"))
}

func TestProcessMetricEvent(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	// construct test log
	log := models.NewMetric("", models.MetricTypeCounter, models.NewTags(), 0, &models.MetricSingleValue{}, nil)
	tags := log.GetTags()
	tags.Add("same_key", "test_value")
	tags.Add("a", "test_value")
	tags.Add("b", "test_value")
	// run test function
	processor.processOtherEvent(log)
	// test assertions
	assert.True(t, tags.Contains("same_key"))
	assert.Equal(t, "test_value", tags.Get("same_key"))
	assert.True(t, tags.Contains("c"))
	assert.Equal(t, "test_value", tags.Get("c"))
	assert.True(t, tags.Contains("d"))
	assert.Equal(t, "test_value", tags.Get("d"))
}

func TestNoKeyErrorV2(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	// construct test log
	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("a", "test_value")
	// construct test event group
	logs := &models.PipelineGroupEvents{
		Events: []models.PipelineEvent{log},
	}
	// construct test context
	context := helper.NewObservePipelineConext(10)
	// run test function
	processor.Process(logs, context)
	// collect test results
	context.Collector().CollectList(logs)
	// test assertions
	assert.True(t, contents.Contains("c"))
	assert.Equal(t, "test_value", contents.Get("c"))
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.True(t, strings.Contains(memoryLog, "RENAME_FIND_ALARM\tcannot find key [b]"), "got: %s", memoryLog)
}
