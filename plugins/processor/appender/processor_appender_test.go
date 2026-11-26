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

package appender

import (
	"os"
	"sync/atomic"
	"testing"

	"github.com/alibaba/ilogtail/plugins/processor/internal/processortest"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/helper/platformmeta"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newProcessor() (*ProcessorAppender, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAppender{
		Key:   "a",
		Value: "|host#$#{{__host__}}|ip#$#{{__ip__}}|env:{{$my}}|switch#$#{{__cloud_image_id__}}",
	}
	err := processor.Init(ctx)
	return processor, err
}

func TestIgnoreIfExistTrue(t *testing.T) {
	_ = os.Setenv("my", "xxx")
	processor, _ := newProcessor()
	log := processortest.Log()
	log.Contents.Add("test_key", "test_value")
	processortest.Apply(processor, log)
	assert.Equal(t, "test_value", pipeline.GetStringValue(log.Contents.Get("test_key")))
	assert.Equal(t, "|host#$#"+util.GetHostName()+"|ip#$#"+util.GetIPAddress()+"|env:"+os.Getenv("my")+"|switch#$#__cloud_image_id__", pipeline.GetStringValue(log.Contents.Get("a")))
	processortest.Apply(processor, log)
	assert.Equal(t, "test_value", pipeline.GetStringValue(log.Contents.Get("test_key")))
	assert.Equal(t, "|host#$#"+util.GetHostName()+"|ip#$#"+util.GetIPAddress()+"|env:"+os.Getenv("my")+"|switch#$#__cloud_image_id__"+"|host#$#"+util.GetHostName()+"|ip#$#"+util.GetIPAddress()+"|env:"+os.Getenv("my")+"|switch#$#__cloud_image_id__", pipeline.GetStringValue(log.Contents.Get("a")))

	processor.SortLabels = true
	log.Contents.Add("a", "")
	processortest.Apply(processor, log)
	assert.Equal(t, "host#$#"+util.GetHostName()+"|ip#$#"+util.GetIPAddress()+"|switch#$#__cloud_image_id__", pipeline.GetStringValue(log.Contents.Get("a")))
	log.Contents.Add("a", "z#$#x")
	processortest.Apply(processor, log)
	assert.Equal(t, "host#$#"+util.GetHostName()+"|ip#$#"+util.GetIPAddress()+"|switch#$#__cloud_image_id__"+"|z#$#x", pipeline.GetStringValue(log.Contents.Get("a")))
}

func TestReadDynamic(t *testing.T) {
	log := processortest.Log()
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAppender{
		Key:   "a",
		Value: "|__cloud_instance_id__#$#{{__cloud_instance_id__}}|__cloud_instance_name__#$#{{__cloud_instance_name__}}|__cloud_region__#$#{{__cloud_region__}}|__cloud_zone__#$#{{__cloud_zone__}}|__cloud_vpc_id__#$#{{__cloud_vpc_id__}}|__cloud_vswitch_id__#$#{{__cloud_vswitch_id__}}|__cloud_instance_type__#$#{{__cloud_instance_type__}}|__cloud_image_id__#$#{{__cloud_image_id__}}|__cloud_max_ingress__#$#{{__cloud_max_ingress__}}|__cloud_max_egress__#$#{{__cloud_max_egress__}}",
	}
	processor.Platform = platformmeta.Mock
	err := processor.Init(ctx)
	require.NoError(t, err)
	processortest.Apply(processor, log)
	require.Equal(t, log.Contents.Get("a"), "|__cloud_instance_id__#$#id_xxx|__cloud_instance_name__#$#name_xxx|__cloud_region__#$#region_xxx|__cloud_zone__#$#zone_xxx|__cloud_vpc_id__#$#vpc_xxx|__cloud_vswitch_id__#$#vswitch_xxx|__cloud_instance_type__#$#type_xxx|__cloud_image_id__#$#image_xxx|__cloud_max_ingress__#$#0|__cloud_max_egress__#$#0")

	atomic.AddInt64(&platformmeta.MockManagerNum, 100)
	log = processortest.Log()
	processortest.Apply(processor, log)
	require.Equal(t, log.Contents.Get("a"), "|__cloud_instance_id__#$#id_xxx|__cloud_instance_name__#$#name_xxx|__cloud_region__#$#region_xxx|__cloud_zone__#$#zone_xxx|__cloud_vpc_id__#$#vpc_xxx|__cloud_vswitch_id__#$#vswitch_xxx|__cloud_instance_type__#$#type_xxx|__cloud_image_id__#$#image_xxx|__cloud_max_ingress__#$#100|__cloud_max_egress__#$#1000")

}

