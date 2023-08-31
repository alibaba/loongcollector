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

package json

import (
	"strings"
	"testing"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func init() {
	logger.InitTestLogger(logger.OptionOpenMemoryReceiver)
}

func newProcessor() (*ProcessorJSON, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorJSON{
		SourceKey:              "s_key",
		NoKeyError:             true, // 目标Key，为空不生效
		ExpandDepth:            0,
		ExpandConnector:        "-",
		Prefix:                 "j",
		KeepSource:             true, // 是否保留源字段
		KeepSourceIfParseError: true,
		UseSourceKeyAsPrefix:   true,
	}
	err := processor.Init(ctx)
	return processor, err
}

var jsonVal = "{\"k1\":{\"k2\":{\"k3\":{\"k4\":{\"k51\":\"51\",\"k52\":\"52\"},\"k41\":\"41\"}}},\"k6\":[{\"x\":\"a\"},{\"x\":\"b\"}],\"k7\":[]}"

func TestSourceKey(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "s_key", Value: jsonVal})
	processor.processLog(log)
	assert.Equal(t, "js_key-k1-k2-k3-k4-k51", log.Contents[1].Key)
	assert.Equal(t, "51", log.Contents[1].Value)
	assert.Equal(t, "js_key-k1-k2-k3-k4-k52", log.Contents[2].Key)
	assert.Equal(t, "52", log.Contents[2].Value)
	assert.Equal(t, "js_key-k1-k2-k3-k41", log.Contents[3].Key)
	assert.Equal(t, "41", log.Contents[3].Value)
	assert.Equal(t, "js_key-k6", log.Contents[4].Key)
	assert.Equal(t, "[{\"x\":\"a\"},{\"x\":\"b\"}]", log.Contents[4].Value)
	assert.Equal(t, "js_key-k7", log.Contents[5].Key)
	assert.Equal(t, "[]", log.Contents[5].Value)
}

func TestIgnoreFirstConnector(t *testing.T) {
	processor, err := newProcessor()
	require.NotNil(t, processor)
	require.NoError(t, err)

	processor.IgnoreFirstConnector = true
	processor.UseSourceKeyAsPrefix = false
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "s_key", Value: jsonVal})
	processor.processLog(log)
	assert.Equal(t, "jk1-k2-k3-k4-k51", log.Contents[1].Key)
	assert.Equal(t, "51", log.Contents[1].Value)
	assert.Equal(t, "jk1-k2-k3-k4-k52", log.Contents[2].Key)
	assert.Equal(t, "52", log.Contents[2].Value)
	assert.Equal(t, "jk1-k2-k3-k41", log.Contents[3].Key)
	assert.Equal(t, "41", log.Contents[3].Value)
	assert.Equal(t, "jk6", log.Contents[4].Key)
	assert.Equal(t, "[{\"x\":\"a\"},{\"x\":\"b\"}]", log.Contents[4].Value)
	assert.Equal(t, "jk7", log.Contents[5].Key)
	assert.Equal(t, "[]", log.Contents[5].Value)
}

func TestExpandDepth(t *testing.T) {
	processor, err := newProcessor()
	if processor == nil {
		return
	}
	processor.ExpandDepth = 1
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "s_key", Value: jsonVal})
	processor.processLog(log)
	assert.Equal(t, "js_key-k1", log.Contents[1].Key)
	assert.Equal(t, "{\"k2\":{\"k3\":{\"k4\":{\"k51\":\"51\",\"k52\":\"52\"},\"k41\":\"41\"}}}", log.Contents[1].Value)
	assert.Equal(t, "js_key-k6", log.Contents[2].Key)
	assert.Equal(t, "[{\"x\":\"a\"},{\"x\":\"b\"}]", log.Contents[2].Value)
	assert.Equal(t, "js_key-k7", log.Contents[3].Key)
	assert.Equal(t, "[]", log.Contents[3].Value)
}

