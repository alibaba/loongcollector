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

package kvsplitter

import (
	"math/rand"
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/protocol"
	pm "github.com/alibaba/ilogtail/pluginmanager"
)

func searchPair(contents []*protocol.Log_Content, key string, value string) bool {
	for _, pair := range contents {
		if pair.Key == key && pair.Value == value {
			return true
		}
	}
	return false
}

func TestSplit(t *testing.T) {
	s := newKeyValueSplitter()
	s.KeepSource = true
	s.SourceKey = "content"
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	log := &protocol.Log{}
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   s.SourceKey,
		Value: "class:main\tuserid:123456\tmethod:get\tkey with empty:xxxxx\tmessage:\"wrong user\"",
	})
	logArray := []*protocol.Log{log}

	outLogArray := s.ProcessLogs(logArray)
	require.Equal(t, len(outLogArray), 1)
	outLog := logArray[0]
	require.Equalf(t, len(outLog.Contents), 6, "%v", outLog.Contents)
	contents := outLog.Contents
	expectedPairs := []struct {
		Key   string
		Value string
	}{
		{"class", "main"},
		{"userid", "123456"},
		{"method", "get"},
		{"key with empty", "xxxxx"},
		{"message", "\"wrong user\""},
	}
	for _, p := range expectedPairs {
		require.Truef(t, searchPair(contents, p.Key, p.Value), "%v:%v", p, contents)
	}
}

func TestSplitWithEmptyKey(t *testing.T) {
	s := newKeyValueSplitter()
	s.KeepSource = true
	s.SourceKey = "content"
	s.EmptyKeyPrefix = "MyEmptyPrefix_"
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	log := &protocol.Log{}
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   s.SourceKey,
		Value: "class:main\tuserid:123456\tmethod:get\t:empty again\tmessage:\"wrong user\"\t:100 empty key\n\nhello",
	})
	logArray := []*protocol.Log{log}

	outLogArray := s.ProcessLogs(logArray)
	require.Equal(t, len(outLogArray), 1)
	outLog := logArray[0]
	require.Equalf(t, len(outLog.Contents), 7, "%v", outLog.Contents)
	contents := outLog.Contents
	expectedPairs := []struct {
		Key   string
		Value string
	}{
		{"class", "main"},
		{"userid", "123456"},
		{"method", "get"},
		{"message", "\"wrong user\""},
		{s.EmptyKeyPrefix + "0", "empty again"},
		{s.EmptyKeyPrefix + "1", "100 empty key\n\nhello"},
	}
	for _, p := range expectedPairs {
		require.Truef(t, searchPair(contents, p.Key, p.Value), "%v:%v", p, contents)
	}
}

func TestSplitWithoutSeparator(t *testing.T) {
	s := newKeyValueSplitter()
	s.KeepSource = true
	s.SourceKey = "content"
	s.NoSeparatorKeyPrefix = "MySeparatorPrefix_"
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	log := &protocol.Log{}
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   s.SourceKey,
		Value: "class:main\tuserid:123456\tmethod:get\tmessage:\"wrong user\"\t100 empty key\n\nhello\tno separator again",
	})
	logArray := []*protocol.Log{log}

	outLogArray := s.ProcessLogs(logArray)
	require.Equal(t, len(outLogArray), 1)
	outLog := logArray[0]
	require.Equalf(t, len(outLog.Contents), 7, "%v", outLog.Contents)
	contents := outLog.Contents
	expectedPairs := []struct {
		Key   string
		Value string
	}{
		{"class", "main"},
		{"userid", "123456"},
		{"method", "get"},
		{"message", "\"wrong user\""},
		{s.NoSeparatorKeyPrefix + "0", "100 empty key\n\nhello"},
		{s.NoSeparatorKeyPrefix + "1", "no separator again"},
	}
	for _, p := range expectedPairs {
		require.Truef(t, searchPair(contents, p.Key, p.Value), "%v:%v", p, contents)
	}
}

func benchmarkSplit(b *testing.B, s *KeyValueSplitter, totalPartCount int, partLength int) {
	value := ""
	for countIdx := 0; countIdx < totalPartCount; countIdx++ {
		separatorPos := 1 + rand.Intn(partLength-len(s.Separator)-2) //nolint:gosec
		for valIdx := 0; valIdx < partLength; valIdx++ {
			if valIdx != separatorPos {
				value += "a"
			} else {
				value += s.Separator
			}
		}
		if countIdx != totalPartCount-1 {
			value += s.Delimiter
		}
	}

	b.ResetTimer()
	for loop := 0; loop < b.N; loop++ {
		s.ProcessLogs([]*protocol.Log{{
			Contents: []*protocol.Log_Content{
				{Key: s.SourceKey, Value: value},
			},
		}})
	}
}

// go test -bench=. -cpu=1 -benchtime=10s
// S: single delimiter.
// S: single separator.
// 10: 50 key value pairs.
// 30: each pair's length: key+separator+value.
func BenchmarkSplit_S_S_10_30(b *testing.B) {
	s := newKeyValueSplitter()
	s.KeepSource = true
	s.SourceKey = "content"
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkSplit(b, s, 10, 30)
}

func BenchmarkSplit_S_S_50_100(b *testing.B) {
	s := newKeyValueSplitter()
	s.KeepSource = true
	s.SourceKey = "content"
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkSplit(b, s, 50, 100)
}

func BenchmarkSplit_S_S_100_100(b *testing.B) {
	s := newKeyValueSplitter()
	s.KeepSource = true
	s.SourceKey = "content"
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkSplit(b, s, 100, 100)
}

func BenchmarkSplit_S_S_50_500(b *testing.B) {
	s := newKeyValueSplitter()
	s.KeepSource = true
	s.SourceKey = "content"
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkSplit(b, s, 50, 500)
}

func BenchmarkSplit_S_S_50_1000(b *testing.B) {
	s := newKeyValueSplitter()
	s.KeepSource = true
	s.SourceKey = "content"
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkSplit(b, s, 50, 1000)
}
