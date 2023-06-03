// Copyright 2023 iLogtail Authors
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

package stringreplace

import (
	"testing"

	. "github.com/smartystreets/goconvey/convey"

	"github.com/alibaba/ilogtail/pkg/protocol"
	pm "github.com/alibaba/ilogtail/pluginmanager"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestInitError(t *testing.T) {
	Convey("Init error.", t, func() {
		Convey("Regex error", func() {
			processor := &ProcessorStringReplace{
				SourceKey:     "content",
				Method:        MethodRegex,
				Match:         "{\\",
				ReplaceString: "",
			}
			err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldNotBeNil)
		})

		Convey("No SourceKey error", func() {
			processor := &ProcessorStringReplace{
				Method: MethodRegex,
				Match:  "{}",
			}
			err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldNotBeNil)
			So(processor.ReplaceString, ShouldEqual, "")
		})

		Convey("No Method error", func() {
			processor := &ProcessorStringReplace{}
			err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldNotBeNil)
			So(processor.ReplaceString, ShouldEqual, "")
		})

		Convey("No Match error", func() {
			processor := &ProcessorStringReplace{
				Method: MethodConst,
			}
			err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldNotBeNil)
			So(processor.ReplaceString, ShouldEqual, "")
		})
	})

}

func TestProcessorStringReplaceWork(t *testing.T) {
	Convey("Test Match = regex.", t, func() {
		processor := &ProcessorStringReplace{
			SourceKey:     "content",
			Method:        MethodRegex,
			Match:         "\\\\u\\w+\\[\\d{1,3};*\\d{1,3}m|N/A",
			ReplaceString: "",
		}
		err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("Test regex1", func() {
			record := `2022-09-16 09:03:31.013 \u001b[32mINFO \u001b[0;39m \u001b[34m[TID: N/A]\u001b[0;39m [\u001b[35mThread-30\u001b[0;39m] \u001b[36mc.s.govern.polygonsync.job.BlockTask\u001b[0;39m : 区块采集------结束------\r`
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			logs := []*protocol.Log{}
			logs = append(logs, log)
			logs = processor.ProcessLogs(logs)
			So(len(logs[0].Contents), ShouldEqual, 1)
			So(logs[0].Contents[0].Key, ShouldEqual, `content`)
			So(logs[0].Contents[0].Value, ShouldEqual, `2022-09-16 09:03:31.013 INFO  [TID: ] [Thread-30] c.s.govern.polygonsync.job.BlockTask : 区块采集------结束------\r`)
		})

		processor = &ProcessorStringReplace{
			SourceKey:     "ip",
			Method:        MethodRegex,
			Match:         "(\\d.*\\.)\\d+",
			ReplaceString: "$1*/24",
		}
		err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("Test regex2 Group", func() {
			ipRecord := `10.10.239.16`
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "ip", Value: ipRecord})
			logs := []*protocol.Log{}
			logs = append(logs, log)
			logs = processor.ProcessLogs(logs)
			So(len(logs[0].Contents), ShouldEqual, 1)
			So(logs[0].Contents[0].Key, ShouldEqual, `ip`)
			So(logs[0].Contents[0].Value, ShouldEqual, `10.10.239.*/24`)
		})

		processor = &ProcessorStringReplace{
			SourceKey:     "ip",
			Method:        MethodRegex,
			Match:         ".\\d ",
			ReplaceString: "0/24 ",
		}
		err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("Test regex3 Group", func() {
			ipRecord := `10.10.239.16 10.10.238.10 `
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "ip", Value: ipRecord})
			logs := []*protocol.Log{}
			logs = append(logs, log)
			logs = processor.ProcessLogs(logs)
			So(len(logs[0].Contents), ShouldEqual, 1)
			So(logs[0].Contents[0].Key, ShouldEqual, `ip`)
			So(logs[0].Contents[0].Value, ShouldEqual, `10.10.239.0/24 10.10.238.0/24 `)
		})

		processor = &ProcessorStringReplace{
			SourceKey:     "attribute",
			Method:        MethodRegex,
			Match:         "(?<!(\\d|\\w))(13[0-9]|14[5-9]|15[0-35-9]|16[25-7]|17[0-8]|18[0-9]|19[0135689])(\\d{4})(\\d{4})(?!(\\d|\\w))",
			ReplaceString: "$2****$4",
		}
		err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("Test regex4 Group(match fail)", func() {
			ipRecord := "{\"http.method\":\"GET\",\"http.status_code\":\"200\",\"http.url\":\"http://\",\"requestClientIp\":\"0.0.0.0\",\"requestHeader\":\"Accept=[text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7]\\nAuth=null\\nHost=[]\",\"requestParams\":\"{\\\"userId\\\":\\\"666666\\\"}\",\"responseBody\":\"{\\\"msg\\\":\\\"OK\\\",\\\"code\\\":0,\\\"data\\\":[],\\\"success\\\":true,\\\"error\\\":false}\"}"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "attribute", Value: ipRecord})
			logs := []*protocol.Log{}
			logs = append(logs, log)
			logs = processor.ProcessLogs(logs)
			So(len(logs[0].Contents), ShouldEqual, 1)
			So(logs[0].Contents[0].Key, ShouldEqual, `attribute`)
			So(logs[0].Contents[0].Value, ShouldEqual, "{\"http.method\":\"GET\",\"http.status_code\":\"200\",\"http.url\":\"http://\",\"requestClientIp\":\"0.0.0.0\",\"requestHeader\":\"Accept=[text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7]\\nAuth=null\\nHost=[]\",\"requestParams\":\"{\\\"userId\\\":\\\"666666\\\"}\",\"responseBody\":\"{\\\"msg\\\":\\\"OK\\\",\\\"code\\\":0,\\\"data\\\":[],\\\"success\\\":true,\\\"error\\\":false}\"}")
		})
	})
}

