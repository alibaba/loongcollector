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

package desensitize

import (
	"errors"
	"strconv"
	"testing"

	. "github.com/smartystreets/goconvey/convey"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newProcessor() *ProcessorDesensitize {
	processor := &ProcessorDesensitize{
		SourceKey:     "content",
		Method:        "const",
		Match:         "regex",
		ReplaceString: "***",
		RegexBegin:    "'password':'",
		RegexContent:  "[^']*",
	}
	return processor
}

func TestChineseSample(t *testing.T) {
	Convey("Test Match = full. should not replace case with chinese", t, func() {
		processor := newProcessor()
		processor.Match = "regex"
		processor.RegexBegin = "码"
		processor.RegexContent = "XXX"
		err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("Test const chinese", func() {
			record := "中文电话号码有用中文电话号码有用"
			res := processor.desensitize(record)
			So(res, ShouldEqual, "中文电话号码有用中文电话号码有用")
		})
	})

	Convey("Test Match = regex. should replace case with chinese", t, func() {
		processor := newProcessor()
		processor.Match = "regex"
		processor.RegexBegin = "号"
		processor.RegexContent = "码"
		err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("Test const chinese", func() {
			record := "中文电话号码有用中文电话号码有用"
			res := processor.desensitize(record)
			So(res, ShouldEqual, "中文电话号***有用中文电话号***有用")
		})
	})
}

func TestProcessorDesensitizeInit(t *testing.T) {
	Convey("Test load Processor Desensitize.", t, func() {
		processor := newProcessor()

		Convey("Test load success", func() {
			err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldBeNil)
			So(processor.Description(), ShouldEqual, "desensitize processor for logtail")
		})

		Convey("Test load fail with no SourceKey error", func() {
			processor.SourceKey = ""
			err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldResemble, errors.New("parameter SourceKey should not be empty"))
		})

		Convey("Test load fail with no Method error", func() {
			processor.Method = ""
			err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldResemble, errors.New("parameter Method should be \"const\" or \"md5\""))
		})

		Convey("Test load fail with wrong Method error", func() {
			processor.Method = "Base64"
			err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldResemble, errors.New("parameter Method should be \"const\" or \"md5\""))
		})

		Convey("Test load fail with wrong Match error", func() {
			processor.Match = "overwrite"
			err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldResemble, errors.New("parameter Match should be \"full\" or \"regex\""))
		})

		Convey("Test load fail with no RegexBegin error", func() {
			processor.RegexBegin = ""
			err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldResemble, errors.New("need parameter RegexBegin"))
		})

		Convey("Test load fail with wrong RegexBegin error", func() {
			processor.RegexBegin = "*"
			err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldNotBeNil)
		})

		Convey("Test load fail with no RegexContent error", func() {
			processor.RegexContent = ""
			err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldResemble, errors.New("need parameter RegexContent"))
		})

		Convey("Test load fail with wrong RegexContent error", func() {
			processor.RegexContent = "*"
			err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldNotBeNil)
		})

		Convey("Test load fail with no ReplaceString error", func() {
			processor.Method = "const"
			processor.ReplaceString = ""
			err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldResemble, errors.New("parameter ReplaceString should not be empty when Method is \"const\""))
		})
	})
}

func TestProcessorDesensitizeWork(t *testing.T) {
	Convey("Test Match = full.", t, func() {
		processor := newProcessor()
		processor.Match = "full"
		err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("Test const", func() {
			record := "[{'account':'1812213231432969','password':'04a23f38'}, {'account':'1812213685634','password':'123a'}]"
			res := processor.desensitize(record)
			So(res, ShouldEqual, "***")
		})

		Convey("Test md5", func() {
			processor.Method = "md5"
			err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldBeNil)

			record := "[{'account':'1812213231432969','password':'04a23f38'}, {'account':'1812213685634','password':'123a'}]"
			res := processor.desensitize(record)
			So(res, ShouldEqual, "700085e3968c3efb83b54ba47dd1367d")
		})
	})

	Convey("Test Match = regex.", t, func() {
		processor := newProcessor()
		err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("Test const", func() {
			record := "[{'account':'1812213231432969','password':'04a23f38'}, {'account':'1812213685634','password':'123a'}, {'account':'1812213685634','password':'666777888ccc']"
			res := processor.desensitize(record)
			So(res, ShouldEqual, "[{'account':'1812213231432969','password':'***'}, {'account':'1812213685634','password':'***'}, {'account':'1812213685634','password':'***']")
		})

		processor.Method = "md5"
		err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("Test md5", func() {
			err := processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldBeNil)

			record := "[{'account':'1812213231432969','password':'04a23f38'}, {'account':'1812213685634','password':'123a'}]"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			logs := []*protocol.Log{}
			logs = append(logs, log)
			logs = processor.ProcessLogs(logs)
			So(logs[0].Contents[0].Value, ShouldEqual, "[{'account':'1812213231432969','password':'9c525f463ba1c89d6badcd78b2b7bd79'}, {'account':'1812213685634','password':'1552c03e78d38d5005d4ce7b8018addf'}]")
		})
	})
}

