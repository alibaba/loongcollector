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

// TestFlusherChecker_ExportRecordsEventsNatively verifies that the v2 Export
// path records PipelineGroupEvents natively, without populating the v1 LogGroup
// or otherwise routing through the v1 pipeline data structures.
func TestFlusherChecker_ExportRecordsEventsNatively(t *testing.T) {
	_, v2Groups := makeParityLogGroups()

	checker := &FlusherChecker{}
	require.NoError(t, checker.Init(mock.NewEmptyContext("p", "l", "c")))
	require.NoError(t, checker.Export(v2Groups, nil))

	// Export must not touch the v1 LogGroup.
	require.Empty(t, checker.LogGroup.Logs)

	// Events are recorded as native PipelineGroupEvents and counted.
	expected := 0
	for _, g := range v2Groups {
		expected += len(g.Events)
	}
	require.Equal(t, len(v2Groups), len(checker.GroupEvents))
	require.Equal(t, expected, checker.GetLogCount())

	// Field values survive the v2 path unchanged.
	seen := map[string]interface{}{}
	for _, g := range checker.GroupEvents {
		for _, e := range g.Events {
			log, ok := e.(*models.Log)
			require.True(t, ok)
			for k, v := range log.GetIndices().Iterator() {
				seen[k] = v
			}
		}
	}
	require.Equal(t, "The message: 10", seen["message"])
	require.Equal(t, "1", seen["group"])
}