func TestProcessorConstReplaceWork(t *testing.T) {
	Convey("Test Match = const.", t, func() {
		processor := &ProcessorStringReplace{
			SourceKey:     "content",
			Method:        MethodConst,
			Match:         "how old are you?",
			ReplaceString: "",
		}
		err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		record := `hello,how old are you? nice to meet you`
		log := &protocol.Log{Time: 0}
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
		logs := []*protocol.Log{}
		logs = append(logs, log)
		logs = processor.ProcessLogs(logs)
		So(len(logs[0].Contents), ShouldEqual, 1)
		So(logs[0].Contents[0].Key, ShouldEqual, `content`)
		So(logs[0].Contents[0].Value, ShouldEqual, `hello, nice to meet you`)
	})
}

func TestProcessorUnquoteReplaceWork(t *testing.T) {
	Convey("Test Match = unquote.", t, func() {
		processor := &ProcessorStringReplace{
			SourceKey: "content",
			Method:    MethodUnquote,
		}
		err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("Test unquote1", func() {
			record := "{\\x22UNAME\\x22:\\x22\\x22,\\x22GID\\x22:\\x22\\x22,\\x22PAID\\x22:\\x22\\x22,\\x22UUID\\x22:\\x22\\x22,\\x22STARTTIME\\x22:\\x22\\x22,\\x22ENDTIME\\x22:\\x22\\x22,\\x22UID\\x22:\\x2212345678\\x22,\\x22page_num\\x22:1,\\x22page_size\\x22:10}"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			logs := []*protocol.Log{}
			logs = append(logs, log)
			logs = processor.ProcessLogs(logs)
			So(len(logs[0].Contents), ShouldEqual, 1)
			So(logs[0].Contents[0].Key, ShouldEqual, `content`)
			So(logs[0].Contents[0].Value, ShouldEqual, `{"UNAME":"","GID":"","PAID":"","UUID":"","STARTTIME":"","ENDTIME":"","UID":"12345678","page_num":1,"page_size":10}`)
		})

		Convey("Test unquote2", func() {
			record := "aaa\"\\u554a"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			logs := []*protocol.Log{}
			logs = append(logs, log)
			logs = processor.ProcessLogs(logs)
			So(len(logs[0].Contents), ShouldEqual, 1)
			So(logs[0].Contents[0].Key, ShouldEqual, `content`)
			So(logs[0].Contents[0].Value, ShouldEqual, `aaa"啊`)
		})

		Convey("Test unquote3", func() {
			record := "\"message\""
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			logs := []*protocol.Log{}
			logs = append(logs, log)
			logs = processor.ProcessLogs(logs)
			So(len(logs[0].Contents), ShouldEqual, 1)
			So(logs[0].Contents[0].Key, ShouldEqual, `content`)
			So(logs[0].Contents[0].Value, ShouldEqual, `message`)
		})
	})
}

func TestProcessorRegexChinessCharacters(t *testing.T) {
	Convey("Test Chinese Match.", t, func() {
		processor := &ProcessorStringReplace{
			SourceKey:     "content",
			Method:        MethodRegex,
			Match:         "\uff08[^）]*\uff09",
			ReplaceString: "",
		}
		err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("Test regex", func() {
			record := `替换前（需要被替换的字符）12345`
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			logs := []*protocol.Log{}
			logs = append(logs, log)
			logs = processor.ProcessLogs(logs)
			So(len(logs[0].Contents), ShouldEqual, 1)
			So(logs[0].Contents[0].Key, ShouldEqual, `content`)
			So(logs[0].Contents[0].Value, ShouldEqual, `替换前12345`)
		})

		processor = &ProcessorStringReplace{
			SourceKey:     "content",
			Method:        MethodConst,
			Match:         "（需要被替换的字符）",
			ReplaceString: "",
		}
		err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)
		Convey("Test const", func() {
			record := `替换前（需要被替换的字符）12345`
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			logs := []*protocol.Log{}
			logs = append(logs, log)
			logs = processor.ProcessLogs(logs)
			So(len(logs[0].Contents), ShouldEqual, 1)
			So(logs[0].Contents[0].Key, ShouldEqual, `content`)
			So(logs[0].Contents[0].Value, ShouldEqual, `替换前12345`)
		})
	})
}

