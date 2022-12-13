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
	"testing"

	. "github.com/smartystreets/goconvey/convey"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newProcessor() (*ProcessorDesensitize, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorDesensitize{
		SourceKey:     "content",
		Method:        "const",
		Match:         "regex",
		ReplaceString: "***",
		RegexBegin:    "'password':'",
		RegexContent:  "[^']*",
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestProcessorDesensitizeInit(t *testing.T) {
	Convey("Test load Processor Desensitize.", t, func() {
		processor, err := newProcessor()
		So(err, ShouldBeNil)

		Convey("Test load success", func() {
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldBeNil)
			So(processor.Description(), ShouldEqual, "desensitize processor for logtail")
		})

		Convey("Test load fail with no SourceKey error", func() {
			processor.SourceKey = ""
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldResemble, errors.New("parameter SourceKey should not be empty"))
		})

		Convey("Test load fail with no Method error", func() {
			processor.Method = ""
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldResemble, errors.New("parameter Method should be \"const\" or \"md5\""))
		})

		Convey("Test load fail with wrong Method error", func() {
			processor.Method = "Base64"
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldResemble, errors.New("parameter Method should be \"const\" or \"md5\""))
		})

		Convey("Test load fail with wrong Match error", func() {
			processor.Match = "overwrite"
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldResemble, errors.New("parameter Match should be \"full\" or \"regex\""))
		})

		Convey("Test load fail with no RegexBegin error", func() {
			processor.RegexBegin = ""
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldResemble, errors.New("need parameter RegexBegin"))
		})

		Convey("Test load fail with wrong RegexBegin error", func() {
			processor.RegexBegin = "*"
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldNotBeNil)
		})

		Convey("Test load fail with no RegexContent error", func() {
			processor.RegexContent = ""
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldResemble, errors.New("need parameter RegexContent"))
		})

		Convey("Test load fail with wrong RegexContent error", func() {
			processor.RegexContent = "*"
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldNotBeNil)
		})

		Convey("Test load fail with no ReplaceString error", func() {
			processor.Method = "const"
			processor.ReplaceString = ""
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldResemble, errors.New("parameter ReplaceString should not be empty when Method is \"const\""))
		})
	})
}

func TestProcessorDesensitizeWork(t *testing.T) {
	Convey("Test Match = full.", t, func() {
		processor, err := newProcessor()
		So(err, ShouldBeNil)
		processor.Match = "full"
		err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("Test const", func() {
			record := "[{'account':'1812213231432969','password':'04a23f38'}, {'account':'1812213685634','password':'123a'}]"
			res := processor.desensitize(record)
			So(res, ShouldEqual, "***")
		})

		Convey("Test md5", func() {
			processor.Method = "md5"
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldBeNil)

			record := "[{'account':'1812213231432969','password':'04a23f38'}, {'account':'1812213685634','password':'123a'}]"
			res := processor.desensitize(record)
			So(res, ShouldEqual, "700085e3968c3efb83b54ba47dd1367d")
		})
	})

	Convey("Test Match = regex.", t, func() {
		processor, err := newProcessor()
		So(err, ShouldBeNil)
		err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("Test const", func() {
			record := "[{'account':'1812213231432969','password':'04a23f38'}, {'account':'1812213685634','password':'123a'}]"
			res := processor.desensitize(record)
			So(res, ShouldEqual, "[{'account':'1812213231432969','password':'***'}, {'account':'1812213685634','password':'***'}]")
		})

		processor.Method = "md5"
		err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		Convey("Test md5", func() {
			err = processor.Init(mock.NewEmptyContext("p", "l", "c"))
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
