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
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

const (
	RawData = "cpu.load.short,host=server01,region=cn value=0.64"
)

func TestInitMetadataGroupAggregator(t *testing.T) {
	Convey("Given a metadata group aggregator without GroupMetadataKeys", t, func() {
		agg := NewAggregatorMetadataGroup()
		Convey("When Init(), should return error", func() {
			_, err := agg.Init(nil)
			So(err, ShouldBeNil)
		})
	})

	Convey("Given a metadata group aggregator with valid GroupKeys", t, func() {
		agg := NewAggregatorMetadataGroup()
		agg.GroupMetadataKeys = []string{"testB", "testA"}
		Convey("When Init(), should not return error", func() {
			_, err := agg.Init(nil)
			So(err, ShouldBeNil)
		})
	})

	Convey("Given a metadata group aggregator with invalid GroupMaxEventLength", t, func() {
		agg := NewAggregatorMetadataGroup()
		agg.GroupMaxEventLength = -1
		Convey("When Init(), should return error", func() {
			_, err := agg.Init(nil)
			So(err, ShouldNotBeNil)
		})
	})

	Convey("Given a metadata group aggregator with invalid GroupMaxByteLength", t, func() {
		agg := NewAggregatorMetadataGroup()
		agg.GroupMaxByteLength = -1
		Convey("When Init(), should return error", func() {
			_, err := agg.Init(nil)
			So(err, ShouldNotBeNil)
		})
	})

	Convey("Given a metadata group aggregator with invalid GroupMaxByteLength", t, func() {
		agg := NewAggregatorMetadataGroup()
		agg.GroupMaxByteLength = -1
		Convey("When Init(), should return error", func() {
			_, err := agg.Init(nil)
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
			ctx := helper.NewObservePipelineContext(100)
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
			ctx := helper.NewObservePipelineContext(100)
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
			ctx := helper.NewObservePipelineContext(100)
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
			ctx := helper.NewObservePipelineContext(100)
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
	for i := 0; i < count; i++ {
		groupEvents := &models.PipelineGroupEvents{Group: models.NewGroup(models.NewMetadataWithMap(meta), nil),
			Events: []models.PipelineEvent{event}}
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
			ctx := helper.NewObservePipelineContext(100)
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

func TestMetadataGroupGroup_Record_Directly(t *testing.T) {
	p := NewAggregatorMetadataGroup()
	p.GroupMaxEventLength = 5
	ctx := helper.NewObservePipelineContext(100)

	err := p.Record(constructEvents(104, map[string]string{}, map[string]string{}), ctx)
	require.NoError(t, err)
	require.Equal(t, 20, len(ctx.Collector().ToArray()))
}

func TestMetadataGroupGroup_Record_Tag(t *testing.T) {
	p := NewAggregatorMetadataGroup()
	p.GroupMaxEventLength = 5
	p.GroupMetadataKeys = []string{"meta"}
	events := constructEvents(104, map[string]string{
		"meta": "metaval",
	}, map[string]string{
		"tag": "tagval",
	})
	ctx := helper.NewObservePipelineContext(100)
	p.Init(mock.NewEmptyContext("a", "b", "c"))
	err := p.Record(events, ctx)
	require.NoError(t, err)
	array := ctx.Collector().ToArray()
	require.Equal(t, 20, len(array))
	require.Equal(t, 1, len(array[0].Group.Metadata.Iterator()))
	require.Equal(t, "metaval", array[0].Group.Metadata.Get("meta"))
}

func TestMetadataGroupGroup_Record_Timer(t *testing.T) {
	p := NewAggregatorMetadataGroup()
	p.GroupMaxEventLength = 500
	ctx := helper.NewObservePipelineContext(100)
	p.Init(mock.NewEmptyContext("a", "b", "c"))
	err := p.Record(constructEvents(104, map[string]string{}, map[string]string{}), ctx)
	require.NoError(t, err)
	require.Equal(t, 0, len(ctx.Collector().ToArray()))
	_ = p.GetResult(ctx)
	array := ctx.Collector().ToArray()
	require.Equal(t, 1, len(array))
	require.Equal(t, 104, len(array[0].Events))
}

func TestMetadataGroupGroup_Oversize(t *testing.T) {
	{
		p := NewAggregatorMetadataGroup()
		p.GroupMaxEventLength = 500
		p.GroupMaxByteLength = 1
		p.DropOversizeEvent = true
		ctx := helper.NewObservePipelineContext(100)
		p.Init(mock.NewEmptyContext("a", "b", "c"))

		events := generateByteArrayEvents(5, map[string]string{"a": "1", "b": "2", "c": "3"})
		for _, event := range events {
			require.NoError(t, p.Record(event, ctx))
		}
		require.Equal(t, 0, len(ctx.Collector().ToArray()))
		_ = p.GetResult(ctx)
		require.Equal(t, 0, len(ctx.Collector().ToArray()))
	}
	{
		p := NewAggregatorMetadataGroup()
		p.GroupMaxEventLength = 500
		p.GroupMaxByteLength = 1
		p.DropOversizeEvent = false
		ctx := helper.NewObservePipelineContext(100)
		p.Init(mock.NewEmptyContext("a", "b", "c"))

		events := generateByteArrayEvents(5, map[string]string{"a": "1", "b": "2", "c": "3"})
		for _, event := range events {
			require.NoError(t, p.Record(event, ctx))
		}
		require.Equal(t, 5, len(ctx.Collector().ToArray()))
		_ = p.GetResult(ctx)
		require.Equal(t, 0, len(ctx.Collector().ToArray()))
	}

}

func constructEvents(num int, meta, tags map[string]string) *models.PipelineGroupEvents {
	group := models.NewGroup(models.NewMetadataWithMap(meta), models.NewTagsWithMap(tags))
	e := new(models.PipelineGroupEvents)
	e.Group = group
	for i := 0; i < num; i++ {
		e.Events = append(e.Events, &models.Metric{})
	}
	return e
}

func TestMetadataGroupMixedEventsRespectByteLimitAndKeepInput(t *testing.T) {
	for _, drop := range []bool{true, false} {
		aggregator := NewAggregatorMetadataGroup()
		aggregator.GroupMaxByteLength = 3
		aggregator.DropOversizeEvent = drop
		_, err := aggregator.Init(mock.NewEmptyContext("project", "store", "config"))
		require.NoError(t, err)
		ctx := helper.NewObservePipelineContext(10)
		info := models.NewGroup(nil, models.NewTagsWithKeyValues("group", "tag"))
		metric := models.NewSingleValueMetric("cpu", models.MetricTypeGauge, models.NewTags(), 1, 1)
		log := models.NewSimpleLog([]byte("message"), models.NewTags(), 2)
		input := &models.PipelineGroupEvents{Group: info, Events: []models.PipelineEvent{models.ByteArray("a"), metric, models.ByteArray("abcd"), log}}
		require.NoError(t, aggregator.Record(input, ctx))
		require.NoError(t, aggregator.GetResult(ctx))
		require.Len(t, input.Events, 4)
		var events []models.PipelineEvent
		groups := ctx.Collector().ToArray()
		if !drop {
			require.Len(t, groups, 3)
			require.Equal(t, []models.PipelineEvent{models.ByteArray("abcd")}, groups[1].Events)
		}
		for _, group := range groups {
			require.Equal(t, "tag", group.Group.GetTags().Get("group"))
			if len(group.Events) == 1 {
				if raw, ok := group.Events[0].(models.ByteArray); ok {
					require.Equal(t, "abcd", string(raw))
				}
			}
			events = append(events, group.Events...)
		}
		if drop {
			require.Equal(t, []models.PipelineEvent{models.ByteArray("a"), metric, log}, events)
		} else {
			require.Equal(t, input.Events, events)
		}
	}
}

func TestMetadataGroupKeepsDestinationsAndLogFirstByteLimit(t *testing.T) {
	aggregator := NewAggregatorMetadataGroup()
	aggregator.GroupMetadataKeys = []string{"source"}
	aggregator.GroupMaxByteLength = 2
	_, err := aggregator.Init(mock.NewEmptyContext("project", "store", "config"))
	require.NoError(t, err)
	ctx := helper.NewObservePipelineContext(10)
	for _, store := range []string{"one", "two"} {
		info := models.NewGroup(models.NewMetadataWithKeyValues("source", "pack"), models.NewTagsWithKeyValues("host", "node"))
		info.DestinationLogstore = store
		log := models.NewSimpleLog([]byte(store), models.NewTags(), 1)
		input := &models.PipelineGroupEvents{Group: info, Events: []models.PipelineEvent{log, models.ByteArray("too large")}}
		require.NoError(t, aggregator.Record(input, ctx))
		require.Len(t, input.Events, 2)
	}
	require.NoError(t, aggregator.GetResult(ctx))
	groups := ctx.Collector().ToArray()
	require.Len(t, groups, 2)
	for _, group := range groups {
		require.Len(t, group.Events, 1)
		require.Equal(t, group.Group.GetDestinationLogstore(), string(group.Events[0].(*models.Log).GetBody()))
		require.Equal(t, "pack", group.Group.GetMetadata().Get("source"))
		require.Equal(t, "node", group.Group.GetTags().Get("host"))
	}
}