func TestReadOnce(t *testing.T) {
	atomic.StoreInt64(&platformmeta.MockManagerNum, 0)
	log := processortest.Log()
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAppender{
		Key:   "a",
		Value: "|__cloud_instance_id__#$#{{__cloud_instance_id__}}|__cloud_instance_name__#$#{{__cloud_instance_name__}}|__cloud_region__#$#{{__cloud_region__}}|__cloud_zone__#$#{{__cloud_zone__}}|__cloud_vpc_id__#$#{{__cloud_vpc_id__}}|__cloud_vswitch_id__#$#{{__cloud_vswitch_id__}}|__cloud_instance_type__#$#{{__cloud_instance_type__}}|__cloud_image_id__#$#{{__cloud_image_id__}}|__cloud_max_ingress__#$#{{__cloud_max_ingress__}}|__cloud_max_egress__#$#{{__cloud_max_egress__}}",
	}
	processor.ReadOnce = true
	processor.Platform = platformmeta.Mock
	err := processor.Init(ctx)
	require.NoError(t, err)
	processortest.Apply(processor, log)
	require.Equal(t, log.Contents.Get("a"), "|__cloud_instance_id__#$#id_xxx|__cloud_instance_name__#$#name_xxx|__cloud_region__#$#region_xxx|__cloud_zone__#$#zone_xxx|__cloud_vpc_id__#$#vpc_xxx|__cloud_vswitch_id__#$#vswitch_xxx|__cloud_instance_type__#$#type_xxx|__cloud_image_id__#$#image_xxx|__cloud_max_ingress__#$#0|__cloud_max_egress__#$#0")

	require.Equal(t, len(processor.replaceFuncs), 0)
	atomic.AddInt64(&platformmeta.MockManagerNum, 100)
	log = processortest.Log()
	processortest.Apply(processor, log)
	require.Equal(t, log.Contents.Get("a"), "|__cloud_instance_id__#$#id_xxx|__cloud_instance_name__#$#name_xxx|__cloud_region__#$#region_xxx|__cloud_zone__#$#zone_xxx|__cloud_vpc_id__#$#vpc_xxx|__cloud_vswitch_id__#$#vswitch_xxx|__cloud_instance_type__#$#type_xxx|__cloud_image_id__#$#image_xxx|__cloud_max_ingress__#$#0|__cloud_max_egress__#$#0")
}

func TestParameterCheck(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAppender{}
	err := processor.Init(ctx)
	assert.Error(t, err)
}

func newAppender(t *testing.T, key, value string) *ProcessorAppender {
	processor := pipeline.Processors["processor_appender"]().(*ProcessorAppender)
	processor.Key = key
	processor.Value = value
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
	return processor
}

func TestProcessorAppender_ProcessAppendsToExistingKey(t *testing.T) {
	processor := newAppender(t, "k", "_suffix")

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("k", "base")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.Equal(t, "base_suffix", log.GetIndices().Get("k"))
}

func TestProcessorAppender_ProcessCreatesMissingKey(t *testing.T) {
	processor := newAppender(t, "k", "_suffix")

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	assert.Equal(t, "_suffix", log.GetIndices().Get("k"))
}

func TestProcessorAppender_ProcessPassesThroughMetric(t *testing.T) {
	processor := newAppender(t, "k", "_suffix")

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}
