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

package drop

import (
	"strconv"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newProcessor(idxs ...int) (*ProcessorDrop, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	var dropKeys []string
	for _, i := range idxs {
		k := "test_" + strconv.Itoa(i)
		dropKeys = append(dropKeys, k)
	}
	processor := &ProcessorDrop{
		DropKeys: dropKeys,
	}
	err := processor.Init(ctx)
	return processor, err
}

func getLog() *protocol.Log {
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "test_1", Value: "test_1"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "test_2", Value: "test_2"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "test_3", Value: "test_3"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "test_4", Value: "test_4"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "test_5", Value: "test_5"})
	return log
}

// drop all
func TestSourceKey(t *testing.T) {
	processor, err := newProcessor(1, 2, 3, 4, 5)
	require.NoError(t, err)
	log := getLog()
	processor.processLog(log)
	assert.Equal(t, 0, len(log.Contents))
}

// drop first two
func TestSourceKey1(t *testing.T) {
	processor, err := newProcessor(1, 2)
	require.NoError(t, err)
	log := getLog()
	processor.processLog(log)
	assert.Equal(t, "test_3", log.Contents[0].Value)
	assert.Equal(t, "test_4", log.Contents[1].Value)
	assert.Equal(t, "test_5", log.Contents[2].Value)
}

// drop middle two
func TestSourceKey2(t *testing.T) {
	processor, err := newProcessor(2, 3)
	require.NoError(t, err)
	log := getLog()
	processor.processLog(log)
	assert.Equal(t, "test_1", log.Contents[0].Value)
	assert.Equal(t, "test_4", log.Contents[1].Value)
	assert.Equal(t, "test_5", log.Contents[2].Value)
}

func TestSourceKey3(t *testing.T) {
	processor, err := newProcessor(1, 5)
	require.NoError(t, err)
	log := getLog()
	processor.processLog(log)
	assert.Equal(t, "test_2", log.Contents[0].Value)
	assert.Equal(t, "test_3", log.Contents[1].Value)
	assert.Equal(t, "test_4", log.Contents[2].Value)
}

func TestSourceKey4(t *testing.T) {
	processor, err := newProcessor(1, 3)
	require.NoError(t, err)
	log := getLog()
	processor.processLog(log)
	assert.Equal(t, "test_2", log.Contents[0].Value)
	assert.Equal(t, "test_4", log.Contents[1].Value)
	assert.Equal(t, "test_5", log.Contents[2].Value)
}

func TestSourceKey5(t *testing.T) {
	processor, err := newProcessor(3, 5)
	require.NoError(t, err)
	log := getLog()
	processor.processLog(log)
	assert.Equal(t, "test_1", log.Contents[0].Value)
	assert.Equal(t, "test_2", log.Contents[1].Value)
	assert.Equal(t, "test_4", log.Contents[2].Value)
}