func TestProcessorRegexDestKey(t *testing.T) {
	Convey("Test Dest key.", t, func() {
		processor := &ProcessorStringReplace{
			SourceKey:     "ip",
			Method:        MethodRegex,
			Match:         "(\\d.*\\.)\\d+",
			ReplaceString: "$1*/24",
			DestKey:       "ip_sebnet",
		}
		err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		ipRecord := `10.10.239.16`
		log := &protocol.Log{Time: 0}
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: "ip", Value: ipRecord})
		logs := []*protocol.Log{}
		logs = append(logs, log)
		logs = processor.ProcessLogs(logs)
		So(len(logs[0].Contents), ShouldEqual, 2)
		So(logs[0].Contents[0].Key, ShouldEqual, `ip`)
		So(logs[0].Contents[0].Value, ShouldEqual, `10.10.239.16`)
		So(logs[0].Contents[1].Key, ShouldEqual, `ip_sebnet`)
		So(logs[0].Contents[1].Value, ShouldEqual, `10.10.239.*/24`)
	})
}

func benchmarkReplace(b *testing.B, s *ProcessorStringReplace, replaceCount int, doubleDashCount int, totalLength int) {
	doubleDashV := "\\\\_"
	replaceV := "b_"
	value := ""
	for countIdx := 0; countIdx < replaceCount; countIdx++ {
		value += replaceV
	}
	for countIdx := 0; countIdx < doubleDashCount; countIdx++ {
		value += doubleDashV
	}
	for countIdx := replaceCount*len(replaceV) + doubleDashCount*len(doubleDashV); countIdx < totalLength; countIdx++ {
		value += "a"
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
// Regex/Const/Unquote: method types
// 10: replace char length.
// 0: double dash char length.
// 100: content length.
// 209143	      5091 ns/op	    4688 B/op	      58 allocs/op
func BenchmarkSplit_Regex_10_0_100(b *testing.B) {
	s := &ProcessorStringReplace{
		SourceKey:     "content",
		Method:        MethodRegex,
		Match:         "b",
		ReplaceString: "",
	}
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkReplace(b, s, 10, 0, 100)
}

// 6149728	       201.5 ns/op	      96 B/op	       1 allocs/op
func BenchmarkSplit_Const_10_0_100(b *testing.B) {
	s := &ProcessorStringReplace{
		SourceKey:     "content",
		Method:        MethodConst,
		Match:         "b",
		ReplaceString: "",
	}
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkReplace(b, s, 10, 0, 100)
}

// 1321136         772.7 ns/op         408 B/op         5 allocs/op
func BenchmarkSplit_Unquote_0_10_100(b *testing.B) {
	s := &ProcessorStringReplace{
		SourceKey: "content",
		Method:    MethodUnquote,
	}
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkReplace(b, s, 0, 10, 100)
}

// 26280	     46231 ns/op	   43648 B/op	     509 allocs/op
func BenchmarkSplit_Regex_100_0_1000(b *testing.B) {
	s := &ProcessorStringReplace{
		SourceKey:     "content",
		Method:        MethodRegex,
		Match:         "b",
		ReplaceString: "",
	}
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkReplace(b, s, 100, 0, 1000)
}

// 803905	      1482 ns/op	    1024 B/op	       1 allocs/op
func BenchmarkSplit_Const_100_0_1000(b *testing.B) {
	s := &ProcessorStringReplace{
		SourceKey:     "content",
		Method:        MethodConst,
		Match:         "b",
		ReplaceString: "",
	}
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkReplace(b, s, 100, 0, 1000)
}

// 181201           5798 ns/op       3624 B/op          5 allocs/op
func BenchmarkSplit_Unquote_0_100_1000(b *testing.B) {
	s := &ProcessorStringReplace{
		SourceKey: "content",
		Method:    MethodUnquote,
	}
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkReplace(b, s, 0, 100, 1000)
}

// 129014	      8364 ns/op	    6232 B/op	      14 allocs/op
func BenchmarkSplit_Regex_1_0_512(b *testing.B) {
	s := &ProcessorStringReplace{
		SourceKey:     "content",
		Method:        MethodRegex,
		Match:         "b",
		ReplaceString: "",
	}
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkReplace(b, s, 1, 0, 512)
}

// 5886925	       233.8 ns/op	     552 B/op	       3 allocs/op
func BenchmarkSplit_Const_1_0_512(b *testing.B) {
	s := &ProcessorStringReplace{
		SourceKey:     "content",
		Method:        MethodConst,
		Match:         "b",
		ReplaceString: "",
	}
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkReplace(b, s, 1, 0, 512)
}

// 283285           3819 ns/op        2024 B/op          5 allocs/op
func BenchmarkSplit_Unquote_0_1_512(b *testing.B) {
	s := &ProcessorStringReplace{
		SourceKey: "content",
		Method:    MethodUnquote,
	}
	ctx := &pm.ContextImp{}
	ctx.InitContext("test", "test", "test")
	_ = s.Init(ctx)

	benchmarkReplace(b, s, 0, 1, 512)
}