// 新旧对比
// 测试场景：password脱敏。使用regex匹配，const替换
// goos: linux
// goarch: amd64
// pkg: github.com/alibaba/ilogtail/plugins/processor/desensitize
// cpu: Intel(R) Xeon(R) Platinum 8369B CPU @ 2.70GHz
// BenchmarkDesensitizeTest

// Case 1: content中仅有一处需要脱敏，处理时间减少55.60%，内存占用减少59.14%
// 旧
// BenchmarkDesensitizeTest/original10-4         	   41397	     28838 ns/op	   22320 B/op	     320 allocs/op
// BenchmarkDesensitizeTest/original100-4        	    4684	    258775 ns/op	  223201 B/op	    3200 allocs/op
// BenchmarkDesensitizeTest/original1000-4       	     447	   2618614 ns/op	 2232082 B/op	   32000 allocs/op
// BenchmarkDesensitizeTest/original10000-4      	      38	  33511101 ns/op	22328585 B/op	  320000 allocs/op
// 新
// BenchmarkDesensitizeTest/original10-4         	  103401	     13123 ns/op	    9120 B/op	     120 allocs/op
// BenchmarkDesensitizeTest/original100-4        	   10225	    115931 ns/op	   91200 B/op	    1200 allocs/op
// BenchmarkDesensitizeTest/original1000-4       	     984	   1166139 ns/op	  912021 B/op	   12000 allocs/op
// BenchmarkDesensitizeTest/original10000-4      	      85	  14331049 ns/op	 9121948 B/op	  120000 allocs/op

// Case 2: content中有两处需要脱敏，处理时间减少64.60%，内存占用减少64.82%
// 旧
// BenchmarkDesensitizeTest/original10-4         	   18672	     63168 ns/op	   57760 B/op	     660 allocs/op
// BenchmarkDesensitizeTest/original100-4        	    1874	    637405 ns/op	  577606 B/op	    6600 allocs/op
// BenchmarkDesensitizeTest/original1000-4       	     178	   6475453 ns/op	 5776389 B/op	   66000 allocs/op
// BenchmarkDesensitizeTest/original10000-4      	      13	  89868205 ns/op	57809656 B/op	  660001 allocs/op
// 新
// BenchmarkDesensitizeTest/original10-4         	   54200	     25223 ns/op	   20320 B/op	     230 allocs/op
// BenchmarkDesensitizeTest/original100-4        	    4932	    224786 ns/op	  203201 B/op	    2300 allocs/op
// BenchmarkDesensitizeTest/original1000-4       	     536	   2267414 ns/op	 2032069 B/op	   23000 allocs/op
// BenchmarkDesensitizeTest/original10000-4      	      43	  28211592 ns/op	20327549 B/op	  230000 allocs/op

// Case 3: content中有三处需要脱敏，处理时间减少70.85%，内存占用减少68.12%
// 旧
// BenchmarkDesensitizeTest/original10-4         	    7843	    135498 ns/op	  103761 B/op	     990 allocs/op
// BenchmarkDesensitizeTest/original100-4        	     951	   1130952 ns/op	 1037651 B/op	    9900 allocs/op
// BenchmarkDesensitizeTest/original1000-4       	      97	  11766097 ns/op	10380511 B/op	   99000 allocs/op
// BenchmarkDesensitizeTest/original10000-4      	       8	 140325966 ns/op	104300582 B/op	  990001 allocs/op
// 新
// BenchmarkDesensitizeTest/original10-4         	   34443	     38427 ns/op	   33120 B/op	     340 allocs/op
// BenchmarkDesensitizeTest/original100-4        	    3558	    334694 ns/op	  331204 B/op	    3400 allocs/op
// BenchmarkDesensitizeTest/original1000-4       	     348	   3407212 ns/op	 3312291 B/op	   34000 allocs/op
// BenchmarkDesensitizeTest/original10000-4      	      28	  41672304 ns/op	33154506 B/op	  340000 allocs/op

