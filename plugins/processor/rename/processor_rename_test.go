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
	"github.com/alibaba/ilogtail/pkg/logger"
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

func newProcessor() (*ProcessorRename, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorRename{
		SourceKeys: []string{
			"a",
			"b",
			"test_key",
		},
		DestKeys: []string{
			"c",
			"d",
			"e",
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
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "test_key", Value: "test_value"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "a", Value: "test_value"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "b", Value: "test_value"})
	processor.processLog(log)
	assert.Equal(t, "e", log.Contents[0].Key)
	assert.Equal(t, "c", log.Contents[1].Key)
	assert.Equal(t, "d", log.Contents[2].Key)
}

func TestNoKeyError(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "a", Value: "test_value"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "b", Value: "test_value"})
	processor.processLog(log)
	assert.Equal(t, "c", log.Contents[0].Key)
	assert.Equal(t, "d", log.Contents[1].Key)
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.True(t, strings.Contains(memoryLog, "RENAME_FIND_ALARM\tcannot find key [test_key]"), "got: %s", memoryLog)
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
