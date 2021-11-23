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
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newProcessor() (*ProcessorAppender, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAppender{
		Key:   "a",
		Value: "|host#$#{{__host__}}|ip#$#{{__ip__}}|env:{{$my}}",
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
	assert.Equal(t, "|host#$#"+util.GetHostName()+"|ip#$#"+util.GetIPAddress()+"|env:"+os.Getenv("my"), log.Contents[1].Value)
	processor.processLog(log)
	assert.Equal(t, "test_value", log.Contents[0].Value)
	assert.Equal(t, "|host#$#"+util.GetHostName()+"|ip#$#"+util.GetIPAddress()+"|env:"+os.Getenv("my")+"|host#$#"+util.GetHostName()+"|ip#$#"+util.GetIPAddress()+"|env:"+os.Getenv("my"), log.Contents[1].Value)

	processor.SortLabels = true
	log.Contents[1].Value = ""
	processor.processLog(log)
	assert.Equal(t, "host#$#"+util.GetHostName()+"|ip#$#"+util.GetIPAddress(), log.Contents[1].Value)
	log.Contents[1].Value = "z#$#x"
	processor.processLog(log)
	assert.Equal(t, "host#$#"+util.GetHostName()+"|ip#$#"+util.GetIPAddress()+"|z#$#x", log.Contents[1].Value)
}

func TestParameterCheck(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAppender{}
	err := processor.Init(ctx)
	assert.Error(t, err)
}
