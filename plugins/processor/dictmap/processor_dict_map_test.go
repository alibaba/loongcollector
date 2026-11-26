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

package dictmap

import (
	"encoding/csv"
	"fmt"
	"os"
	"testing"
	"time"

	"github.com/alibaba/ilogtail/plugins/processor/internal/processortest"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newProcessor() (*ProcessorDictMap, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorDictMap{
		DictFilePath:  "testfile.csv",
		MapDict:       map[string]string{"1": "TCP", "2": "UDP", "3": "HTTP", "*": "Unknown"},
		SourceKey:     "_ip_",
		DestKey:       "",
		HandleMissing: true,
		Missing:       "Not Detected",
		Mode:          "overwrite",
		MaxDictSize:   100,
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestCsvParser(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	assert.Equal(t, processor.MapDict, map[string]string{"127.0.0.1": "LocalHost-LocalHost", "192.168.0.1": "default login"})
}

func TestEmptyCsv(t *testing.T) {
	nowTime := time.Now().Unix()
	testFile := fmt.Sprintf("testfile%v.csv", nowTime)
	f, err := os.Create(testFile)
	require.NoError(t, err)
	defer f.Close() //nolint:gosec

	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorDictMap{
		DictFilePath:  testFile,
		MapDict:       map[string]string{"1": "TCP", "2": "UDP", "3": "HTTP", "*": "Unknown"},
		SourceKey:     "_ip_",
		DestKey:       "",
		HandleMissing: true,
		Missing:       "Not Detected",
		Mode:          "overwrite",
		MaxDictSize:   100,
	}
	err = processor.Init(ctx)
	require.Error(t, err)

	err = os.Remove(testFile)
	require.NoError(t, err)
}

func TestIllegalCsv1(t *testing.T) {
	nowTime := time.Now().Unix()
	testFile := fmt.Sprintf("testfile%v.csv", nowTime)
	f, err := os.Create(testFile)
	require.NoError(t, err)
	defer f.Close() //nolint:gosec
	w := csv.NewWriter(f)
	data := [][]string{
		{"source", "dest", "append"},
	}
	_ = w.WriteAll(data)
	w.Flush()

	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorDictMap{
		DictFilePath:  testFile,
		MapDict:       map[string]string{"1": "TCP", "2": "UDP", "3": "HTTP", "*": "Unknown"},
		SourceKey:     "_ip_",
		DestKey:       "",
		HandleMissing: true,
		Missing:       "Not Detected",
		Mode:          "overwrite",
		MaxDictSize:   100,
	}
	err = processor.Init(ctx)
	require.Error(t, err)

	err = os.Remove(testFile)
	require.NoError(t, err)
}

func TestIllegalCsv2(t *testing.T) {
	nowTime := time.Now().Unix()
	testFile := fmt.Sprintf("testfile%v.csv", nowTime)
	f, err := os.Create(testFile)
	require.NoError(t, err)
	defer f.Close() //nolint:gosec
	w := csv.NewWriter(f)
	data := [][]string{
		{"source"},
	}
	_ = w.WriteAll(data)
	w.Flush()

	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorDictMap{
		DictFilePath:  testFile,
		MapDict:       map[string]string{"1": "TCP", "2": "UDP", "3": "HTTP", "*": "Unknown"},
		SourceKey:     "_ip_",
		DestKey:       "",
		HandleMissing: true,
		Missing:       "Not Detected",
		Mode:          "overwrite",
		MaxDictSize:   100,
	}
	err = processor.Init(ctx)
	require.Error(t, err)

	err = os.Remove(testFile)
	require.NoError(t, err)
}

func TestDictParser(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorDictMap{
		DictFilePath:  "",
		MapDict:       map[string]string{"1": "TCP", "2": "UDP", "3": "HTTP", "*": "Unknown"},
		SourceKey:     "_ip_",
		DestKey:       "_MapIp_",
		HandleMissing: true,
		Missing:       "Not Detected",
		Mode:          "overwrite",
		MaxDictSize:   100,
	}
	err := processor.Init(ctx)
	require.NoError(t, err)
	assert.Equal(t, processor.MapDict, map[string]string{"1": "TCP", "2": "UDP", "3": "HTTP", "*": "Unknown"})
}

func TestDestKey(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	log := processortest.Log()
	log.Contents.Add("_ip_", "127.0.0.1")
	processortest.Apply(processor, log)
	assert.True(t, log.Contents.Contains(processor.SourceKey))

	log = processortest.Log()
	log.Contents.Add("_ip_", "192.168.0.1")
	processortest.Apply(processor, log)
	assert.True(t, log.Contents.Contains(processor.DestKey))

	log = processortest.Log()
	log.Contents.Add("_ip_", "0.0.0.0")
	processortest.Apply(processor, log)
	assert.Equal(t, 1, log.Contents.Len())

	log = processortest.Log()
	log.Contents.Add("_Missing_ip_", "192.168.0.1")
	processortest.Apply(processor, log)
	assert.True(t, log.Contents.Contains(processor.DestKey))
}

func TestDestValue(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	log := processortest.Log()
	log.Contents.Add("_ip_", "127.0.0.1")
	processortest.Apply(processor, log)
	assert.Equal(t, processor.MapDict["127.0.0.1"], log.Contents.Get(processor.DestKey))

	log = processortest.Log()
	log.Contents.Add("_ip_", "192.168.0.1")
	processortest.Apply(processor, log)
	assert.Equal(t, processor.MapDict["192.168.0.1"], log.Contents.Get(processor.DestKey))

	log = processortest.Log()
	log.Contents.Add("_Missing_ip_", "192.168.0.1")
	processortest.Apply(processor, log)
	assert.Equal(t, processor.Missing, log.Contents.Get(processor.DestKey))
}

func TestOverWrite1(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorDictMap{
		DictFilePath:  "testfile.csv",
		MapDict:       map[string]string{"1": "TCP", "2": "UDP", "3": "HTTP", "*": "Unknown"},
		SourceKey:     "_ip_",
		DestKey:       "_ip_",
		HandleMissing: true,
		Missing:       "Not Detected",
		Mode:          "overwrite",
		MaxDictSize:   100,
	}
	err := processor.Init(ctx)

	require.NoError(t, err)
	log := processortest.Log()
	log.Contents.Add("_ip_", "127.0.0.1")
	processortest.Apply(processor, log)
	assert.Equal(t, processor.MapDict["127.0.0.1"], log.Contents.Get(processor.DestKey))

	log = processortest.Log()
	log.Contents.Add("_ip_", "192.168.0.1")
	log.Contents.Add("_another_ip_", "127.0.0.1")
	processortest.Apply(processor, log)
	assert.Equal(t, processor.MapDict["192.168.0.1"], log.Contents.Get(processor.DestKey))

	log = processortest.Log()
	log.Contents.Add("_Missing_ip_", "192.168.0.1")
	processortest.Apply(processor, log)
	assert.Equal(t, processor.Missing, log.Contents.Get(processor.DestKey))
}

func TestOverWrite2(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorDictMap{
		DictFilePath:  "testfile.csv",
		MapDict:       map[string]string{"1": "TCP", "2": "UDP", "3": "HTTP", "*": "Unknown"},
		SourceKey:     "_ip_",
		DestKey:       "_Newip_",
		HandleMissing: true,
		Missing:       "Not Detected",
		Mode:          "overwrite",
		MaxDictSize:   100,
	}
	err := processor.Init(ctx)

	require.NoError(t, err)
	log := processortest.Log()
	log.Contents.Add("_Newip_", "127.0.0.1")
	log.Contents.Add("_ip_", "127.0.0.1")
	processortest.Apply(processor, log)
	assert.Equal(t, processor.MapDict["127.0.0.1"], log.Contents.Get(processor.DestKey))

	log = processortest.Log()
	log.Contents.Add("_ip_", "192.168.0.1")
	log.Contents.Add("_Newip_", "127.0.0.1")
	processortest.Apply(processor, log)
	assert.Equal(t, processor.MapDict["192.168.0.1"], log.Contents.Get(processor.DestKey))

	log = processortest.Log()
	log.Contents.Add("_Missing_ip_", "192.168.0.1")
	processortest.Apply(processor, log)
	assert.Equal(t, processor.Missing, log.Contents.Get(processor.DestKey))
}

func TestFill(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorDictMap{
		DictFilePath:  "testfile.csv",
		MapDict:       map[string]string{"1": "TCP", "2": "UDP", "3": "HTTP", "*": "Unknown"},
		SourceKey:     "_ip_",
		DestKey:       "_Newip_",
		HandleMissing: true,
		Missing:       "Not Detected",
		Mode:          "fill",
		MaxDictSize:   100,
	}
	err := processor.Init(ctx)

	require.NoError(t, err)
	log := processortest.Log()
	log.Contents.Add("_Newip_", "127.0.0.1")
	log.Contents.Add("_ip_", "127.0.0.1")
	processortest.Apply(processor, log)
	assert.Equal(t, "127.0.0.1", log.Contents.Get(processor.DestKey))

	log = processortest.Log()
	log.Contents.Add("_ip_", "192.168.0.1")
	log.Contents.Add("_Newip_", "127.0.0.1")
	processortest.Apply(processor, log)
	assert.Equal(t, "127.0.0.1", log.Contents.Get(processor.DestKey))

	log = processortest.Log()
	log.Contents.Add("_Missing_ip_", "192.168.0.1")
	processortest.Apply(processor, log)
	assert.Equal(t, processor.Missing, log.Contents.Get(processor.DestKey))
}

func TestDontHandleMissing(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorDictMap{
		DictFilePath:  "testfile.csv",
		MapDict:       map[string]string{"1": "TCP", "2": "UDP", "3": "HTTP", "*": "Unknown"},
		SourceKey:     "_ip_",
		DestKey:       "_ip_",
		HandleMissing: false,
		Missing:       "Not Detected",
		Mode:          "fill",
		MaxDictSize:   100,
	}
	err := processor.Init(ctx)
	require.NoError(t, err)

	log := processortest.Log()
	log.Contents.Add("_Missing_ip_", "192.168.0.1")
	processortest.Apply(processor, log)
	assert.Equal(t, 1, log.Contents.Len())

	log = processortest.Log()
	log.Contents.Add("_Another_ip_", "0.0.0.0")
	processortest.Apply(processor, log)
	assert.Equal(t, 1, log.Contents.Len())
}

func newDictMap(t *testing.T, p *ProcessorDictMap) *ProcessorDictMap {
	if p.MaxDictSize == 0 {
		p.MaxDictSize = 1000
	}
	if p.Mode == "" {
		p.Mode = "overwrite"
	}
	require.NoError(t, p.Init(mock.NewEmptyContext("p", "l", "c")))
	return p
}

func TestProcessorDictMap_ProcessInPlace(t *testing.T) {
	processor := newDictMap(t, &ProcessorDictMap{SourceKey: "k", MapDict: map[string]string{"a": "b"}})

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("k", "a")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.Equal(t, "b", log.GetIndices().Get("k"))
}

func TestProcessorDictMap_ProcessToDestKey(t *testing.T) {
	processor := newDictMap(t, &ProcessorDictMap{SourceKey: "k", DestKey: "d", MapDict: map[string]string{"a": "b"}})

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("k", "a")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.Equal(t, "a", log.GetIndices().Get("k"), "source is preserved")
	assert.Equal(t, "b", log.GetIndices().Get("d"))
}

func TestProcessorDictMap_ProcessMissingHandled(t *testing.T) {
	processor := newDictMap(t, &ProcessorDictMap{SourceKey: "k", DestKey: "d", MapDict: map[string]string{"a": "b"}, HandleMissing: true, Missing: "Unknown"})

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("other", "x")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.Equal(t, "Unknown", log.GetIndices().Get("d"))
}

func TestProcessorDictMap_ProcessPassesThroughMetric(t *testing.T) {
	processor := newDictMap(t, &ProcessorDictMap{SourceKey: "k", MapDict: map[string]string{"a": "b"}})

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}
