// Copyright 2022 iLogtail Authors
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

package grok

import (
	"testing"

	"github.com/dlclark/regexp2"
	. "github.com/smartystreets/goconvey/convey"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

/*
PASS
coverage: 92.8% of statements
ok      github.com/alibaba/ilogtail/plugins/processor/grok      0.221s
*/

func newProcessor() (*ProcessorGrok, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorGrok{
		CustomPatternDir:    []string{},
		CustomPatterns:      map[string]string{},
		SourceKey:           "content",
		Match:               []string{},
		TimeoutMilliSeconds: 0,
		IgnoreParseFailure:  true,
		KeepSource:          true,
		NoKeyError:          true,
		NoMatchError:        true,
		TimeoutError:        true,
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestProcessorGrokInit(t *testing.T) {
	Convey("Test load Grok Patterns from different single sources, determine if they can be parsed properly as correct regular expressions.", t, func() {
		processor, err := newProcessor()
		So(err, ShouldBeNil)

		Convey("Load Grok Patterns from processor_grok_default_patterns.go, then compile all of them.", func() {

			for k, v := range processor.processedPatterns {
				_, err = regexp2.Compile(v, regexp2.RE2)
				if err != nil {
					t.Log(k, v)
				}
				So(err, ShouldBeNil)
			}

			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldBeNil)
		})

		Convey("Load Grok Patterns from CustomPatternDir, then compile all of them.", func() {
			processor.CustomPatternDir = []string{"./test_patterns"}
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldBeNil)

			So(processor.processedPatterns["ELB_URI"], ShouldEqual, ans["SLB_URI"])
			for k, v := range processor.processedPatterns {
				_, err = regexp2.Compile(v, regexp2.RE2)
				if err != nil {
					t.Log(k, v)
				}
				So(err, ShouldBeNil)
			}
		})

		Convey("Load Grok Patterns from CustomPattern, then compile it.", func() {
			processor.CustomPatternDir = []string{}
			processor.CustomPatterns = map[string]string{"HTTP": "%{IP:client} %{WORD:method} %{URIPATHPARAM:request} %{NUMBER:bytes} %{NUMBER:duration}"}
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldBeNil)

			So(processor.processedPatterns["HTTP"], ShouldEqual, ans["%{HTTP}"])
			_, err = regexp2.Compile(processor.processedPatterns["Http"], regexp2.RE2)
			So(err, ShouldBeNil)
		})

		Convey("Load Grok Patterns from multiple sources and let some patterns conflict with others, test coverage loading.", func() {
			processor.CustomPatternDir = []string{"./test_patterns"}
			processor.CustomPatterns = map[string]string{"DATA": "%{IP:client} %{WORD:method} %{URIPATHPARAM:request} %{NUMBER:bytes} %{NUMBER:duration}"} // DATA was difined in processor_grok_default_patterns.go and ./patterns/grok-pattern
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldBeNil)

			So(processor.processedPatterns["DATA"], ShouldEqual, ans["%{HTTP}"])
			_, err := regexp2.Compile(processor.processedPatterns["Http"], regexp2.RE2)
			So(err, ShouldBeNil)
		})
	})
}

