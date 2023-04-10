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

func TestSplitWithMultiCharacterDelimiter(t *testing.T) {
	s := newKeyValueSplitter()
	s.KeepSource = true
	s.SourceKey = "content"
	s.NoSeparatorKeyPrefix = "MySeparatorPrefix_"
	s.Delimiter = "#?#"
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	log := &protocol.Log{}
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   s.SourceKey,
		Value: "class:main#?#userid:123456#?#method:get#?#message:\"wrong user\"#?#100 empty key\n\nhello#?#no separator again",
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

func TestSplitWithChineseCharacter(t *testing.T) {
	s := newKeyValueSplitter()
	s.KeepSource = true
	s.SourceKey = "中文content"
	s.NoSeparatorKeyPrefix = "MySeparatorPrefix_"
	s.Delimiter = "#D中文分隔#"
	s.Separator = "#S中文分隔#"
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	log := &protocol.Log{}
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   s.SourceKey,
		Value: "class#S中文分隔#main#D中文分隔#userid#S中文分隔#123456#D中文分隔#method#S中文分隔#get#D中文分隔#message#S中文分隔#\"wrong user\"#D中文分隔#100 没有key\n\nhello#D中文分隔#没有分隔符again",
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
		{s.NoSeparatorKeyPrefix + "0", "100 没有key\n\nhello"},
		{s.NoSeparatorKeyPrefix + "1", "没有分隔符again"},
	}
	for _, p := range expectedPairs {
		require.Truef(t, searchPair(contents, p.Key, p.Value), "%v:%v", p, contents)
	}
}

func buildQValue(value string, quote string) string {
	return quote + value + quote
}

func TestSplitWithQuote(t *testing.T) {
	s := newKeyValueSplitter()
	s.KeepSource = true
	s.SourceKey = "content"
	s.NoSeparatorKeyPrefix = "MySeparatorPrefix_"
	s.Delimiter = " "
	Quotes := []string{"\"", "\"\"", "\"\"\""}
	for _, quote := range Quotes {
		s.Quote = quote
		ctx := &pm.ContextImp{}
		ctx.InitContext("test", "test", "test")
		_ = s.Init(ctx)
		log := &protocol.Log{}
		log.Contents = append(log.Contents, &protocol.Log_Content{
			Key: s.SourceKey,
			Value: "a1:" + buildQValue("b \\\" c", quote) +
				" a2:" + buildQValue("b \\\" \\\" c \\\\", quote) +
				" class:main userid:123456" +
				" half_quote:\" " +
				buildQValue("多  分  隔 ", quote) +
				" method:get " + buildQValue("中文", quote) +
				" chinesekey:" + buildQValue("中文", quote) +
				" " + buildQValue("", quote) +
				" nullval:" + buildQValue("", quote) +
				" http_user_agent:" + buildQValue("User Agent", quote) +
				" message:" + buildQValue("wrong user", quote) +
				" 100 empty key\n\nhello " +
				buildQValue("no separator again", quote) + " \"123",
		})
		logArray := []*protocol.Log{log}

		outLogArray := s.ProcessLogs(logArray)
		require.Equal(t, len(outLogArray), 1)
		outLog := logArray[0]
		require.Equalf(t, len(outLog.Contents), 19, "%v", outLog.Contents)
		contents := outLog.Contents
		expectedPairs := []struct {
			Key   string
			Value string
		}{
			{"a1", "b \\\" c"},
			{"a2", "b \\\" \\\" c \\\\"},
			{"class", "main"},
			{"userid", "123456"},
			{"method", "get"},
			{"http_user_agent", "User Agent"},
			{"message", "wrong user"},
			{"nullval", ""},
			{"chinesekey", "中文"},
			{"half_quote", "\""},
			{s.NoSeparatorKeyPrefix + "0", "多  分  隔 "},
			{s.NoSeparatorKeyPrefix + "1", "中文"},
			{s.NoSeparatorKeyPrefix + "2", ""},
			{s.NoSeparatorKeyPrefix + "3", "100"},
			{s.NoSeparatorKeyPrefix + "4", "empty"},
			{s.NoSeparatorKeyPrefix + "5", "key\n\nhello"},
			{s.NoSeparatorKeyPrefix + "6", "no separator again"},
			{s.NoSeparatorKeyPrefix + "7", "\"123"},
		}
		for _, p := range expectedPairs {
			require.Truef(t, searchPair(contents, p.Key, p.Value), "%v:%v", p, contents)
		}
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
// split-range 940720              1133 ns/op             760 B/op         17 allocs/op
// slice-index 1331194              855.5 ns/op           600 B/op         16 allocs/op
func BenchmarkSplit_S_S_10_30(b *testing.B) {
	s := newKeyValueSplitter()
	s.KeepSource = true
	s.SourceKey = "content"
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkSplit(b, s, 10, 30)
}

// split-range 252682              4816 ns/op            3544 B/op         59 allocs/op
// slice-index 264661              4090 ns/op            2648 B/op         58 allocs/op
func BenchmarkSplit_S_S_50_100(b *testing.B) {
	s := newKeyValueSplitter()
	s.KeepSource = true
	s.SourceKey = "content"
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkSplit(b, s, 50, 100)
}

// split-range 105076              9728 ns/op            7064 B/op        110 allocs/op
// slice-index 150987              7647 ns/op            5272 B/op        109 allocs/op
func BenchmarkSplit_S_S_100_100(b *testing.B) {
	s := newKeyValueSplitter()
	s.KeepSource = true
	s.SourceKey = "content"
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkSplit(b, s, 100, 100)
}

// split-range 239517	      	   5873 ns/op	    	 3544 B/op	       59 allocs/op
// slice-index 253881              4824 ns/op            2648 B/op         58 allocs/op
func BenchmarkSplit_S_S_50_500(b *testing.B) {
	s := newKeyValueSplitter()
	s.KeepSource = true
	s.SourceKey = "content"
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkSplit(b, s, 50, 500)
}

// split-range 172878              6370 ns/op            3544 B/op         59 allocs/op
// slice-index 194558              5837 ns/op            2648 B/op         58 allocs/op
func BenchmarkSplit_S_S_50_1000(b *testing.B) {
	s := newKeyValueSplitter()
	s.KeepSource = true
	s.SourceKey = "content"
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkSplit(b, s, 50, 1000)
}
