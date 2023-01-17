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

package metadatagroup

import (
	"testing"

	. "github.com/smartystreets/goconvey/convey"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/models"
)

const (
	RawData = "cpu.load.short,host=server01,region=cn value=0.64"
)

func TestInitMetadataGroupAggregator(t *testing.T) {
	Convey("Given a metadata group aggregator without GroupMetadataKeys", t, func() {
		agg := NewAggregatorMetadataGroup()
		Convey("When Init(), should return error", func() {
			_, err := agg.Init(nil, nil)
			So(err, ShouldNotBeNil)
		})
	})

	Convey("Given a metadata group aggregator with valid GroupKeys", t, func() {
		agg := NewAggregatorMetadataGroup()
		agg.GroupMetadataKeys = []string{"testB", "testA"}
		Convey("When Init(), should not return error", func() {
			_, err := agg.Init(nil, nil)
			So(err, ShouldBeNil)
		})
	})

	Convey("Given a metadata group aggregator with invalid GroupMaxEventLength", t, func() {
		agg := NewAggregatorMetadataGroup()
		agg.GroupMaxEventLength = -1
		Convey("When Init(), should return error", func() {
			_, err := agg.Init(nil, nil)
			So(err, ShouldNotBeNil)
		})
	})

	Convey("Given a metadata group aggregator with invalid GroupMaxByteLength", t, func() {
		agg := NewAggregatorMetadataGroup()
		agg.GroupMaxByteLength = -1
		Convey("When Init(), should return error", func() {
			_, err := agg.Init(nil, nil)
			So(err, ShouldNotBeNil)
		})
	})

	Convey("Given a metadata group aggregator with invalid GroupMaxByteLength", t, func() {
		agg := NewAggregatorMetadataGroup()
		agg.GroupMaxByteLength = -1
		Convey("When Init(), should return error", func() {
			_, err := agg.Init(nil, nil)
			So(err, ShouldNotBeNil)
		})
	})
}

func TestDefaultMetadataGroupAggregator(t *testing.T) {
	Convey("Given a default metadata group aggregator", t, func() {
		agg := NewAggregatorMetadataGroup()
		So(agg.GroupMaxEventLength, ShouldEqual, maxEventsLength)
		So(agg.GroupMaxByteLength, ShouldEqual, maxBytesLength)
	})
}

func TestMetadataGroupAggregatorRecord(t *testing.T) {
	Convey("Given a metadata group aggregator with GroupMetadataKeys:[a, b]", t, func() {
		agg := NewAggregatorMetadataGroup()
		agg.GroupMetadataKeys = []string{"a", "b"}

		Convey("record events with ByteArray, when byte length exceeds GroupMaxByteLength", func() {
			agg.GroupMaxByteLength = len([]byte(RawData)) * 5
			ctx := ilogtail.NewObservePipelineConext(100)
			groupEvent := generateByteArrayEvents(5, map[string]string{"a": "1", "b": "2", "c": "3"})
			for _, group := range groupEvent {
				err := agg.Record(group, ctx)
				So(err, ShouldBeNil)
			}
			result := ctx.Collector().ToArray()
			So(len(result), ShouldEqual, 0)

			groupEvent = generateByteArrayEvents(4, map[string]string{"a": "1", "b": "1", "c": "3"})
			for _, group := range groupEvent {
				err := agg.Record(group, ctx)
				So(err, ShouldBeNil)
			}
			result = ctx.Collector().ToArray()
			So(len(result), ShouldEqual, 0)

			groupEvent = generateByteArrayEvents(5, map[string]string{"a": "1", "b": "2", "c": "3"})
			for _, group := range groupEvent {
				err := agg.Record(group, ctx)
				So(err, ShouldBeNil)
			}
			result = ctx.Collector().ToArray()
			So(len(result), ShouldEqual, 1)
			So(len(result[0].Events), ShouldEqual, 5)
			So(result[0].Group.GetMetadata().Get("a"), ShouldEqual, "1")
			So(result[0].Group.GetMetadata().Get("b"), ShouldEqual, "2")
			So(result[0].Group.GetMetadata().Get("c"), ShouldBeEmpty)
		})

		Convey("record with metric events,when events length exceeds GroupMaxEventLength", func() {
			agg.GroupMaxEventLength = 5
			ctx := ilogtail.NewObservePipelineConext(100)
			groupEvent := generateMetricEvents(5, map[string]string{"a": "1", "b": "2", "c": "3"})
			for _, group := range groupEvent {
				err := agg.Record(group, ctx)
				So(err, ShouldBeNil)
			}
			result := ctx.Collector().ToArray()
			So(len(result), ShouldEqual, 0)

			groupEvent = generateMetricEvents(5, map[string]string{"a": "1", "b": "1", "c": "3"})
			for _, group := range groupEvent {
				err := agg.Record(group, ctx)
				So(err, ShouldBeNil)
			}
			result = ctx.Collector().ToArray()
			So(len(result), ShouldEqual, 0)

			groupEvent = generateMetricEvents(5, map[string]string{"a": "1", "b": "2", "c": "3"})
			for _, group := range groupEvent {
				err := agg.Record(group, ctx)
				So(err, ShouldBeNil)
			}
			result = ctx.Collector().ToArray()
			So(len(result), ShouldEqual, 1)
			So(len(result[0].Events), ShouldEqual, 5)
			So(result[0].Group.GetMetadata().Get("a"), ShouldEqual, "1")
			So(result[0].Group.GetMetadata().Get("b"), ShouldEqual, "2")
			So(result[0].Group.GetMetadata().Get("c"), ShouldBeEmpty)
		})
	})
}