func TestProcessorGrokParse(t *testing.T) {
	Convey("Test parse logs with one Grok Pattern in Match", t, func() {
		processor, err := newProcessor()
		So(err, ShouldBeNil)
		processor.Match = []string{
			"%{WORD:word1} %{NUMBER:request_time} %{WORD:word2}",
		}
		err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("A single log which is in english.", func() {
			record := "begin 123.456 end"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.processGrok(log, &record)

			So(res, ShouldEqual, matchSuccess)
			So(len(log.Contents), ShouldEqual, 4)
			So(log.Contents[1].Value, ShouldEqual, "begin")
			So(log.Contents[2].Value, ShouldEqual, "123.456")
			So(log.Contents[3].Value, ShouldEqual, "end")
		})

		Convey("A single log which is empty.", func() {
			record := ""
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.processGrok(log, &record)

			So(res, ShouldEqual, matchFail)
			So(len(log.Contents), ShouldEqual, 1)
		})

		Convey("A single log which has escape character.", func() {
			record := "begin 123.456 end\n"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.processGrok(log, &record)

			So(res, ShouldEqual, matchSuccess)
			So(len(log.Contents), ShouldEqual, 4)
			So(log.Contents[1].Value, ShouldEqual, "begin")
			So(log.Contents[2].Value, ShouldEqual, "123.456")
			So(log.Contents[3].Value, ShouldEqual, "end")
		})

		processor.Match = []string{
			"%{WORD:english-word} %{GREEDYDATA:message}",
		}
		err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)
		Convey("A single log which has special character.", func() {
			record := "hello こんにちは"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.processGrok(log, &record)

			So(res, ShouldEqual, matchSuccess)
			So(len(log.Contents), ShouldEqual, 3)
			So(log.Contents[1].Value, ShouldEqual, "hello")
			So(log.Contents[2].Value, ShouldEqual, "こんにちは")
		})

		processor.Match = []string{
			"%{WORD:english-word} %{GREEDYDATA:message} (?P<message2>.*)",
		}
		err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)
		Convey("Grok Pattern with grok expression and regex expression.", func() {
			record := "hello こんにちは 你好"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.processGrok(log, &record)

			So(res, ShouldEqual, matchSuccess)
			So(len(log.Contents), ShouldEqual, 4)
			So(log.Contents[1].Value, ShouldEqual, "hello")
			So(log.Contents[2].Value, ShouldEqual, "こんにちは")
			So(log.Contents[3].Value, ShouldEqual, "你好")
		})

		processor.Match = []string{
			`\[%{TIMESTAMP_ISO8601:time_local}\] %{NUMBER:pid} %{QUOTEDSTRING:thread} prio=%{NUMBER:prio} tid=%{BASE16NUM:tid} nid=%{BASE16NUM:nid} %{DATA:func} \[%{BASE16NUM:addr}\]%{SPACE}(?ms)%{GREEDYDATA:stack}`,
		}
		err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)
		Convey("Grok Pattern with multiline expression.", func() {
			record := `[2023-02-09T00:24:43.922554223+08:00] 1 "BLOCKED_TEST pool-1-thread-2" prio=6 tid=0x0000000007673800 nid=0x260c waiting for monitor entry [0x0000000008abf000]
java.lang.Thread.State: BLOCKED (on object monitor)
			 at com.nbp.theplatform.threaddump.ThreadBlockedState.monitorLock(ThreadBlockedState.java:43)
			 - waiting to lock <0x0000000780a000b0> (a com.nbp.theplatform.threaddump.ThreadBlockedState)`
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.processGrok(log, &record)

			So(res, ShouldEqual, matchSuccess)
			So(len(log.Contents), ShouldEqual, 10)
			So(log.Contents[1].Value, ShouldEqual, "2023-02-09T00:24:43.922554223+08:00")
			So(log.Contents[2].Value, ShouldEqual, "1")
			So(log.Contents[3].Value, ShouldEqual, "\"BLOCKED_TEST pool-1-thread-2\"")
			So(log.Contents[4].Value, ShouldEqual, "6")
			So(log.Contents[5].Value, ShouldEqual, "0x0000000007673800")
			So(log.Contents[6].Value, ShouldEqual, "0x260c")
			So(log.Contents[7].Value, ShouldEqual, "waiting for monitor entry")
			So(log.Contents[8].Value, ShouldEqual, "0x0000000008abf000")
			So(log.Contents[9].Value, ShouldEqual, `java.lang.Thread.State: BLOCKED (on object monitor)
			 at com.nbp.theplatform.threaddump.ThreadBlockedState.monitorLock(ThreadBlockedState.java:43)
			 - waiting to lock <0x0000000780a000b0> (a com.nbp.theplatform.threaddump.ThreadBlockedState)`)
		})
	})

	Convey("Test parse logs with multiple Grok Patterns in Match and test some settings", t, func() {
		processor, err := newProcessor()
		So(err, ShouldBeNil)
		processor.CustomPatterns = map[string]string{
			"HTTP": "%{IP:client} %{WORD:method} %{URIPATHPARAM:request} %{NUMBER:bytes} %{NUMBER:duration}",
		}
		processor.Match = []string{
			"%{HTTP}",
			"%{WORD:word1} %{NUMBER:request_time} %{WORD:word2}",
			"%{YEAR:year} %{MONTH:month} %{MONTHDAY:day} %{QUOTEDSTRING:motto}",
		}
		err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("Parse logs with multiple Grok Patterns.", func() {
			log1 := `begin 123.456 end`
			logPb1 := test.CreateLogs("content", log1)
			log2 := `2019 June 24 "I am iron man"`
			logPb2 := test.CreateLogs("content", log2)
			log3 := `WRONG LOG`
			logPb3 := test.CreateLogs("content", log3)
			log4 := `10.0.0.0 GET /index.html 15824 0.043`
			logPb4 := test.CreateLogs("content", log4)
			logArray := make([]*protocol.Log, 4)
			logArray[0] = logPb1
			logArray[1] = logPb2
			logArray[2] = logPb3
			logArray[3] = logPb4

			outLogs := processor.ProcessLogs(logArray)

			So(len(outLogs), ShouldEqual, 4)

			So(len(outLogs[0].Contents), ShouldEqual, 4)
			So(outLogs[0].Contents[1].GetKey(), ShouldEqual, "word1")
			So(outLogs[0].Contents[1].GetValue(), ShouldEqual, "begin")
			So(outLogs[0].Contents[2].GetKey(), ShouldEqual, "request_time")
			So(outLogs[0].Contents[2].GetValue(), ShouldEqual, "123.456")
			So(outLogs[0].Contents[3].GetKey(), ShouldEqual, "word2")
			So(outLogs[0].Contents[3].GetValue(), ShouldEqual, "end")

			So(len(outLogs[1].Contents), ShouldEqual, 5)
			So(outLogs[1].Contents[1].GetKey(), ShouldEqual, "year")
			So(outLogs[1].Contents[1].GetValue(), ShouldEqual, "2019")
			So(outLogs[1].Contents[2].GetKey(), ShouldEqual, "month")
			So(outLogs[1].Contents[2].GetValue(), ShouldEqual, "June")
			So(outLogs[1].Contents[3].GetKey(), ShouldEqual, "day")
			So(outLogs[1].Contents[3].GetValue(), ShouldEqual, "24")
			So(outLogs[1].Contents[4].GetKey(), ShouldEqual, "motto")
			So(outLogs[1].Contents[4].GetValue(), ShouldEqual, "\"I am iron man\"")

			So(len(outLogs[2].Contents), ShouldEqual, 1)

			So(len(outLogs[3].Contents), ShouldEqual, 6)
			So(outLogs[3].Contents[1].GetKey(), ShouldEqual, "client")
			So(outLogs[3].Contents[1].GetValue(), ShouldEqual, "10.0.0.0")
			So(outLogs[3].Contents[2].GetKey(), ShouldEqual, "method")
			So(outLogs[3].Contents[2].GetValue(), ShouldEqual, "GET")
			So(outLogs[3].Contents[3].GetKey(), ShouldEqual, "request")
			So(outLogs[3].Contents[3].GetValue(), ShouldEqual, "/index.html")
			So(outLogs[3].Contents[4].GetKey(), ShouldEqual, "bytes")
			So(outLogs[3].Contents[4].GetValue(), ShouldEqual, "15824")
			So(outLogs[3].Contents[5].GetKey(), ShouldEqual, "duration")
			So(outLogs[3].Contents[5].GetValue(), ShouldEqual, "0.043")
		})

		Convey("Test IgnoreParseFailure.", func() {
			processor.IgnoreParseFailure = false
			log1 := `begin 123.456 end`
			logPb1 := test.CreateLogs("content", log1)
			log2 := `2019 June 24 "I am iron man"`
			logPb2 := test.CreateLogs("content", log2)
			log3 := `WRONG LOG`
			logPb3 := test.CreateLogs("content", log3)
			log4 := `10.0.0.0 GET /index.html 15824 0.043`
			logPb4 := test.CreateLogs("content", log4)
			logArray := make([]*protocol.Log, 4)
			logArray[0] = logPb1
			logArray[1] = logPb2
			logArray[2] = logPb3
			logArray[3] = logPb4

			outLogs := processor.ProcessLogs(logArray)

			So(len(outLogs), ShouldEqual, 4)

			So(len(outLogs[2].Contents), ShouldEqual, 0)
		})

		Convey("Test KeepSource.", func() {
			processor.IgnoreParseFailure = true
			processor.KeepSource = false

			log1 := `begin 123.456 end`
			logPb1 := test.CreateLogs("content", log1)
			log2 := `2019 June 24 "I am iron man"`
			logPb2 := test.CreateLogs("content", log2)
			log3 := `WRONG LOG`
			logPb3 := test.CreateLogs("content", log3)
			log4 := `10.0.0.0 GET /index.html 15824 0.043`
			logPb4 := test.CreateLogs("content", log4)
			logArray := make([]*protocol.Log, 4)
			logArray[0] = logPb1
			logArray[1] = logPb2
			logArray[2] = logPb3
			logArray[3] = logPb4

			outLogs := processor.ProcessLogs(logArray)

			So(len(outLogs), ShouldEqual, 4)

			So(len(outLogs[0].Contents), ShouldEqual, 3)
			So(outLogs[0].Contents[0].GetKey(), ShouldEqual, "word1")
			So(outLogs[0].Contents[0].GetValue(), ShouldEqual, "begin")
			So(outLogs[0].Contents[1].GetKey(), ShouldEqual, "request_time")
			So(outLogs[0].Contents[1].GetValue(), ShouldEqual, "123.456")
			So(outLogs[0].Contents[2].GetKey(), ShouldEqual, "word2")
			So(outLogs[0].Contents[2].GetValue(), ShouldEqual, "end")

			So(len(outLogs[1].Contents), ShouldEqual, 4)
			So(outLogs[1].Contents[0].GetKey(), ShouldEqual, "year")
			So(outLogs[1].Contents[0].GetValue(), ShouldEqual, "2019")
			So(outLogs[1].Contents[1].GetKey(), ShouldEqual, "month")
			So(outLogs[1].Contents[1].GetValue(), ShouldEqual, "June")
			So(outLogs[1].Contents[2].GetKey(), ShouldEqual, "day")
			So(outLogs[1].Contents[2].GetValue(), ShouldEqual, "24")
			So(outLogs[1].Contents[3].GetKey(), ShouldEqual, "motto")
			So(outLogs[1].Contents[3].GetValue(), ShouldEqual, "\"I am iron man\"")

			So(len(outLogs[2].Contents), ShouldEqual, 1)

			So(len(outLogs[3].Contents), ShouldEqual, 5)
			So(outLogs[3].Contents[0].GetKey(), ShouldEqual, "client")
			So(outLogs[3].Contents[0].GetValue(), ShouldEqual, "10.0.0.0")
			So(outLogs[3].Contents[1].GetKey(), ShouldEqual, "method")
			So(outLogs[3].Contents[1].GetValue(), ShouldEqual, "GET")
			So(outLogs[3].Contents[2].GetKey(), ShouldEqual, "request")
			So(outLogs[3].Contents[2].GetValue(), ShouldEqual, "/index.html")
			So(outLogs[3].Contents[3].GetKey(), ShouldEqual, "bytes")
			So(outLogs[3].Contents[3].GetValue(), ShouldEqual, "15824")
			So(outLogs[3].Contents[4].GetKey(), ShouldEqual, "duration")
			So(outLogs[3].Contents[4].GetValue(), ShouldEqual, "0.043")
		})
	})
}

