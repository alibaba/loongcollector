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

func TestIgnoreFirstConnector(t *testing.T) {
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

func TestExpandDepth(t *testing.T) {
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

func TestKeepSource(t *testing.T) {
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
	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	contents := log.GetIndices()
	contents.Add("key", jsonV)
	processor.processEvent(log)
	assert.Equal(t, 1, contents.Len())
	assert.True(t, contents.Contains("key"))
	assert.Equal(t, "value", contents.Get("key"))
}

func TestKeepSourceIfParseError(t *testing.T) {
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

func TestNoKeyError(t *testing.T) {
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

func TestExpandArray(t *testing.T) {
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
