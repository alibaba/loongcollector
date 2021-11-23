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

package packjson

import (
	"strings"
	"testing"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func init() {
	logger.InitTestLogger(logger.OptionOpenMemoryReceiver)
}

func newProcessor() (*ProcessorPackjson, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorPackjson{
		SourceKeys:        []string{"a", "b"},
		DestKey:           "d_key", // 目标Key，为空不生效
		KeepSource:        true,    // 是否保留源字段
		AlarmIfIncomplete: true,
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestSourceKey(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "a", Value: "1"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "b", Value: "2"})
	processor.processLog(log)
	assert.Equal(t, "d_key", log.Contents[2].Key)
	assert.Equal(t, "{\"a\":\"1\",\"b\":\"2\"}", log.Contents[2].Value)
}

func TestKeepSource(t *testing.T) {
	processor, err := newProcessor()
	if processor == nil {
		return
	}
	processor.KeepSource = false
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "a", Value: "1"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "b", Value: "2"})
	processor.processLog(log)
	assert.Equal(t, "d_key", log.Contents[0].Key)
	assert.Equal(t, "{\"a\":\"1\",\"b\":\"2\"}", log.Contents[0].Value)
}

func TestAlarmIfEmpty(t *testing.T) {
	logger.ClearMemoryLog()
	processor, err := newProcessor()
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "a", Value: "1"})
	processor.processLog(log)
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.Truef(t, strings.Contains(memoryLog, "PACK_JSON_ALARM\tSourceKeys not found [b]"), "got %s", memoryLog)
}