// Case 4: content中有10处需要脱敏，处理时间减少84.43%，内存占用减少78.09%
// 旧
// BenchmarkDesensitizeTest/original10-4         	    1558	    879456 ns/op	  815054 B/op	    3340 allocs/op
// BenchmarkDesensitizeTest/original100-4        	     152	   7863173 ns/op	 8151517 B/op	   33400 allocs/op
// BenchmarkDesensitizeTest/original1000-4       	      13	  78217830 ns/op	81629931 B/op	  334001 allocs/op
// BenchmarkDesensitizeTest/original10000-4      	       2	 867478952 ns/op	823203064 B/op	 3340008 allocs/op
// 新
// BenchmarkDesensitizeTest/original10-4         	    9744	    133285 ns/op	  178562 B/op	    1110 allocs/op
// BenchmarkDesensitizeTest/original100-4        	     916	   1214273 ns/op	 1785769 B/op	   11100 allocs/op
// BenchmarkDesensitizeTest/original1000-4       	      92	  12256439 ns/op	17872064 B/op	  111000 allocs/op
// BenchmarkDesensitizeTest/original10000-4      	       8	 138718380 ns/op	180400550 B/op	 1110001 allocs/op

type MockParam struct {
	name         string
	num          int
	separator    string
	separatorReg string
	mockFunc     func(int, string, string) []*protocol.Log
}

var params = []MockParam{
	{"original", 10, " ", "\\s", mockData},
	{"original", 100, " ", "\\s", mockData},
	{"original", 1000, " ", "\\s", mockData},
	{"original", 10000, " ", "\\s", mockData},
}

func mockData(num int, separator, separatorReg string) []*protocol.Log {
	Logs := []*protocol.Log{}

	for i := 0; i < num; i++ {
		log := new(protocol.Log)
		log.Contents = make([]*protocol.Log_Content, 1)

		// case 1
		// log.Contents[0] = &protocol.Log_Content{Key: "content", Value: "[{'account':'1812213231432969','password':'04a23f38'}]"}
		// case 2
		// log.Contents[0] = &protocol.Log_Content{Key: "content", Value: "[{'account':'1812213231432969','password':'04a23f38'}, {'account':'1812213685634','password':'123a'}]"}
		// case 3
		// log.Contents[0] = &protocol.Log_Content{Key: "content", Value: "[{'account':'1812213231432969','password':'04a23f38'}, {'account':'1812213685634','password':'123a'}, {'account':'1812213685634','password':'666777888ccc'}]"}
		// case 4
		log.Contents[0] = &protocol.Log_Content{Key: "content", Value: "[{'account':'1812213231432969','password':'04a23f38'}, {'account':'1812213685634','password':'123a'}, {'account':'1812213685634','password':'666777888ccc'}, {'account':'1812213685634','password':'123a'}, {'account':'1812213685634','password':'666777888ccc'}, {'account':'1812213685634','password':'123a'}, {'account':'1812213685634','password':'666777888ccc'}, {'account':'1812213685634','password':'123a'}, {'account':'1812213685634','password':'666777888ccc'}, {'account':'1812213685634','password':'666777888ccc'}]"}

		Logs = append(Logs, log)
	}
	return Logs
}

func BenchmarkDesensitizeTest(b *testing.B) {
	for _, param := range params {
		b.Run(param.name+strconv.Itoa(param.num), func(b *testing.B) {
			Logs := param.mockFunc(param.num, param.separator, param.separatorReg)
			processor := newProcessor()

			processor.RegexBegin = "'password':'"
			processor.RegexContent = "[^']*"

			processor.Init(mock.NewEmptyContext("p", "l", "c"))
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				processor.ProcessLogs(Logs)
			}
			b.StopTimer()
		})
	}
}