func TestProcessorGrokError(t *testing.T) {
	Convey("Test init error", t, func() {
		processor, err := newProcessor()
		So(err, ShouldBeNil)

		Convey("Load Grok Patterns from path error.", func() {
			processor.CustomPatternDir = []string{"./no_exist_path"}
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldNotBeNil)
		})

		Convey("Load Grok Pattern from CustomPatterns which has grammar error.", func() {
			processor.CustomPatternDir = []string{}
			processor.CustomPatterns = map[string]string{"TEST": "%{IP:client} ("}
			processor.Match = []string{"%{TEST}"}
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldNotBeNil)
		})

		Convey("Load Grok Patterns from CustomPatterns which have circular references error.", func() {
			processor.CustomPatternDir = []string{}
			processor.CustomPatterns = map[string]string{
				"A": "%{B:b}",
				"B": "%{A:a}",
			}
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldNotBeNil)
		})

	})
}

var ans = map[string]string{
	"%{WORD:word1} %{NUMBER:request_time} %{WORD:word2}":                `(?P<word1>\b\w+\b) (?P<request_time>(?:((?<![0-9.+-])(?>[+-]?(?:(?:[0-9]+(?:\.[0-9]+)?)|(?:\.[0-9]+)))))) (?P<word2>\b\w+\b)`,
	"%{YEAR:year} %{MONTH:month} %{MONTHDAY:day} %{QUOTEDSTRING:motto}": "(?P<year>(?>\\d\\d){1,2}) (?P<month>\\b(?:Jan(?:uary|uar)?|Feb(?:ruary|ruar)?|M(?:a|ä)?r(?:ch|z)?|Apr(?:il)?|Ma(?:y|i)?|Jun(?:e|i)?|Jul(?:y)?|Aug(?:ust)?|Sep(?:tember)?|O(?:c|k)?t(?:ober)?|Nov(?:ember)?|De(?:c|z)(?:ember)?)\\b) (?P<day>(?:(?:0[1-9])|(?:[12][0-9])|(?:3[01])|[1-9])) (?P<motto>(?>(?<!\\\\)(?>\"(?>\\\\.|[^\\\\\"]+)+\"|\"\"|(?>'(?>\\\\.|[^\\\\']+)+')|''|(?>`(?>\\\\.|[^\\\\`]+)+`)|``)))",
	"%{HTTP}": `(?P<client>(?:(((([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|(([0-9A-Fa-f]{1,4}:){6}(:[0-9A-Fa-f]{1,4}|((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){5}(((:[0-9A-Fa-f]{1,4}){1,2})|:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){4}(((:[0-9A-Fa-f]{1,4}){1,3})|((:[0-9A-Fa-f]{1,4})?:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){3}(((:[0-9A-Fa-f]{1,4}){1,4})|((:[0-9A-Fa-f]{1,4}){0,2}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){2}(((:[0-9A-Fa-f]{1,4}){1,5})|((:[0-9A-Fa-f]{1,4}){0,3}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){1}(((:[0-9A-Fa-f]{1,4}){1,6})|((:[0-9A-Fa-f]{1,4}){0,4}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(:(((:[0-9A-Fa-f]{1,4}){1,7})|((:[0-9A-Fa-f]{1,4}){0,5}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:)))(%.+)?)|((?<![0-9])(?:(?:[0-1]?[0-9]{1,2}|2[0-4][0-9]|25[0-5])[.](?:[0-1]?[0-9]{1,2}|2[0-4][0-9]|25[0-5])[.](?:[0-1]?[0-9]{1,2}|2[0-4][0-9]|25[0-5])[.](?:[0-1]?[0-9]{1,2}|2[0-4][0-9]|25[0-5]))(?![0-9])))) (?P<method>\b\w+\b) (?P<request>((?:/[A-Za-z0-9$.+!*'(){},~:;=@#%_\-]*)+)(?:(\?[A-Za-z0-9$.+!*'|(){},~@#%&/=:;_?\-\[\]<>]*))?) (?P<bytes>(?:((?<![0-9.+-])(?>[+-]?(?:(?:[0-9]+(?:\.[0-9]+)?)|(?:\.[0-9]+)))))) (?P<duration>(?:((?<![0-9.+-])(?>[+-]?(?:(?:[0-9]+(?:\.[0-9]+)?)|(?:\.[0-9]+))))))`,
	"SLB_URI": `(?P<proto>[A-Za-z]+(\+[A-Za-z+]+)?)://(?:(([a-zA-Z0-9._-]+))(?::[^@]*)?@)?(?:(?P<urihost>((?:((?:(((([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|(([0-9A-Fa-f]{1,4}:){6}(:[0-9A-Fa-f]{1,4}|((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){5}(((:[0-9A-Fa-f]{1,4}){1,2})|:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){4}(((:[0-9A-Fa-f]{1,4}){1,3})|((:[0-9A-Fa-f]{1,4})?:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){3}(((:[0-9A-Fa-f]{1,4}){1,4})|((:[0-9A-Fa-f]{1,4}){0,2}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){2}(((:[0-9A-Fa-f]{1,4}){1,5})|((:[0-9A-Fa-f]{1,4}){0,3}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){1}(((:[0-9A-Fa-f]{1,4}){1,6})|((:[0-9A-Fa-f]{1,4}){0,4}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(:(((:[0-9A-Fa-f]{1,4}){1,7})|((:[0-9A-Fa-f]{1,4}){0,5}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:)))(%.+)?)|((?<![0-9])(?:(?:[0-1]?[0-9]{1,2}|2[0-4][0-9]|25[0-5])[.](?:[0-1]?[0-9]{1,2}|2[0-4][0-9]|25[0-5])[.](?:[0-1]?[0-9]{1,2}|2[0-4][0-9]|25[0-5])[.](?:[0-1]?[0-9]{1,2}|2[0-4][0-9]|25[0-5]))(?![0-9]))))|(\b(?:[0-9A-Za-z][0-9A-Za-z-]{0,62})(?:\.(?:[0-9A-Za-z][0-9A-Za-z-]{0,62}))*(\.?|\b))))(?::(?P<port>\b(?:[1-9][0-9]*)\b))?))?(?:((?P<path>(?:/[A-Za-z0-9$.+!*'(){},~:;=@#%_\-]*)+)(?:(?P<params>\?[A-Za-z0-9$.+!*'|(){},~@#%&/=:;_?\-\[\]<>]*))?))?`,
}
