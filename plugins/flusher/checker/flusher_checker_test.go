// Copyright 2026 iLogtail Authors
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

package checker

import (
	"encoding/json"
	"sort"
	"strconv"
	"testing"
	"time"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func createLogByFields(fields map[string]string) *protocol.Log {
	var slsLog protocol.Log
	for key, val := range fields {
		slsLog.Contents = append(slsLog.Contents, &protocol.Log_Content{Key: key, Value: val})
	}
	protocol.SetLogTime(&slsLog, uint32(time.Now().Unix()))
	return &slsLog
}

func makeTestLogGroupList() *protocol.LogGroupList {
	fields := map[string]string{}
	lgl := &protocol.LogGroupList{
		LogGroupList: make([]*protocol.LogGroup, 0, 10),
	}
	for i := 1; i <= 10; i++ {
		lg := &protocol.LogGroup{
			Logs:   make([]*protocol.Log, 0, 10),
			Source: "",
		}
		for j := 1; j <= 10; j++ {
			fields["group"] = strconv.Itoa(i)
			fields["message"] = "The message: " + strconv.Itoa(j)
			lg.Logs = append(lg.Logs, createLogByFields(fields))
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

func logsToNormalizedJSON(logs []*protocol.Log) ([]string, error) {
	out := make([]string, 0, len(logs))
	for _, log := range logs {
		fields := make(map[string]string, len(log.Contents))
		for _, c := range log.Contents {
			fields[c.Key] = c.Value
		}
		payload, err := json.Marshal(map[string]interface{}{
			"time":   log.GetTime(),
			"fields": fields,
		})
		if err != nil {
			return nil, err
		}
		out = append(out, string(payload))
	}
	sort.Strings(out)
	return out, nil
}

func TestFlusherChecker_FlushExportParity(t *testing.T) {
	v1Groups, v2Groups := makeParityLogGroups()

	flushChecker := &FlusherChecker{}
	require.NoError(t, flushChecker.Init(mock.NewEmptyContext("p", "l", "c")))
	require.NoError(t, flushChecker.Flush("", "", "", v1Groups))

	exportChecker := &FlusherChecker{}
	require.NoError(t, exportChecker.Init(mock.NewEmptyContext("p", "l", "c")))
	require.NoError(t, exportChecker.Export(v2Groups, nil))

	v1JSON, err := logsToNormalizedJSON(flushChecker.LogGroup.Logs)
	require.NoError(t, err)
	v2JSON, err := logsToNormalizedJSON(exportChecker.LogGroup.Logs)
	require.NoError(t, err)
	require.Equal(t, v1JSON, v2JSON)
}