func TestKeepSource(t *testing.T) {
	processor, err := newProcessor()
	if processor == nil {
		return
	}
	processor.KeepSource = false
	processor.ExpandDepth = 1
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "s_key", Value: jsonVal})
	processor.processLog(log)
	assert.Equal(t, "js_key-k1", log.Contents[0].Key)
	assert.Equal(t, "{\"k2\":{\"k3\":{\"k4\":{\"k51\":\"51\",\"k52\":\"52\"},\"k41\":\"41\"}}}", log.Contents[0].Value)
	assert.Equal(t, "js_key-k6", log.Contents[1].Key)
	assert.Equal(t, "[{\"x\":\"a\"},{\"x\":\"b\"}]", log.Contents[1].Value)
	assert.Equal(t, "js_key-k7", log.Contents[2].Key)
	assert.Equal(t, "[]", log.Contents[2].Value)
}

func TestExpandKeySameToSource(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorJSON{
		SourceKey:              "key",
		NoKeyError:             true, // 目标Key，为空不生效
		ExpandDepth:            0,
		ExpandConnector:        "",
		Prefix:                 "",
		KeepSource:             false, // 是否保留源字段
		KeepSourceIfParseError: false,
		UseSourceKeyAsPrefix:   false,
	}
	assert.Nil(t, processor.Init(ctx))
	var jsonV = "{\"key\":\"value\"}"
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "key", Value: jsonV})
	processor.processLog(log)
	assert.Equal(t, 1, len(log.Contents))
	assert.Equal(t, "key", log.Contents[0].Key)
	assert.Equal(t, "value", log.Contents[0].Value)
}

func TestKeepSourceIfParseError(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	require.NotNil(t, processor)

	processor.KeepSource = false
	processor.ExpandDepth = 1

	// Case 1: Valid log, no source key in output log.
	{
		log := &protocol.Log{Time: 0}
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "s_key", Value: jsonVal})
		processor.processLog(log)
		require.Equal(t, 3, len(log.Contents))
		require.Equal(t, "js_key-k1", log.Contents[0].Key)
		require.Equal(t, "{\"k2\":{\"k3\":{\"k4\":{\"k51\":\"51\",\"k52\":\"52\"},\"k41\":\"41\"}}}",
			log.Contents[0].Value)
		require.Equal(t, "js_key-k6", log.Contents[1].Key)
		require.Equal(t, "[{\"x\":\"a\"},{\"x\":\"b\"}]", log.Contents[1].Value)
		require.Equal(t, "js_key-k7", log.Contents[2].Key)
		require.Equal(t, "[]", log.Contents[2].Value)
	}

	// Case 2: Invalid log, keep source key in output log.
	{
		const invalidValue = "hello"
		log := &protocol.Log{Time: 0}
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "s_key", Value: invalidValue})
		processor.processLog(log)
		require.Equal(t, 1, len(log.Contents))
		require.Equal(t, "s_key", log.Contents[0].Key)
		require.Equal(t, invalidValue, log.Contents[0].Value)
	}
}

func TestNoKeyError(t *testing.T) {
	logger.ClearMemoryLog()
	processor, err := newProcessor()
	if processor == nil {
		return
	}
	processor.KeepSource = false
	processor.ExpandDepth = 1
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "d_key", Value: jsonVal})
	processor.processLog(log)
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.True(t, strings.Contains(memoryLog, "PROCESSOR_JSON_FIND_ALARM\tcannot find key s_key"))
}

func TestExpandArray(t *testing.T) {
	processor, err := newProcessor()
	if processor == nil {
		return
	}
	processor.ExpandArray = true
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "s_key", Value: jsonVal})
	processor.processLog(log)
	assert.Equal(t, "js_key-k1-k2-k3-k4-k51", log.Contents[1].Key)
	assert.Equal(t, "51", log.Contents[1].Value)
	assert.Equal(t, "js_key-k1-k2-k3-k4-k52", log.Contents[2].Key)
	assert.Equal(t, "52", log.Contents[2].Value)
	assert.Equal(t, "js_key-k1-k2-k3-k41", log.Contents[3].Key)
	assert.Equal(t, "41", log.Contents[3].Value)
	assert.Equal(t, "js_key-k6[0]-x", log.Contents[4].Key)
	assert.Equal(t, "a", log.Contents[4].Value)
	assert.Equal(t, "js_key-k6[1]-x", log.Contents[5].Key)
	assert.Equal(t, "b", log.Contents[5].Value)
	assert.Equal(t, 6, len(log.Contents))
}

