// Copyright 2025 iLogtail Authors
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

package odps

import (
	"fmt"
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

func TestPartitionHelperInit(t *testing.T) {
	ph := NewPartitionHelper()

	config := "pt1=test1,pt2={{col1}},pt3=test_{{col2}},pt4={{%Y%m%d}},pt5=test_{{%H%M}}aaa"
	err := ph.Init(config, 15)
	fmt.Println(err)
	assert.Nil(t, err)

	assert.Equal(t, 15, ph.timeRange)
	assert.Equal(t, 9, len(ph.columns))

	fmt.Println(ph.columns)
	assert.Equal(t, Default, ph.columns[0].colType)
	assert.Equal(t, "pt1=test1,pt2=", ph.columns[0].format)
	assert.Equal(t, Data, ph.columns[1].colType)
	assert.Equal(t, "col1", ph.columns[1].format)
	assert.Equal(t, Default, ph.columns[2].colType)
	assert.Equal(t, ",pt3=test_", ph.columns[2].format)

	assert.Equal(t, Data, ph.columns[3].colType)
	assert.Equal(t, "col2", ph.columns[3].format)
	assert.Equal(t, Default, ph.columns[4].colType)
	assert.Equal(t, ",pt4=", ph.columns[4].format)
	assert.Equal(t, Time, ph.columns[5].colType)
	assert.Equal(t, "20060102", ph.columns[5].format)
	assert.Equal(t, Default, ph.columns[6].colType)
	assert.Equal(t, ",pt5=test_", ph.columns[6].format)
	assert.Equal(t, Time, ph.columns[7].colType)
	assert.Equal(t, "1504", ph.columns[7].format)
	assert.Equal(t, Default, ph.columns[8].colType)
	assert.Equal(t, "aaa", ph.columns[8].format)
}

func TestPartitionHelperInit2(t *testing.T) {
	ph := NewPartitionHelper()

	config := "ds={{%Y%m%d}},hh={{%H}},mm={{%M}},action={{method}}"
	err := ph.Init(config, 15)
	assert.Nil(t, err)

	fmt.Println(ph.columns)
}

func TestPartitionHelperInitFailed(t *testing.T) {
	ph := NewPartitionHelper()

	config := "pt1=test1,pt2={col1"
	err := ph.Init(config, 15)
	assert.NotNil(t, err)
	fmt.Println(err)

	config = "pt1=test1,pt2={{col1"
	err = ph.Init(config, 15)
	assert.NotNil(t, err)
	fmt.Println(err)

	config = "pt1=test1,pt2={{col1}"
	err = ph.Init(config, 15)
	assert.NotNil(t, err)
	fmt.Println(err)

	config = "pt1=test1,pt2={col1}}"
	err = ph.Init(config, 15)
	assert.NotNil(t, err)
	fmt.Println(err)

	config = "pt1=test1,pt2={a{col1}}"
	err = ph.Init(config, 15)
	assert.NotNil(t, err)
	fmt.Println(err)

	config = "pt1=test1,pt2={{col1}b}"
	err = ph.Init(config, 15)
	assert.NotNil(t, err)
	fmt.Println(err)

	config = "pt1=test1,pt2={{%col1}}"
	err = ph.Init(config, 15)
	assert.NotNil(t, err)
	fmt.Println(err)

	config = "pt1=test1,pt2={{}}"
	err = ph.Init(config, 15)
	assert.NotNil(t, err)
	fmt.Println(err)
}

func TestGenPartition(t *testing.T) {
	ph := NewPartitionHelper()

	config := "pt1=test1,pt2={{col1}},pt3=test_{{col2}},pt4={{%Y%m%d}},pt5=test_{{%H%M}}aaa"
	err := ph.Init(config, 15)
	assert.Nil(t, err)

	log := protocol.Log{
		Time: 1742541463,
	}

	log.Contents = make([]*protocol.Log_Content, 0)
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "col1",
		Value: "value1",
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "col2",
		Value: "value2",
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "col3",
		Value: "value3",
	})

	str := ph.GenPartition(&log)
	assert.Equal(t, "pt1=test1,pt2=value1,pt3=test_value2,pt4=20250321,pt5=test_1515aaa", str)
}
