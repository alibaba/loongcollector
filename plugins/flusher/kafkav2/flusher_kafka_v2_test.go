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

package kafkav2

import (
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"strconv"
	"testing"

	"github.com/stretchr/testify/require"
)

func TestConnectAndWriteFlusherV1(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	brokers := []string{"tony2c4g:9092"}
	k := NewFlusherKafka()
	k.Brokers = brokers
	k.Topic = "tony_topic"
	// Verify that we can connect to the Kafka broker
	lctx := mock.NewEmptyContext("p", "l", "c")

	err := k.Init(lctx)
	require.NoError(t, err)

	// Verify that we can successfully write data to the kafka broker
	lgl := makeTestLogGroupList()
	err = k.Flush("projectName", "logstoreName", "configName", lgl.GetLogGroupList())
	require.NoError(t, err)
	_ = k.Stop()
}

func makeTestLogGroupList() *protocol.LogGroupList {
	f := map[string]string{}
	lgl := &protocol.LogGroupList{
		LogGroupList: make([]*protocol.LogGroup, 0, 10),
	}
	for i := 1; i <= 10; i++ {
		lg := &protocol.LogGroup{
			Logs: make([]*protocol.Log, 0, 10),
		}
		for j := 1; j <= 10; j++ {
			f["group"] = strconv.Itoa(i)
			f["message"] = "The message: " + strconv.Itoa(j)
			l := test.CreateLogByFields(f)
			lg.Logs = append(lg.Logs, l)
		}
		lgl.LogGroupList = append(lgl.LogGroupList, lg)
	}
	return lgl
}

func TestConnectAndWriteFlusherV2(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	brokers := []string{"tony2c4g:9092"}
	k := NewFlusherKafka()
	k.Brokers = brokers
	k.Topic = "tony_topic"
	k.Convert.Protocol = "raw"
	k.Convert.Encoding = "custom"
	// Verify that we can connect to the Kafka broker
	lctx := mock.NewEmptyContext("p", "l", "c")

	err := k.Init(lctx)
	require.NoError(t, err)

	// Verify that we can successfully write data to the kafka broker
	groupEventsArray := makeTestGroupEventsArray()
	err = k.Export(groupEventsArray, helper.NewNoopPipelineContext())
	require.NoError(t, err)
	_ = k.Stop()
}

func makeTestGroupEventsArray() []*models.PipelineGroupEvents {
	groupEventsArray := make([]*models.PipelineGroupEvents, 0, 10)
	for i := 1; i <= 10; i++ {
		groupEvents := &models.PipelineGroupEvents{
			Group: &models.GroupInfo{
				Metadata: models.NewMetadata(),
				Tags:     models.NewTags(),
			},
			Events: make([]models.PipelineEvent, 0, 10),
		}
		groupEvents.Group.Metadata.Add("group", strconv.Itoa(i))

		for j := 1; j <= 10; j++ {
			event := models.ByteArray("The message: " + strconv.Itoa(j))
			groupEvents.Events = append(groupEvents.Events, event)
		}
		groupEventsArray = append(groupEventsArray, groupEvents)
	}
	return groupEventsArray
}
