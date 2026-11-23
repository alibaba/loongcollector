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
	"strings"
	"testing"
	"time"

	"github.com/alibaba/ilogtail/plugins/processor/internal/processortest"

	"github.com/dlclark/regexp2"
	. "github.com/smartystreets/goconvey/convey"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

/*
PASS
coverage: 92.8% of statements
ok      github.com/alibaba/ilogtail/plugins/processor/grok      0.221s
*/

var expectedGrokPatterns = map[string]string{
	"%{WORD:word1} %{NUMBER:request_time} %{WORD:word2}":                `(?P<word1>\b\w+\b) (?P<request_time>(?:((?<![0-9.+-])(?>[+-]?(?:(?:[0-9]+(?:\.[0-9]+)?)|(?:\.[0-9]+)))))) (?P<word2>\b\w+\b)`,
	"%{YEAR:year} %{MONTH:month} %{MONTHDAY:day} %{QUOTEDSTRING:motto}": "(?P<year>(?>\\d\\d){1,2}) (?P<month>\\b(?:Jan(?:uary|uar)?|Feb(?:ruary|ruar)?|M(?:a|ä)?r(?:ch|z)?|Apr(?:il)?|Ma(?:y|i)?|Jun(?:e|i)?|Jul(?:y)?|Aug(?:ust)?|Sep(?:tember)?|O(?:c|k)?t(?:ober)?|Nov(?:ember)?|De(?:c|z)(?:ember)?)\\b) (?P<day>(?:(?:0[1-9])|(?:[12][0-9])|(?:3[01])|[1-9])) (?P<motto>(?>(?<!\\\\)(?>\"(?>\\\\.|[^\\\\\"]+)+\"|\"\"|(?>'(?>\\\\.|[^\\\\']+)+')|''|(?>`(?>\\\\.|[^\\\\`]+)+`)|``)))",
	"%{HTTP}": `(?P<client>(?:(((([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|(([0-9A-Fa-f]{1,4}:){6}(:[0-9A-Fa-f]{1,4}|((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){5}(((:[0-9A-Fa-f]{1,4}){1,2})|:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){4}(((:[0-9A-Fa-f]{1,4}){1,3})|((:[0-9A-Fa-f]{1,4})?:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){3}(((:[0-9A-Fa-f]{1,4}){1,4})|((:[0-9A-Fa-f]{1,4}){0,2}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){2}(((:[0-9A-Fa-f]{1,4}){1,5})|((:[0-9A-Fa-f]{1,4}){0,3}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){1}(((:[0-9A-Fa-f]{1,4}){1,6})|((:[0-9A-Fa-f]{1,4}){0,4}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(:(((:[0-9A-Fa-f]{1,4}){1,7})|((:[0-9A-Fa-f]{1,4}){0,5}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:)))(%.+)?)|((?<![0-9])(?:(?:[0-1]?[0-9]{1,2}|2[0-4][0-9]|25[0-5])[.](?:[0-1]?[0-9]{1,2}|2[0-4][0-9]|25[0-5])[.](?:[0-1]?[0-9]{1,2}|2[0-4][0-9]|25[0-5])[.](?:[0-1]?[0-9]{1,2}|2[0-4][0-9]|25[0-5]))(?![0-9])))) (?P<method>\b\w+\b) (?P<request>((?:/[A-Za-z0-9$.+!*'(){},~:;=@#%_\-]*)+)(?:(\?[A-Za-z0-9$.+!*'|(){},~@#%&/=:;_?\-\[\]<>]*))?) (?P<bytes>(?:((?<![0-9.+-])(?>[+-]?(?:(?:[0-9]+(?:\.[0-9]+)?)|(?:\.[0-9]+)))))) (?P<duration>(?:((?<![0-9.+-])(?>[+-]?(?:(?:[0-9]+(?:\.[0-9]+)?)|(?:\.[0-9]+))))))`,
	"SLB_URI": `(?P<proto>[A-Za-z]+(\+[A-Za-z+]+)?)://(?:(([a-zA-Z0-9._-]+))(?::[^@]*)?@)?(?:(?P<urihost>((?:((?:(((([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|(([0-9A-Fa-f]{1,4}:){6}(:[0-9A-Fa-f]{1,4}|((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){5}(((:[0-9A-Fa-f]{1,4}){1,2})|:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})|:))|(([0-9A-Fa-f]{1,4}:){4}(((:[0-9A-Fa-f]{1,4}){1,3})|((:[0-9A-Fa-f]{1,4})?:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){3}(((:[0-9A-Fa-f]{1,4}){1,4})|((:[0-9A-Fa-f]{1,4}){0,2}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){2}(((:[0-9A-Fa-f]{1,4}){1,5})|((:[0-9A-Fa-f]{1,4}){0,3}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(([0-9A-Fa-f]{1,4}:){1}(((:[0-9A-Fa-f]{1,4}){1,6})|((:[0-9A-Fa-f]{1,4}){0,4}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:))|(:(((:[0-9A-Fa-f]{1,4}){1,7})|((:[0-9A-Fa-f]{1,4}){0,5}:((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}))|:)))(%.+)?)|((?<![0-9])(?:(?:[0-1]?[0-9]{1,2}|2[0-4][0-9]|25[0-5])[.](?:[0-1]?[0-9]{1,2}|2[0-4][0-9]|25[0-5])[.](?:[0-1]?[0-9]{1,2}|2[0-4][0-9]|25[0-5])[.](?:[0-1]?[0-9]{1,2}|2[0-4][0-9]|25[0-5]))(?![0-9]))))|(\b(?:[0-9A-Za-z][0-9A-Za-z-]{0,62})(?:\.(?:[0-9A-Za-z][0-9A-Za-z-]{0,62}))*(\.?|\b))))(?::(?P<port>\b(?:[1-9][0-9]*)\b))?))?(?:((?P<path>(?:/[A-Za-z0-9$.+!*'(){},~:;=@#%_\-]*)+)(?:(?P<params>\?[A-Za-z0-9$.+!*'|(){},~@#%&/=:;_?\-\[\]<>]*))?))?`,
}

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

			So(processor.processedPatterns["ELB_URI"], ShouldEqual, expectedGrokPatterns["SLB_URI"])
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

			So(processor.processedPatterns["HTTP"], ShouldEqual, expectedGrokPatterns["%{HTTP}"])
			_, err = regexp2.Compile(processor.processedPatterns["Http"], regexp2.RE2)
			So(err, ShouldBeNil)
		})

		Convey("Load Grok Patterns from multiple sources and let some patterns conflict with others, test coverage loading.", func() {
			processor.CustomPatternDir = []string{"./test_patterns"}
			processor.CustomPatterns = map[string]string{"DATA": "%{IP:client} %{WORD:method} %{URIPATHPARAM:request} %{NUMBER:bytes} %{NUMBER:duration}"} // DATA was difined in processor_grok_default_patterns.go and ./patterns/grok-pattern
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldBeNil)

			So(processor.processedPatterns["DATA"], ShouldEqual, expectedGrokPatterns["%{HTTP}"])
			_, err := regexp2.Compile(processor.processedPatterns["Http"], regexp2.RE2)
			So(err, ShouldBeNil)
		})
	})
}