func TestSourceKeyV2(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("s_key", jsonVal)
	processor.processEvent(log)
	assert.True(t, contents.Contains("js_key-k1-k2-k3-k4-k51"))
	assert.Equal(t, "51", contents.Get("js_key-k1-k2-k3-k4-k51"))
	assert.True(t, contents.Contains("js_key-k1-k2-k3-k4-k52"))
	assert.Equal(t, "52", contents.Get("js_key-k1-k2-k3-k4-k52"))
	assert.True(t, contents.Contains("js_key-k1-k2-k3-k41"))
	assert.Equal(t, "41", contents.Get("js_key-k1-k2-k3-k41"))
	assert.True(t, contents.Contains("js_key-k6"))
	assert.Equal(t, "[{\"x\":\"a\"},{\"x\":\"b\"}]", contents.Get("js_key-k6"))
	assert.True(t, contents.Contains("js_key-k7"))
	assert.Equal(t, "[]", contents.Get("js_key-k7"))
}

func TestIgnoreFirstConnectorV2(t *testing.T) {
	processor, err := newProcessor()
	require.NotNil(t, processor)
	require.NoError(t, err)

	processor.IgnoreFirstConnector = true
	processor.UseSourceKeyAsPrefix = false
	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("s_key", jsonVal)
	processor.processEvent(log)
	assert.True(t, contents.Contains("jk1-k2-k3-k4-k51"))
	assert.Equal(t, "51", contents.Get("jk1-k2-k3-k4-k51"))
	assert.True(t, contents.Contains("jk1-k2-k3-k4-k52"))
	assert.Equal(t, "52", contents.Get("jk1-k2-k3-k4-k52"))
	assert.True(t, contents.Contains("jk1-k2-k3-k41"))
	assert.Equal(t, "41", contents.Get("jk1-k2-k3-k41"))
	assert.True(t, contents.Contains("jk6"))
	assert.Equal(t, "[{\"x\":\"a\"},{\"x\":\"b\"}]", contents.Get("jk6"))
	assert.True(t, contents.Contains("jk7"))
	assert.Equal(t, "[]", contents.Get("jk7"))
}

func TestExpandDepthV2(t *testing.T) {
	processor, err := newProcessor()
	if processor == nil {
		return
	}
	processor.ExpandDepth = 1
	require.NoError(t, err)
	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("s_key", jsonVal)
	processor.processEvent(log)
	assert.True(t, contents.Contains("js_key-k1"))
	assert.Equal(t, "{\"k2\":{\"k3\":{\"k4\":{\"k51\":\"51\",\"k52\":\"52\"},\"k41\":\"41\"}}}", contents.Get("js_key-k1"))
	assert.True(t, contents.Contains("js_key-k6"))
	assert.Equal(t, "[{\"x\":\"a\"},{\"x\":\"b\"}]", contents.Get("js_key-k6"))
	assert.True(t, contents.Contains("js_key-k7"))
	assert.Equal(t, "[]", contents.Get("js_key-k7"))
}

func TestKeepSourceV2(t *testing.T) {
	processor, err := newProcessor()
	if processor == nil {
		return
	}
	processor.KeepSource = false
	processor.ExpandDepth = 1
	require.NoError(t, err)
	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("s_key", jsonVal)
	processor.processEvent(log)
	assert.True(t, contents.Contains("js_key-k1"))
	assert.Equal(t, "{\"k2\":{\"k3\":{\"k4\":{\"k51\":\"51\",\"k52\":\"52\"},\"k41\":\"41\"}}}", contents.Get("js_key-k1"))
	assert.True(t, contents.Contains("js_key-k6"))
	assert.Equal(t, "[{\"x\":\"a\"},{\"x\":\"b\"}]", contents.Get("js_key-k6"))
	assert.True(t, contents.Contains("js_key-k7"))
	assert.Equal(t, "[]", contents.Get("js_key-k7"))
}

