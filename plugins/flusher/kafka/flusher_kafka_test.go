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

package kafka

import (
	"encoding/json"
	"sort"
	"strconv"
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

// Invalid Test
func InvalidTestConnectAndWrite(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	brokers := []string{"172.17.0.2:9092"}
	k := &FlusherKafka{
		Brokers:      brokers,
		Topic:        "Test",
		SASLUsername: "",
		SASLPassword: "",
	}

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
			Logs:   make([]*protocol.Log, 0, 10),
			Source: "",
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

func makeParityLogGroups() ([]*protocol.LogGroup, []*models.PipelineGroupEvents) {
	v1Groups := makeTestLogGroupList().GetLogGroupList()[:1]
	return v1Groups, makePipelineGroupEventsFromLogGroups(v1Groups)
}

func makePipelineGroupEventsFromLogGroups(v1Groups []*protocol.LogGroup) []*models.PipelineGroupEvents {
	v2Groups := make([]*models.PipelineGroupEvents, 0, len(v1Groups))
	for _, lg := range v1Groups {
		groupTags := models.NewTags()
		groupTags.Add("host.ip", lg.Source)
		for _, tag := range lg.LogTags {
			groupTags.Add(tag.Key, tag.Value)
		}
		group := models.NewGroup(models.NewMetadata(), groupTags)
		events := make([]models.PipelineEvent, 0, len(lg.Logs))
		for _, log := range lg.Logs {
			ts := uint64(log.GetTime()) * 1e9
			if log.TimeNs != nil {
				ts += uint64(*log.TimeNs)
			}
			v2Log := models.NewLog("", nil, "", "", "", models.NewTags(), ts)
			for _, c := range log.Contents {
				v2Log.GetIndices().Add(c.Key, c.Value)
			}
			events = append(events, v2Log)
		}
		v2Groups = append(v2Groups, &models.PipelineGroupEvents{Group: group, Events: events})
	}
	return v2Groups
}

func TestFlusherKafka_FlushExportParity(t *testing.T) {
	v1Groups, v2Groups := makeParityLogGroups()

	var flushPayloads, exportPayloads []string
	newCapture := func(dst *[]string) FlusherFunc {
		return func(_ string, _ string, _ string, logGroupList []*protocol.LogGroup) error {
			for _, lg := range logGroupList {
				for _, log := range lg.Logs {
					sort.Slice(log.Contents, func(i, j int) bool {
						return log.Contents[i].Key < log.Contents[j].Key
					})
					buf, err := json.Marshal(log)
					require.NoError(t, err)
					*dst = append(*dst, string(buf))
				}
			}
			return nil
		}
	}

	flushFlusher := &FlusherKafka{flusher: newCapture(&flushPayloads)}
	require.NoError(t, flushFlusher.Flush("", "", "", v1Groups))

	exportFlusher := &FlusherKafka{flusher: newCapture(&exportPayloads)}
	require.NoError(t, exportFlusher.Export(v2Groups, nil))

	require.Equal(t, len(flushPayloads), len(exportPayloads))
	for i := range flushPayloads {
		require.JSONEq(t, flushPayloads[i], exportPayloads[i])
	}
}