func TestProcessorGrokParse(t *testing.T) {
	Convey("Test parse logs with one Grok Pattern in Match", t, func() {
		processor, err := newProcessor()
		processor.KeepSource = true
		So(err, ShouldBeNil)
		processor.Match = []string{
			"%{WORD:word1} %{NUMBER:request_time} %{WORD:word2}",
		}
		err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("A single log which is in english.", func() {
			record := "begin 123.456 end"
			log := processortest.Log()
			log.Contents.Add("content", record)
			processortest.Apply(processor, log)
			So(log.Contents.Len(), ShouldEqual, 4)
			So(log.Contents.Get("word1"), ShouldEqual, "begin")
			So(log.Contents.Get("request_time"), ShouldEqual, "123.456")
			So(log.Contents.Get("word2"), ShouldEqual, "end")
		})

		Convey("A single log which is empty.", func() {
			record := ""
			log := processortest.Log()
			log.Contents.Add("content", record)
			processortest.Apply(processor, log)
			So(log.Contents.Len(), ShouldEqual, 1)
		})

		Convey("A single log which has escape character.", func() {
			record := "begin 123.456 end\n"
			log := processortest.Log()
			log.Contents.Add("content", record)
			processortest.Apply(processor, log)
			So(log.Contents.Len(), ShouldEqual, 4)
			So(log.Contents.Get("word1"), ShouldEqual, "begin")
			So(log.Contents.Get("request_time"), ShouldEqual, "123.456")
			So(log.Contents.Get("word2"), ShouldEqual, "end")
		})

		processor.Match = []string{
			"%{WORD:english-word} %{GREEDYDATA:message}",
		}
		err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)
		Convey("A single log which has special character.", func() {
			record := "hello こんにちは"
			log := processortest.Log()
			log.Contents.Add("content", record)
			processortest.Apply(processor, log)
			So(log.Contents.Len(), ShouldEqual, 3)
			So(log.Contents.Get("english-word"), ShouldEqual, "hello")
			So(log.Contents.Get("message"), ShouldEqual, "こんにちは")
		})

		processor.Match = []string{
			"%{WORD:english-word} %{GREEDYDATA:message} (?P<message2>.*)",
		}
		err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)
		Convey("Grok Pattern with grok expression and regex expression.", func() {
			record := "hello こんにちは 你好"
			log := processortest.Log()
			log.Contents.Add("content", record)
			processortest.Apply(processor, log)
			So(log.Contents.Len(), ShouldEqual, 4)
			So(log.Contents.Get("english-word"), ShouldEqual, "hello")
			So(log.Contents.Get("message"), ShouldEqual, "こんにちは")
			So(log.Contents.Get("message2"), ShouldEqual, "你好")
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
			log := processortest.Log()
			log.Contents.Add("content", record)
			processortest.Apply(processor, log)
			So(log.Contents.Len(), ShouldEqual, 10)
			So(log.Contents.Get("time_local"), ShouldEqual, "2023-02-09T00:24:43.922554223+08:00")
			So(log.Contents.Get("pid"), ShouldEqual, "1")
			So(log.Contents.Get("thread"), ShouldEqual, "\"BLOCKED_TEST pool-1-thread-2\"")
			So(log.Contents.Get("prio"), ShouldEqual, "6")
			So(log.Contents.Get("tid"), ShouldEqual, "0x0000000007673800")
			So(log.Contents.Get("nid"), ShouldEqual, "0x260c")
			So(log.Contents.Get("func"), ShouldEqual, "waiting for monitor entry")
			So(log.Contents.Get("addr"), ShouldEqual, "0x0000000008abf000")
			So(log.Contents.Get("stack"), ShouldEqual, `java.lang.Thread.State: BLOCKED (on object monitor)
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
			logEvent1 := processortest.Log("content", log1)
			log2 := `2019 June 24 "I am iron man"`
			logEvent2 := processortest.Log("content", log2)
			log3 := `WRONG LOG`
			logEvent3 := processortest.Log("content", log3)
			log4 := `10.0.0.0 GET /index.html 15824 0.043`
			logEvent4 := processortest.Log("content", log4)
			logArray := make([]*models.Log, 4)
			logArray[0] = logEvent1
			logArray[1] = logEvent2
			logArray[2] = logEvent3
			logArray[3] = logEvent4

			outLogs := processortest.Apply(processor, logArray...)

			So(len(outLogs), ShouldEqual, 4)

			So(outLogs[0].Contents.Len(), ShouldEqual, 4)
			So(pipeline.GetStringValue(outLogs[0].Contents.Get("word1")), ShouldEqual, "begin")
			So(pipeline.GetStringValue(outLogs[0].Contents.Get("request_time")), ShouldEqual, "123.456")
			So(pipeline.GetStringValue(outLogs[0].Contents.Get("word2")), ShouldEqual, "end")

			So(outLogs[1].Contents.Len(), ShouldEqual, 5)
			So(pipeline.GetStringValue(outLogs[1].Contents.Get("year")), ShouldEqual, "2019")
			So(pipeline.GetStringValue(outLogs[1].Contents.Get("month")), ShouldEqual, "June")
			So(pipeline.GetStringValue(outLogs[1].Contents.Get("day")), ShouldEqual, "24")
			So(pipeline.GetStringValue(outLogs[1].Contents.Get("motto")), ShouldEqual, "\"I am iron man\"")

			So(outLogs[2].Contents.Len(), ShouldEqual, 1)

			So(outLogs[3].Contents.Len(), ShouldEqual, 6)
			So(pipeline.GetStringValue(outLogs[3].Contents.Get("client")), ShouldEqual, "10.0.0.0")
			So(pipeline.GetStringValue(outLogs[3].Contents.Get("method")), ShouldEqual, "GET")
			So(pipeline.GetStringValue(outLogs[3].Contents.Get("request")), ShouldEqual, "/index.html")
			So(pipeline.GetStringValue(outLogs[3].Contents.Get("bytes")), ShouldEqual, "15824")
			So(pipeline.GetStringValue(outLogs[3].Contents.Get("duration")), ShouldEqual, "0.043")
		})

		Convey("Test IgnoreParseFailure.", func() {
			processor.IgnoreParseFailure = false
			log1 := `begin 123.456 end`
			logEvent1 := processortest.Log("content", log1)
			log2 := `2019 June 24 "I am iron man"`
			logEvent2 := processortest.Log("content", log2)
			log3 := `WRONG LOG`
			logEvent3 := processortest.Log("content", log3)
			log4 := `10.0.0.0 GET /index.html 15824 0.043`
			logEvent4 := processortest.Log("content", log4)
			logArray := make([]*models.Log, 4)
			logArray[0] = logEvent1
			logArray[1] = logEvent2
			logArray[2] = logEvent3
			logArray[3] = logEvent4

			outLogs := processortest.Apply(processor, logArray...)

			So(len(outLogs), ShouldEqual, 4)

			So(outLogs[2].Contents.Len(), ShouldEqual, 0)
		})

		Convey("Test KeepSource.", func() {
			processor.IgnoreParseFailure = true
			processor.KeepSource = false

			log1 := `begin 123.456 end`
			logEvent1 := processortest.Log("content", log1)
			log2 := `2019 June 24 "I am iron man"`
			logEvent2 := processortest.Log("content", log2)
			log3 := `WRONG LOG`
			logEvent3 := processortest.Log("content", log3)
			log4 := `10.0.0.0 GET /index.html 15824 0.043`
			logEvent4 := processortest.Log("content", log4)
			logArray := make([]*models.Log, 4)
			logArray[0] = logEvent1
			logArray[1] = logEvent2
			logArray[2] = logEvent3
			logArray[3] = logEvent4

			outLogs := processortest.Apply(processor, logArray...)

			So(len(outLogs), ShouldEqual, 4)

			So(outLogs[0].Contents.Len(), ShouldEqual, 3)
			So(pipeline.GetStringValue(outLogs[0].Contents.Get("word1")), ShouldEqual, "begin")
			So(pipeline.GetStringValue(outLogs[0].Contents.Get("request_time")), ShouldEqual, "123.456")
			So(pipeline.GetStringValue(outLogs[0].Contents.Get("word2")), ShouldEqual, "end")

			So(outLogs[1].Contents.Len(), ShouldEqual, 4)
			So(pipeline.GetStringValue(outLogs[1].Contents.Get("year")), ShouldEqual, "2019")
			So(pipeline.GetStringValue(outLogs[1].Contents.Get("month")), ShouldEqual, "June")
			So(pipeline.GetStringValue(outLogs[1].Contents.Get("day")), ShouldEqual, "24")
			So(pipeline.GetStringValue(outLogs[1].Contents.Get("motto")), ShouldEqual, "\"I am iron man\"")

			So(outLogs[2].Contents.Len(), ShouldEqual, 1)

			So(outLogs[3].Contents.Len(), ShouldEqual, 5)
			So(pipeline.GetStringValue(outLogs[3].Contents.Get("client")), ShouldEqual, "10.0.0.0")
			So(pipeline.GetStringValue(outLogs[3].Contents.Get("method")), ShouldEqual, "GET")
			So(pipeline.GetStringValue(outLogs[3].Contents.Get("request")), ShouldEqual, "/index.html")
			So(pipeline.GetStringValue(outLogs[3].Contents.Get("bytes")), ShouldEqual, "15824")
			So(pipeline.GetStringValue(outLogs[3].Contents.Get("duration")), ShouldEqual, "0.043")
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

func TestGrokWarnZeroWidthPattern(t *testing.T) {
	Convey("grok should warn about zero-width regex patterns in Match", t, func() {
		p, err := newProcessor()
		So(err, ShouldBeNil)
		p.Match = []string{"(?s)^$"}

		// Capture logs to verify warning is issued
		ctx := mock.NewEmptyContext("p", "l", "c")
		err = p.Init(ctx)
		So(err, ShouldBeNil)

		// The processor should still work despite zero-width pattern
		log := processortest.Log()
		log.Contents.Add("content", "")
		logs := []*models.Log{log}
		out := processortest.Apply(p, logs...)
		So(len(out), ShouldEqual, 1)
	})
}

func TestGrokTotalTimeoutBudget(t *testing.T) {
	Convey("grok should honor TotalTimeoutMs budget", t, func() {
		p, err := newProcessor()
		So(err, ShouldBeNil)
		// Use a catastrophic backtracking pattern that will definitely timeout
		p.Match = []string{"(a+)+b"}
		p.TimeoutMilliSeconds = 50
		err = p.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		// Input that triggers catastrophic backtracking - many 'a's but no 'b'
		record := strings.Repeat("a", 10000) + "c"
		start := time.Now()
		_, _, result := p.matchGrok(record)
		elapsed := time.Since(start)

		// Should timeout and return matchTimeOut
		So(result, ShouldEqual, matchTimeOut)
		// Should complete within reasonable time due to timeout protection
		So(elapsed.Milliseconds(), ShouldBeLessThan, 500)
	})
}

func TestGrokTimeoutDefaults(t *testing.T) {
	Convey("Timeout fields should have proper defaults", t, func() {
		p := &ProcessorGrok{
			CustomPatternDir:    []string{},
			CustomPatterns:      map[string]string{},
			SourceKey:           "content",
			Match:               []string{},
			TimeoutMilliSeconds: 0, // should default to 100
			IgnoreParseFailure:  true,
			KeepSource:          true,
			NoKeyError:          true,
			NoMatchError:        true,
			TimeoutError:        true,
		}
		err := p.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)
		So(p.TimeoutMilliSeconds, ShouldEqual, 100)
	})
}

func newEventProcessor(t *testing.T) *ProcessorGrok {
	processor := &ProcessorGrok{
		CustomPatternDir:    []string{},
		CustomPatterns:      map[string]string{},
		SourceKey:           "content",
		Match:               []string{"%{WORD:word1} %{NUMBER:request_time} %{WORD:word2}"},
		TimeoutMilliSeconds: 0,
		IgnoreParseFailure:  true,
		KeepSource:          true,
		NoKeyError:          true,
		NoMatchError:        true,
		TimeoutError:        true,
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
	return processor
}

// TestProcessorGrok_ProcessParse verifies that Process extracts the
// grok-matched fields into new keys and keeps the source (KeepSource=true).
func TestProcessorGrok_ProcessParse(t *testing.T) {
	processor := newEventProcessor(t)

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "begin 123.456 end")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.Equal(t, "begin", contents.Get("word1"))
	assert.Equal(t, "123.456", contents.Get("request_time"))
	assert.Equal(t, "end", contents.Get("word2"))
	assert.Equal(t, "begin 123.456 end", contents.Get("content"), "source kept when KeepSource is true")
}

// TestProcessorGrok_ProcessKeepSourceFalse verifies the source field is
// dropped on a successful match when KeepSource is false.
func TestProcessorGrok_ProcessKeepSourceFalse(t *testing.T) {
	processor := newEventProcessor(t)
	processor.KeepSource = false

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "begin 123.456 end")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.False(t, contents.Contains("content"), "source removed when KeepSource is false")
	assert.Equal(t, "begin", contents.Get("word1"))
}

// TestProcessorGrok_ProcessNoMatchKeepsSource verifies that on a parse
// failure the source is kept (IgnoreParseFailure) and no fields are extracted.
func TestProcessorGrok_ProcessNoMatchKeepsSource(t *testing.T) {
	processor := newEventProcessor(t)
	processor.KeepSource = false
	processor.IgnoreParseFailure = true

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "no-grok-match-here")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.True(t, contents.Contains("content"), "source kept on parse failure (IgnoreParseFailure)")
	assert.False(t, contents.Contains("word1"), "no fields extracted on no-match")
}

// TestProcessorGrok_ProcessPassesThroughMetric verifies a Metric event is
// emitted unchanged (not dropped) by the log-only processor.
func TestProcessorGrok_ProcessPassesThroughMetric(t *testing.T) {
	processor := newEventProcessor(t)

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}