func TestExpandKeySameToSourceV2(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorJSON{
		SourceKey:              "key",
		NoKeyError:             true, // 目标Key，为空不生效
		ExpandDepth:            0,
		ExpandConnector:        "",
		Prefix:                 "",
		KeepSource:             false, // 是否保留源字段
		KeepSourceIfParseError: false,
		UseSourceKeyAsPrefix:   false,
	}
	assert.Nil(t, processor.Init(ctx))
	var jsonV = "{\"key\":\"value\"}"
	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("key", jsonV)
	processor.processEvent(log)
	assert.Equal(t, 1, contents.Len())
	assert.True(t, contents.Contains("key"))
	assert.Equal(t, "value", contents.Get("key"))
}

func TestKeepSourceIfParseErrorV2(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	require.NotNil(t, processor)

	processor.KeepSource = false
	processor.ExpandDepth = 1

	// Case 1: Valid log, no source key in output log.
	{
		log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
		contents := log.GetIndices()
		contents.Add("s_key", jsonVal)
		processor.processEvent(log)
		assert.Equal(t, 3, contents.Len())
		assert.True(t, contents.Contains("js_key-k1"))
		assert.True(t, contents.Contains("js_key-k6"))
		assert.True(t, contents.Contains("js_key-k7"))
		assert.Equal(t, "{\"k2\":{\"k3\":{\"k4\":{\"k51\":\"51\",\"k52\":\"52\"},\"k41\":\"41\"}}}", contents.Get("js_key-k1"))
		assert.Equal(t, "[{\"x\":\"a\"},{\"x\":\"b\"}]", contents.Get("js_key-k6"))
		assert.Equal(t, "[]", contents.Get("js_key-k7"))
	}

	// Case 2: Invalid log, keep source key in output log.
	{
		const invalidValue = "hello"
		log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
		contents := log.GetIndices()
		contents.Add("s_key", invalidValue)
		processor.processEvent(log)
		assert.Equal(t, 1, contents.Len())
		assert.True(t, contents.Contains("s_key"))
		assert.Equal(t, invalidValue, contents.Get("s_key"))
	}
}

func TestNoKeyErrorV2(t *testing.T) {
	logger.ClearMemoryLog()
	processor, err := newProcessor()
	if processor == nil {
		return
	}
	processor.KeepSource = false
	processor.ExpandDepth = 1
	require.NoError(t, err)
	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("d_key", jsonVal)
	processor.processEvent(log)
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.True(t, strings.Contains(memoryLog, "PROCESSOR_JSON_FIND_ALARM\tcannot find key s_key"))
}

func TestExpandArrayV2(t *testing.T) {
	processor, err := newProcessor()
	processor.ExpandArray = true
	require.NoError(t, err)
	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("s_key", jsonVal)
	processor.processEvent(log)
	assert.True(t, contents.Contains("js_key-k1-k2-k3-k4-k51"))
	assert.Equal(t, "51", contents.Get("js_key-k1-k2-k3-k4-k51"))
	assert.True(t, contents.Contains("js_key-k1-k2-k3-k4-k52"))
	assert.Equal(t, "52", contents.Get("js_key-k1-k2-k3-k4-k52"))
	assert.True(t, contents.Contains("js_key-k1-k2-k3-k41"))
	assert.Equal(t, "41", contents.Get("js_key-k1-k2-k3-k41"))
	assert.True(t, contents.Contains("js_key-k6[0]-x"))
	assert.Equal(t, "a", contents.Get("js_key-k6[0]-x"))
	assert.True(t, contents.Contains("js_key-k6[1]-x"))
	assert.Equal(t, "b", contents.Get("js_key-k6[1]-x"))
	assert.False(t, contents.Contains("js_key-k7"))
}
