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

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper/platformmeta"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/test"
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
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "test_key", Value: "test_value"})
	processor.processLog(log)
	assert.Equal(t, "test_value", log.Contents[0].Value)
	assert.Equal(t, "|host#$#"+util.GetHostName()+"|ip#$#"+util.GetIPAddress()+"|env:"+os.Getenv("my")+"|switch#$#__cloud_image_id__", log.Contents[1].Value)
	processor.processLog(log)
	assert.Equal(t, "test_value", log.Contents[0].Value)
	assert.Equal(t, "|host#$#"+util.GetHostName()+"|ip#$#"+util.GetIPAddress()+"|env:"+os.Getenv("my")+"|switch#$#__cloud_image_id__"+"|host#$#"+util.GetHostName()+"|ip#$#"+util.GetIPAddress()+"|env:"+os.Getenv("my")+"|switch#$#__cloud_image_id__", log.Contents[1].Value)

	processor.SortLabels = true
	log.Contents[1].Value = ""
	processor.processLog(log)
	assert.Equal(t, "host#$#"+util.GetHostName()+"|ip#$#"+util.GetIPAddress()+"|switch#$#__cloud_image_id__", log.Contents[1].Value)
	log.Contents[1].Value = "z#$#x"
	processor.processLog(log)
	assert.Equal(t, "host#$#"+util.GetHostName()+"|ip#$#"+util.GetIPAddress()+"|switch#$#__cloud_image_id__"+"|z#$#x", log.Contents[1].Value)
}

func TestReadDynamic(t *testing.T) {
	log := &protocol.Log{Time: 0}
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAppender{
		Key:   "a",
		Value: "|__cloud_instance_id__#$#{{__cloud_instance_id__}}|__cloud_instance_name__#$#{{__cloud_instance_name__}}|__cloud_region__#$#{{__cloud_region__}}|__cloud_zone__#$#{{__cloud_zone__}}|__cloud_vpc_id__#$#{{__cloud_vpc_id__}}|__cloud_vswitch_id__#$#{{__cloud_vswitch_id__}}|__cloud_instance_type__#$#{{__cloud_instance_type__}}|__cloud_image_id__#$#{{__cloud_image_id__}}|__cloud_max_ingress__#$#{{__cloud_max_ingress__}}|__cloud_max_egress__#$#{{__cloud_max_egress__}}",
	}
	processor.Platform = platformmeta.Mock
	err := processor.Init(ctx)
	require.NoError(t, err)
	processor.processLog(log)
	require.Equal(t, test.ReadLogVal(log, "a"), "|__cloud_instance_id__#$#id_xxx|__cloud_instance_name__#$#name_xxx|__cloud_region__#$#region_xxx|__cloud_zone__#$#zone_xxx|__cloud_vpc_id__#$#vpc_xxx|__cloud_vswitch_id__#$#vswitch_xxx|__cloud_instance_type__#$#type_xxx|__cloud_image_id__#$#image_xxx|__cloud_max_ingress__#$#0|__cloud_max_egress__#$#0")

	atomic.AddInt64(&platformmeta.MockManagerNum, 100)
	log.Contents = log.Contents[:0]
	processor.processLog(log)
	require.Equal(t, test.ReadLogVal(log, "a"), "|__cloud_instance_id__#$#id_xxx|__cloud_instance_name__#$#name_xxx|__cloud_region__#$#region_xxx|__cloud_zone__#$#zone_xxx|__cloud_vpc_id__#$#vpc_xxx|__cloud_vswitch_id__#$#vswitch_xxx|__cloud_instance_type__#$#type_xxx|__cloud_image_id__#$#image_xxx|__cloud_max_ingress__#$#100|__cloud_max_egress__#$#1000")

}

func TestReadOnce(t *testing.T) {
	atomic.StoreInt64(&platformmeta.MockManagerNum, 0)
	log := &protocol.Log{Time: 0}
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAppender{
		Key:   "a",
		Value: "|__cloud_instance_id__#$#{{__cloud_instance_id__}}|__cloud_instance_name__#$#{{__cloud_instance_name__}}|__cloud_region__#$#{{__cloud_region__}}|__cloud_zone__#$#{{__cloud_zone__}}|__cloud_vpc_id__#$#{{__cloud_vpc_id__}}|__cloud_vswitch_id__#$#{{__cloud_vswitch_id__}}|__cloud_instance_type__#$#{{__cloud_instance_type__}}|__cloud_image_id__#$#{{__cloud_image_id__}}|__cloud_max_ingress__#$#{{__cloud_max_ingress__}}|__cloud_max_egress__#$#{{__cloud_max_egress__}}",
	}
	processor.ReadOnce = true
	processor.Platform = platformmeta.Mock
	err := processor.Init(ctx)
	require.NoError(t, err)
	processor.processLog(log)
	require.Equal(t, test.ReadLogVal(log, "a"), "|__cloud_instance_id__#$#id_xxx|__cloud_instance_name__#$#name_xxx|__cloud_region__#$#region_xxx|__cloud_zone__#$#zone_xxx|__cloud_vpc_id__#$#vpc_xxx|__cloud_vswitch_id__#$#vswitch_xxx|__cloud_instance_type__#$#type_xxx|__cloud_image_id__#$#image_xxx|__cloud_max_ingress__#$#0|__cloud_max_egress__#$#0")

	require.Equal(t, len(processor.replaceFuncs), 0)
	atomic.AddInt64(&platformmeta.MockManagerNum, 100)
	log.Contents = log.Contents[:0]
	processor.processLog(log)
	require.Equal(t, test.ReadLogVal(log, "a"), "|__cloud_instance_id__#$#id_xxx|__cloud_instance_name__#$#name_xxx|__cloud_region__#$#region_xxx|__cloud_zone__#$#zone_xxx|__cloud_vpc_id__#$#vpc_xxx|__cloud_vswitch_id__#$#vswitch_xxx|__cloud_instance_type__#$#type_xxx|__cloud_image_id__#$#image_xxx|__cloud_max_ingress__#$#0|__cloud_max_egress__#$#0")
}

func TestParameterCheck(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAppender{}
	err := processor.Init(ctx)
	assert.Error(t, err)
}
