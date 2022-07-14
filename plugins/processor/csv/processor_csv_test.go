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

package csv

import (
	"testing"

	. "github.com/smartystreets/goconvey/convey"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newProcessor() (*ProcessorCSVDecoder, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorCSVDecoder{
		SplitSep:  ",",
		SplitKeys: []string{"f1", "f2", "f3"},
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestProcessorCSVDecoder(t *testing.T) {
	Convey("Given a csv decoder with 3 DstKeys, and without preserving others", t, func() {
		processor, err := newProcessor()
		So(err, ShouldBeNil)

		Convey("When the src record has zero value", func() {
			record := ""
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is valid", func() {
				So(res, ShouldBeTrue)
				So(len(log.Contents), ShouldEqual, 2)
				So(log.Contents[1].Value, ShouldEqual, "")
			})
		})

		Convey("When the src record contains purely blank chars", func() {
			record := "  "
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is valid with warning", func() {
				So(res, ShouldBeTrue)
				So(len(log.Contents), ShouldEqual, 2)
				So(log.Contents[1].Value, ShouldEqual, "  ")
			})
		})

		Convey("When the src record has only a single field", func() {
			record := "12"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is valid with warning", func() {
				So(res, ShouldBeTrue)
				So(len(log.Contents), ShouldEqual, 2)
				So(log.Contents[1].Value, ShouldEqual, "12")
			})
		})

		Convey("When the src record fits the schema", func() {
			record := "12,34,56"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is valid", func() {
				So(res, ShouldBeTrue)
				So(len(log.Contents), ShouldEqual, 4)
				So(log.Contents[1].Key, ShouldEqual, "f1")
				So(log.Contents[1].Value, ShouldEqual, "12")
				So(log.Contents[2].Key, ShouldEqual, "f2")
				So(log.Contents[2].Value, ShouldEqual, "34")
				So(log.Contents[3].Key, ShouldEqual, "f3")
				So(log.Contents[3].Value, ShouldEqual, "56")
			})
		})

		Convey("When the src record fits the schema and all fields are properly quoted", func() {
			record := "\"normal\",\"\"\"quote\"\"\",\",\""
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is valid", func() {
				So(res, ShouldBeTrue)
				So(len(log.Contents), ShouldEqual, 4)
				So(log.Contents[1].Value, ShouldEqual, "normal")
				So(log.Contents[2].Value, ShouldEqual, "\"quote\"")
				So(log.Contents[3].Value, ShouldEqual, ",")
			})
		})

		Convey("When #src record fields < #DstKeys", func() {
			record := "12,34"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is valid with warning", func() {
				So(res, ShouldBeTrue)
				So(len(log.Contents), ShouldEqual, 3)
				So(log.Contents[1].Value, ShouldEqual, "12")
				So(log.Contents[2].Value, ShouldEqual, "34")
			})
		})

		Convey("When #src record fields > #DstKeys", func() {
			record := "12,34,56,78,90"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is valid", func() {
				So(res, ShouldBeTrue)
				So(len(log.Contents), ShouldEqual, 4)
				So(log.Contents[1].Value, ShouldEqual, "12")
				So(log.Contents[2].Value, ShouldEqual, "34")
				So(log.Contents[3].Value, ShouldEqual, "56")
			})
		})

		Convey("When the src record includes json", func() {
			record := "\"  words{\"\"a\"\":123,\"\"b\"\":\"\"string\"\",\"\"c\"\":[1,2,3],\"\"d\"\":{\"\"e\"\":\"\"string\"\"}}  \""
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is valid", func() {
				So(res, ShouldBeTrue)
				So(len(log.Contents), ShouldEqual, 2)
				So(log.Contents[1].Value, ShouldEqual, "  words{\"a\":123,\"b\":\"string\",\"c\":[1,2,3],\"d\":{\"e\":\"string\"}}  ")
			})
		})

		Convey("When the upper quotes in the src record has leading words", func() {
			record := "12\",34,56"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is invalid", func() {
				So(res, ShouldBeFalse)
			})
		})

		Convey("When the upper quotes in the src record has leading space", func() {
			record := "  \"12,34,56"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is invalid", func() {
				So(res, ShouldBeFalse)
			})
		})

		Convey("When bare quotes exist in the src record", func() {
			record := "\"12,34,56"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is invalid", func() {
				So(res, ShouldBeFalse)
			})
		})

		Convey("When the lower quotes in the src record has trailing words", func() {
			record := "12\",\"34,56"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is invalid", func() {
				So(res, ShouldBeFalse)
			})
		})

		Convey("When the lower quotes in the src record has trailing space", func() {
			record := "\"12\"  ,34,56"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is invalid", func() {
				So(res, ShouldBeFalse)
			})
		})
	})

	Convey("Given a csv decoder with 3 DstKeys, with preserving others but no expansion, and without trimming leading space", t, func() {
		processor, err := newProcessor()
		So(err, ShouldBeNil)
		processor.PreserveOthers = true

		Convey("When #src record fields > #DstKeys", func() {
			record := "12,34,56,78,90"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is valid with warning", func() {
				So(res, ShouldBeTrue)
				So(len(log.Contents), ShouldEqual, 5)
				So(log.Contents[1].Value, ShouldEqual, "12")
				So(log.Contents[2].Value, ShouldEqual, "34")
				So(log.Contents[3].Value, ShouldEqual, "56")
				So(log.Contents[4].Value, ShouldEqual, "78,90")
			})
		})

		Convey("When #src record fields > #DstKeys and all fields are properly quoted", func() {
			record := "12,34,56,\"normal\",\"\"\"quote\"\"\",\",\""
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is valid with warning", func() {
				So(res, ShouldBeTrue)
				So(len(log.Contents), ShouldEqual, 5)
				So(log.Contents[1].Value, ShouldEqual, "12")
				So(log.Contents[2].Value, ShouldEqual, "34")
				So(log.Contents[3].Value, ShouldEqual, "56")
				So(log.Contents[4].Value, ShouldEqual, "normal,\"\"\"quote\"\"\",\",\"")
			})
		})

		Convey("When the src record fields have leading space", func() {
			record := "  12,  ,\"  34\",  78,  ,\"  90\""
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is valid", func() {
				So(res, ShouldBeTrue)
				So(len(log.Contents), ShouldEqual, 5)
				So(log.Contents[1].Value, ShouldEqual, "  12")
				So(log.Contents[2].Value, ShouldEqual, "  ")
				So(log.Contents[3].Value, ShouldEqual, "  34")
				So(log.Contents[4].Value, ShouldEqual, "\"  78\",\"  \",\"  90\"")
			})
		})

		Convey("When the src record fields have trailing space", func() {
			record := "12  ,  ,\"34  \",78  ,  ,\"90  \""
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is valid", func() {
				So(res, ShouldBeTrue)
				So(len(log.Contents), ShouldEqual, 5)
				So(log.Contents[1].Value, ShouldEqual, "12  ")
				So(log.Contents[2].Value, ShouldEqual, "  ")
				So(log.Contents[3].Value, ShouldEqual, "34  ")
				So(log.Contents[4].Value, ShouldEqual, "78  ,\"  \",90  ")
			})
		})
	})

	Convey("Given a csv decoder with 3 DstKeys, with preserving others but no expansion, and with trimming leading space", t, func() {
		processor, err := newProcessor()
		So(err, ShouldBeNil)
		processor.PreserveOthers = true
		processor.TrimLeadingSpace = true

		Convey("When the src record contains purely blank chars", func() {
			record := "  "
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is valid", func() {
				So(res, ShouldBeTrue)
				So(len(log.Contents), ShouldEqual, 2)
				So(log.Contents[1].Value, ShouldEqual, "")
			})
		})

		Convey("When the src record fields have leading space", func() {
			record := "  12,  ,\"  34\",  78,  ,\"  90\""
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is valid", func() {
				So(res, ShouldBeTrue)
				So(len(log.Contents), ShouldEqual, 5)
				So(log.Contents[1].Value, ShouldEqual, "12")
				So(log.Contents[2].Value, ShouldEqual, "")
				So(log.Contents[3].Value, ShouldEqual, "  34")
				So(log.Contents[4].Value, ShouldEqual, "78,,\"  90\"")
			})
		})

		Convey("When the upper quotes in the src record has leading space", func() {
			record := "  \"12\",34,56"
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is valid", func() {
				So(res, ShouldBeTrue)
				So(len(log.Contents), ShouldEqual, 4)
				So(log.Contents[1].Value, ShouldEqual, "12")
				So(log.Contents[2].Value, ShouldEqual, "34")
				So(log.Contents[3].Value, ShouldEqual, "56")
			})
		})
	})

	Convey("Given a csv decoder with 3 DstKeys, with preserving others and expansion", t, func() {
		processor, err := newProcessor()
		So(err, ShouldBeNil)
		processor.PreserveOthers = true
		processor.ExpandOthers = true
		processor.ExpandKeyPrefix = "expand_"

		Convey("When #src record fields > #DstKeys", func() {
			record := "12,34,56,\"normal\",\"\"\"quote\"\"\",\",\""
			log := &protocol.Log{Time: 0}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "content", Value: record})
			res := processor.decodeCSV(log, record)

			Convey("Then the decoding is valid", func() {
				So(res, ShouldBeTrue)
				So(len(log.Contents), ShouldEqual, 7)
				So(log.Contents[1].Value, ShouldEqual, "12")
				So(log.Contents[2].Value, ShouldEqual, "34")
				So(log.Contents[3].Value, ShouldEqual, "56")
				So(log.Contents[4].Key, ShouldEqual, "expand_1")
				So(log.Contents[4].Value, ShouldEqual, "normal")
				So(log.Contents[5].Key, ShouldEqual, "expand_2")
				So(log.Contents[5].Value, ShouldEqual, "\"quote\"")
				So(log.Contents[6].Key, ShouldEqual, "expand_3")
				So(log.Contents[6].Value, ShouldEqual, ",")

			})
		})
	})
}
