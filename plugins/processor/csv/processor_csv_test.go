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

	"github.com/alibaba/ilogtail/plugins/processor/internal/processortest"

	. "github.com/smartystreets/goconvey/convey"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newProcessor() (*ProcessorCSVDecoder, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorCSVDecoder{
		SourceKey:  "content",
		KeepSource: true,
		SplitSep:   ",",
		SplitKeys:  []string{"f1", "f2", "f3"},
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestProcessorCSVDecoderInitRequiresSourceKey(t *testing.T) {
	processor := &ProcessorCSVDecoder{SplitSep: ","}
	require.ErrorContains(t, processor.Init(mock.NewEmptyContext("p", "l", "c")), "SourceKey")
}

func TestProcessorCSVDecoder(t *testing.T) {
	Convey("Given a csv decoder with 3 DstKeys, and without preserving others", t, func() {
		processor, err := newProcessor()
		So(err, ShouldBeNil)

		Convey("When the src record has zero value", func() {
			record := ""
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is valid", func() {
				So(log.Contents.Len(), ShouldEqual, 2)
				So(log.Contents.Get("f1"), ShouldEqual, "")
			})
		})

		Convey("When the src record contains purely blank chars", func() {
			record := "  "
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is valid with warning", func() {
				So(log.Contents.Len(), ShouldEqual, 2)
				So(log.Contents.Get("f1"), ShouldEqual, "  ")
			})
		})

		Convey("When the src record has only a single field", func() {
			record := "12"
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is valid with warning", func() {
				So(log.Contents.Len(), ShouldEqual, 2)
				So(log.Contents.Get("f1"), ShouldEqual, "12")
			})
		})

		Convey("When the src record fits the schema", func() {
			record := "12,34,56"
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is valid", func() {
				So(log.Contents.Len(), ShouldEqual, 4)

				So(log.Contents.Get("f1"), ShouldEqual, "12")

				So(log.Contents.Get("f2"), ShouldEqual, "34")

				So(log.Contents.Get("f3"), ShouldEqual, "56")
			})
		})

		Convey("When the src record fits the schema and all fields are properly quoted", func() {
			record := "\"normal\",\"\"\"quote\"\"\",\",\""
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is valid", func() {
				So(log.Contents.Len(), ShouldEqual, 4)
				So(log.Contents.Get("f1"), ShouldEqual, "normal")
				So(log.Contents.Get("f2"), ShouldEqual, "\"quote\"")
				So(log.Contents.Get("f3"), ShouldEqual, ",")
			})
		})

		Convey("When #src record fields < #DstKeys", func() {
			record := "12,34"
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is valid with warning", func() {
				So(log.Contents.Len(), ShouldEqual, 3)
				So(log.Contents.Get("f1"), ShouldEqual, "12")
				So(log.Contents.Get("f2"), ShouldEqual, "34")
			})
		})

		Convey("When #src record fields > #DstKeys", func() {
			record := "12,34,56,78,90"
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is valid", func() {
				So(log.Contents.Len(), ShouldEqual, 4)
				So(log.Contents.Get("f1"), ShouldEqual, "12")
				So(log.Contents.Get("f2"), ShouldEqual, "34")
				So(log.Contents.Get("f3"), ShouldEqual, "56")
			})
		})

		Convey("When the src record includes json", func() {
			record := "\"  words{\"\"a\"\":123,\"\"b\"\":\"\"string\"\",\"\"c\"\":[1,2,3],\"\"d\"\":{\"\"e\"\":\"\"string\"\"}}  \""
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is valid", func() {
				So(log.Contents.Len(), ShouldEqual, 2)
				So(log.Contents.Get("f1"), ShouldEqual, "  words{\"a\":123,\"b\":\"string\",\"c\":[1,2,3],\"d\":{\"e\":\"string\"}}  ")
			})
		})

		Convey("When the upper quotes in the src record has leading words", func() {
			record := "12\",34,56"
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is invalid", func() {
				So(log.Contents.Iterator(), ShouldResemble, map[string]interface{}{"content": record})
			})
		})

		Convey("When the upper quotes in the src record has leading space", func() {
			record := "  \"12,34,56"
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is invalid", func() {
				So(log.Contents.Iterator(), ShouldResemble, map[string]interface{}{"content": record})
			})
		})

		Convey("When bare quotes exist in the src record", func() {
			record := "\"12,34,56"
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is invalid", func() {
				So(log.Contents.Iterator(), ShouldResemble, map[string]interface{}{"content": record})
			})
		})

		Convey("When the lower quotes in the src record has trailing words", func() {
			record := "12\",\"34,56"
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is invalid", func() {
				So(log.Contents.Iterator(), ShouldResemble, map[string]interface{}{"content": record})
			})
		})

		Convey("When the lower quotes in the src record has trailing space", func() {
			record := "\"12\"  ,34,56"
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is invalid", func() {
				So(log.Contents.Iterator(), ShouldResemble, map[string]interface{}{"content": record})
			})
		})
	})

	Convey("Given a csv decoder with 3 DstKeys, with preserving others but no expansion, and without trimming leading space", t, func() {
		processor, err := newProcessor()
		So(err, ShouldBeNil)
		processor.PreserveOthers = true

		Convey("When #src record fields > #DstKeys", func() {
			record := "12,34,56,78,90"
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is valid with warning", func() {
				So(log.Contents.Len(), ShouldEqual, 5)
				So(log.Contents.Get("f1"), ShouldEqual, "12")
				So(log.Contents.Get("f2"), ShouldEqual, "34")
				So(log.Contents.Get("f3"), ShouldEqual, "56")
				So(log.Contents.Get("_decode_preserve_"), ShouldEqual, "78,90")
			})
		})

		Convey("When #src record fields > #DstKeys and all fields are properly quoted", func() {
			record := "12,34,56,\"normal\",\"\"\"quote\"\"\",\",\""
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is valid with warning", func() {
				So(log.Contents.Len(), ShouldEqual, 5)
				So(log.Contents.Get("f1"), ShouldEqual, "12")
				So(log.Contents.Get("f2"), ShouldEqual, "34")
				So(log.Contents.Get("f3"), ShouldEqual, "56")
				So(log.Contents.Get("_decode_preserve_"), ShouldEqual, "normal,\"\"\"quote\"\"\",\",\"")
			})
		})

		Convey("When the src record fields have leading space", func() {
			record := "  12,  ,\"  34\",  78,  ,\"  90\""
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is valid", func() {
				So(log.Contents.Len(), ShouldEqual, 5)
				So(log.Contents.Get("f1"), ShouldEqual, "  12")
				So(log.Contents.Get("f2"), ShouldEqual, "  ")
				So(log.Contents.Get("f3"), ShouldEqual, "  34")
				So(log.Contents.Get("_decode_preserve_"), ShouldEqual, "\"  78\",\"  \",\"  90\"")
			})
		})

		Convey("When the src record fields have trailing space", func() {
			record := "12  ,  ,\"34  \",78  ,  ,\"90  \""
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is valid", func() {
				So(log.Contents.Len(), ShouldEqual, 5)
				So(log.Contents.Get("f1"), ShouldEqual, "12  ")
				So(log.Contents.Get("f2"), ShouldEqual, "  ")
				So(log.Contents.Get("f3"), ShouldEqual, "34  ")
				So(log.Contents.Get("_decode_preserve_"), ShouldEqual, "78  ,\"  \",90  ")
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
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is valid", func() {
				So(log.Contents.Len(), ShouldEqual, 2)
				So(log.Contents.Get("f1"), ShouldEqual, "")
			})
		})

		Convey("When the src record fields have leading space", func() {
			record := "  12,  ,\"  34\",  78,  ,\"  90\""
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is valid", func() {
				So(log.Contents.Len(), ShouldEqual, 5)
				So(log.Contents.Get("f1"), ShouldEqual, "12")
				So(log.Contents.Get("f2"), ShouldEqual, "")
				So(log.Contents.Get("f3"), ShouldEqual, "  34")
				So(log.Contents.Get("_decode_preserve_"), ShouldEqual, "78,,\"  90\"")
			})
		})

		Convey("When the upper quotes in the src record has leading space", func() {
			record := "  \"12\",34,56"
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is valid", func() {
				So(log.Contents.Len(), ShouldEqual, 4)
				So(log.Contents.Get("f1"), ShouldEqual, "12")
				So(log.Contents.Get("f2"), ShouldEqual, "34")
				So(log.Contents.Get("f3"), ShouldEqual, "56")
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
			log := processortest.Log("content", record)
			processortest.Apply(processor, log)

			Convey("Then the decoding is valid", func() {
				So(log.Contents.Len(), ShouldEqual, 7)
				So(log.Contents.Get("f1"), ShouldEqual, "12")
				So(log.Contents.Get("f2"), ShouldEqual, "34")
				So(log.Contents.Get("f3"), ShouldEqual, "56")

				So(log.Contents.Get("expand_1"), ShouldEqual, "normal")

				So(log.Contents.Get("expand_2"), ShouldEqual, "\"quote\"")

				So(log.Contents.Get("expand_3"), ShouldEqual, ",")

			})
		})
	})
}

