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

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/protocol"
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

	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_ip_", Value: "127.0.0.1"})
	processor.processLog(log)
	assert.Equal(t, processor.SourceKey, log.Contents[0].Key)

	log = &protocol.Log{Time: 1}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_ip_", Value: "192.168.0.1"})
	processor.processLog(log)
	assert.Equal(t, processor.DestKey, log.Contents[0].Key)

	log = &protocol.Log{Time: 2}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_ip_", Value: "0.0.0.0"})
	processor.processLog(log)
	assert.Equal(t, 1, len(log.Contents))

	log = &protocol.Log{Time: 3}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_Missing_ip_", Value: "192.168.0.1"})
	processor.processLog(log)
	assert.Equal(t, processor.DestKey, log.Contents[1].Key)
}

func TestDestValue(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_ip_", Value: "127.0.0.1"})
	processor.processLog(log)
	assert.Equal(t, processor.MapDict["127.0.0.1"], log.Contents[0].Value)

	log = &protocol.Log{Time: 1}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_ip_", Value: "192.168.0.1"})
	processor.processLog(log)
	assert.Equal(t, processor.MapDict["192.168.0.1"], log.Contents[0].Value)

	log = &protocol.Log{Time: 2}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_Missing_ip_", Value: "192.168.0.1"})
	processor.processLog(log)
	assert.Equal(t, processor.Missing, log.Contents[1].Value)
}

func TestMultipleInput(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)

	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_ip_", Value: "127.0.0.1"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_ip_", Value: "192.168.0.1"})
	processor.processLog(log)
	assert.Equal(t, processor.MapDict["127.0.0.1"], log.Contents[0].Value)

	log = &protocol.Log{Time: 1}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_ip_", Value: "127.0.0.1"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_ip_", Value: "192.168.0.1"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_ip_", Value: "0.0.0.0"})
	processor.processLog(log)
	assert.Equal(t, processor.MapDict["127.0.0.1"], log.Contents[0].Value)

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
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_ip_", Value: "127.0.0.1"})
	processor.processLog(log)
	assert.Equal(t, processor.MapDict["127.0.0.1"], log.Contents[0].Value)

	log = &protocol.Log{Time: 1}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_ip_", Value: "192.168.0.1"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_another_ip_", Value: "127.0.0.1"})
	processor.processLog(log)
	assert.Equal(t, processor.MapDict["192.168.0.1"], log.Contents[0].Value)

	log = &protocol.Log{Time: 2}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_Missing_ip_", Value: "192.168.0.1"})
	processor.processLog(log)
	assert.Equal(t, processor.Missing, log.Contents[1].Value)
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
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_Newip_", Value: "127.0.0.1"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_ip_", Value: "127.0.0.1"})
	processor.processLog(log)
	assert.Equal(t, processor.MapDict["127.0.0.1"], log.Contents[0].Value)

	log = &protocol.Log{Time: 1}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_ip_", Value: "192.168.0.1"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_Newip_", Value: "127.0.0.1"})
	processor.processLog(log)
	assert.Equal(t, processor.MapDict["192.168.0.1"], log.Contents[1].Value)

	log = &protocol.Log{Time: 2}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_Missing_ip_", Value: "192.168.0.1"})
	processor.processLog(log)
	assert.Equal(t, processor.Missing, log.Contents[1].Value)
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
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_Newip_", Value: "127.0.0.1"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_ip_", Value: "127.0.0.1"})
	processor.processLog(log)
	assert.Equal(t, "127.0.0.1", log.Contents[0].Value)

	log = &protocol.Log{Time: 1}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_ip_", Value: "192.168.0.1"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_Newip_", Value: "127.0.0.1"})
	processor.processLog(log)
	assert.Equal(t, "127.0.0.1", log.Contents[1].Value)

	log = &protocol.Log{Time: 2}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_Missing_ip_", Value: "192.168.0.1"})
	processor.processLog(log)
	assert.Equal(t, processor.Missing, log.Contents[1].Value)
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

	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_Missing_ip_", Value: "192.168.0.1"})
	processor.processLog(log)
	assert.Equal(t, 1, len(log.Contents))

	log = &protocol.Log{Time: 1}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "_Another_ip_", Value: "0.0.0.0"})
	processor.processLog(log)
	assert.Equal(t, 1, len(log.Contents))
}
