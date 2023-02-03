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

package decoding

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

func newProcessor() (*ProcessorBase64Decoding, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorBase64Decoding{
		SourceKey: "src",
		NewKey:    "dest",
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestSourceKey(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	value := "MTIz"
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "src", Value: value})
	processor.ProcessLogs([]*protocol.Log{log})
	assert.Equal(t, "MTIz", log.Contents[0].Value)
	assert.Equal(t, "123", log.Contents[1].Value)
}

func TestDecodeError(t *testing.T) {
	logger.ClearMemoryLog()
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorBase64Decoding{
		SourceKey:   "src",
		NewKey:      "dest",
		DecodeError: true,
	}
	err := processor.Init(ctx)
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "src", Value: "test_value"})
	processor.ProcessLogs([]*protocol.Log{log})
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.Equal(t, 1, logger.GetMemoryLogCount())
	assert.True(t, strings.Contains(memoryLog, "AlarmType:BASE64_D_ALARM\tdecode base64 error"))
}

func TestNoKeyError(t *testing.T) {
	logger.ClearMemoryLog()
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorBase64Decoding{
		SourceKey:  "src",
		NewKey:     "dest",
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
	assert.True(t, strings.Contains(memoryLog, "AlarmType:BASE64_D_FIND_ALARM\tcannot find key:src"))
}

func TestDescription(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	assert.Equal(t, processor.Description(), "base64 decoding processor for logtail")
}

func TestInit(t *testing.T) {
	p := pipeline.Processors["processor_base64_decoding"]()
	assert.Equal(t, reflect.TypeOf(p).String(), "*decoding.ProcessorBase64Decoding")
}