func TestProcessorCSVDecoder_ProcessSplits(t *testing.T) {
	processor := &ProcessorCSVDecoder{
		SourceKey: "content",
		SplitSep:  ",",
		SplitKeys: []string{"f1", "f2", "f3"},
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "12,34,56")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.Equal(t, "12", contents.Get("f1"))
	assert.Equal(t, "34", contents.Get("f2"))
	assert.Equal(t, "56", contents.Get("f3"))
	// KeepSource defaults to false, so the source key is removed on success.
	assert.False(t, contents.Contains("content"), "source key must be removed on successful decode")
}

func TestProcessorCSVDecoder_ProcessExpandOthers(t *testing.T) {
	processor := &ProcessorCSVDecoder{
		SourceKey:       "content",
		SplitSep:        ",",
		SplitKeys:       []string{"f1", "f2", "f3"},
		PreserveOthers:  true,
		ExpandOthers:    true,
		ExpandKeyPrefix: "expand_",
		KeepSource:      true,
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("content", "12,34,56,78,90")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	contents := log.GetIndices()
	assert.Equal(t, "12", contents.Get("f1"))
	assert.Equal(t, "34", contents.Get("f2"))
	assert.Equal(t, "56", contents.Get("f3"))
	assert.Equal(t, "78", contents.Get("expand_1"))
	assert.Equal(t, "90", contents.Get("expand_2"))
	assert.True(t, contents.Contains("content"), "source key must be kept when KeepSource is true")
}

func TestProcessorCSVDecoder_ProcessPassesThroughMetric(t *testing.T) {
	processor := &ProcessorCSVDecoder{
		SourceKey: "content",
		SplitSep:  ",",
		SplitKeys: []string{"f1"},
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}