func TestMetadataGroupAggregatorRecordWithNonexistentKey(t *testing.T) {
	Convey("Given a metadata group aggregator with nonexistent GroupMetadataKeys", t, func() {
		agg := NewAggregatorMetadataGroup()
		agg.GroupMetadataKeys = []string{"nonexistentKEY"}

		Convey("record events with ByteArray, when metadata key not existed: all groups aggregate in one aggregator", func() {
			ctx := ilogtail.NewObservePipelineConext(100)
			agg.GroupMaxByteLength = len([]byte(RawData)) * 5
			groups := generateByteArrayEvents(5, map[string]string{"a": "1"})
			for _, group := range groups {
				err := agg.Record(group, ctx)
				So(err, ShouldBeNil)
			}
			result := ctx.Collector().ToArray()
			So(len(result), ShouldEqual, 0)

			groups = generateByteArrayEvents(4, map[string]string{"b": "1"})
			for _, group := range groups {
				err := agg.Record(group, ctx)
				So(err, ShouldBeNil)
			}
			result = ctx.Collector().ToArray()
			So(len(result), ShouldEqual, 1)
			So(len(result[0].Events), ShouldEqual, 5)
			target := result[0].Group.Metadata
			So(target.Contains("a"), ShouldEqual, false)
			So(target.Contains("b"), ShouldEqual, false)
			So(target.Contains("nonexistentKEY"), ShouldEqual, true)
			So(target.Get("nonexistentKEY"), ShouldEqual, "")

		})

		Convey("record with metric events,when events length exceeds GroupMaxEventLength", func() {
			agg.GroupMaxEventLength = 5
			ctx := ilogtail.NewObservePipelineConext(100)
			groupEvent := generateMetricEvents(5, map[string]string{"a": "1"})
			for _, group := range groupEvent {
				err := agg.Record(group, ctx)
				So(err, ShouldBeNil)
			}
			result := ctx.Collector().ToArray()
			So(len(result), ShouldEqual, 0)

			groupEvent = generateMetricEvents(4, map[string]string{"b": "1"})
			for _, group := range groupEvent {
				err := agg.Record(group, ctx)
				So(err, ShouldBeNil)
			}
			result = ctx.Collector().ToArray()
			So(len(result), ShouldEqual, 1)
			So(len(result[0].Events), ShouldEqual, 5)
			target := result[0].Group.Metadata
			So(target.Contains("a"), ShouldEqual, false)
			So(target.Contains("b"), ShouldEqual, false)
			So(target.Contains("nonexistentKEY"), ShouldEqual, true)
			So(target.Get("nonexistentKEY"), ShouldEqual, "")
		})
	})
}

func generateByteArrayEvents(count int, meta map[string]string) []*models.PipelineGroupEvents {
	groupEventsArray := make([]*models.PipelineGroupEvents, 0, count)
	event := models.ByteArray(RawData)
	groupEvents := &models.PipelineGroupEvents{Group: models.NewGroup(models.NewMetadataWithMap(meta), nil),
		Events: []models.PipelineEvent{event}}
	for i := 0; i < count; i++ {
		groupEventsArray = append(groupEventsArray, groupEvents)
	}
	return groupEventsArray
}

func generateMetricEvents(count int, meta map[string]string) []*models.PipelineGroupEvents {
	groupEventsArray := make([]*models.PipelineGroupEvents, 0, count)
	event := models.Metric{Name: "cpu.load.short", Tags: models.NewTagsWithKeyValues("host", "server01", "region", "cn"),
		MetricType: models.MetricTypeGauge, Value: &models.MetricSingleValue{Value: 0.64}}
	groupEvents := &models.PipelineGroupEvents{Group: models.NewGroup(models.NewMetadataWithMap(meta), nil),
		Events: []models.PipelineEvent{&event}}
	for i := 0; i < count; i++ {
		groupEventsArray = append(groupEventsArray, groupEvents)
	}
	return groupEventsArray
}

func TestMetadataGroupAggregatorGetResult(t *testing.T) {
	Convey("Given a metadata group aggregator with GroupMetadataKeys:[a, b]", t, func() {
		agg := NewAggregatorMetadataGroup()
		agg.GroupMetadataKeys = []string{"a", "b"}

		Convey("record ByteArray Events, then GetResult", func() {
			ctx := ilogtail.NewObservePipelineConext(100)
			groupEvent := generateByteArrayEvents(5, map[string]string{"a": "1", "b": "2", "c": "3"})
			groupEvent = append(groupEvent,
				generateByteArrayEvents(5, map[string]string{"a": "1", "b": "1", "c": "3"})...)
			for _, group := range groupEvent {
				err := agg.Record(group, ctx)
				So(err, ShouldBeNil)
			}
			err := agg.GetResult(ctx)
			So(err, ShouldBeNil)
			result := ctx.Collector().ToArray()
			So(len(result), ShouldEqual, 2)
			for _, r := range result {
				So(len(r.Events), ShouldEqual, 5)
			}
		})

	})
}
