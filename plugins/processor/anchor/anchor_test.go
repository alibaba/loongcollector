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

package anchor

import (
	"reflect"
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func init() {
	logger.InitTestLogger(logger.OptionOpenMemoryReceiver)
}

func newProcessor() (*ProcessorAnchor, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAnchor{
		Anchors: []Anchor{
			{
				Start:      "time:",
				Stop:       "\\t",
				FieldName:  "time",
				FieldType:  "string",
				ExpondJSON: false,
			},
			{
				Start:      "level:",
				Stop:       "\\t",
				FieldName:  "level",
				ExpondJSON: false,
			},
			{
				Start:      "json:",
				Stop:       "\\t",
				FieldName:  "val",
				FieldType:  "json",
				ExpondJSON: true,
			},
			{
				Start:           "json2:",
				Stop:            "",
				FieldName:       "val2",
				FieldType:       "json",
				ExpondJSON:      true,
				ExpondConnecter: "-",
				MaxExpondDepth:  1,
			},
			{
				Start:      "json3:",
				Stop:       "",
				FieldName:  "val3",
				FieldType:  "json",
				ExpondJSON: true,
			},
		},
		SourceKey:  "content",
		NoKeyError: true,
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestSourceKey(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	value := "time:2017.09.12 20:55:36" +
		"\\tlevel:info" +
		"\\tjson:{\"key1\" :\"xx\", \"key2\": false, \"key3\":123.456, \"key4\" : { \"inner1\" : 1, \"inner2\" : false}}" +
		"\\tjson2:{\"key\" : { \"inner1\" : 1, \"inner2\" : false}}"
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: value})
	processor.ProcessLogs([]*protocol.Log{log})
	assert.Equal(t, "time", log.Contents[0].Key)
	assert.Equal(t, "level", log.Contents[1].Key)
	assert.Equal(t, "val_key1", log.Contents[2].Key)
	assert.Equal(t, "val_key2", log.Contents[3].Key)
	assert.Equal(t, "val_key3", log.Contents[4].Key)
	assert.Equal(t, "val_key4_inner1", log.Contents[5].Key)
	assert.Equal(t, "val_key4_inner2", log.Contents[6].Key)
	assert.Equal(t, "val2-key", log.Contents[7].Key)

	assert.Equal(t, "2017.09.12 20:55:36", log.Contents[0].Value)
	assert.Equal(t, "info", log.Contents[1].Value)
	assert.Equal(t, "xx", log.Contents[2].Value)
	assert.Equal(t, "false", log.Contents[3].Value)
	assert.Equal(t, "123.456", log.Contents[4].Value)
	assert.Equal(t, "1", log.Contents[5].Value)
	assert.Equal(t, "false", log.Contents[6].Value)
	assert.Equal(t, "{ \"inner1\" : 1, \"inner2\" : false}", log.Contents[7].Value)
}

func TestDescription(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	assert.Equal(t, processor.Description(), "anchor processor for logtail")
}

func TestNoKeyError(t *testing.T) {
	logger.ClearMemoryLog()
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAnchor{
		Anchors: []Anchor{
			{
				Start:     "NoKeyError:",
				Stop:      "",
				FieldName: "NoKeyError",
			},
		},
		SourceKey:  "content",
		NoKeyError: true,
	}
	err := processor.Init(ctx)
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "test_key", Value: "test_value"})
	processor.ProcessLog(log)
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.Equal(t, 1, logger.GetMemoryLogCount())
	assert.True(t, strings.Contains(memoryLog, "AlarmType:ANCHOR_FIND_ALARM\tanchor cannot find key:content"))
}

func TestNoAnchorErrorNoStart(t *testing.T) {
	logger.ClearMemoryLog()
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAnchor{
		Anchors: []Anchor{
			{
				Start:           "666",
				Stop:            "",
				FieldName:       "test",
				FieldType:       "json",
				ExpondJSON:      true,
				IgnoreJSONError: false,
			},
		},
		SourceKey:     "content",
		NoKeyError:    true,
		NoAnchorError: true,
	}
	err := processor.Init(ctx)
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: "json:{\"key\" : { \"inner1\" : 1, \"inner2\" : false}}"})
	processor.ProcessLog(log)
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.Equal(t, 1, logger.GetMemoryLogCount())
	assert.True(t, strings.Contains(memoryLog, "AlarmType:ANCHOR_FIND_ALARM\tanchor no start"))
}

func TestNoAnchorErrorNoStop(t *testing.T) {
	logger.ClearMemoryLog()
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAnchor{
		Anchors: []Anchor{
			{
				Start:           "json:",
				Stop:            "888",
				FieldName:       "test",
				FieldType:       "json",
				ExpondJSON:      true,
				IgnoreJSONError: false,
			},
		},
		SourceKey:     "content",
		NoKeyError:    true,
		NoAnchorError: true,
	}
	err := processor.Init(ctx)
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: "json:{\"key\" : { \"inner1\" : 1, \"inner2\" : false}}"})
	processor.ProcessLog(log)
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.Equal(t, 1, logger.GetMemoryLogCount())
	assert.True(t, strings.Contains(memoryLog, "AlarmType:ANCHOR_FIND_ALARM\tanchor no stop"))
}

func TestIgnoreJSONError(t *testing.T) {
	logger.ClearMemoryLog()
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAnchor{
		Anchors: []Anchor{
			{
				Start:           "json:",
				Stop:            "",
				FieldName:       "test",
				FieldType:       "json",
				ExpondJSON:      true,
				IgnoreJSONError: false,
			},
		},
		SourceKey:  "content",
		NoKeyError: true,
	}
	err := processor.Init(ctx)
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: "json:{\"key\" : { \"inner1\" : 1, \"inner2\" : false}"})
	processor.ProcessLog(log)
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.Equal(t, 1, logger.GetMemoryLogCount())
	assert.True(t, strings.Contains(memoryLog, "AlarmType:ANCHOR_JSON_ALARM\tprocess json error"))
}

func TestInit(t *testing.T) {
	p := pipeline.Processors["processor_anchor"]()
	assert.Equal(t, reflect.TypeOf(p).String(), "*anchor.ProcessorAnchor")
}
