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

package grpc

import (
	"encoding/json"
	"io"
	"net"
	"sort"
	"strconv"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc"
	"google.golang.org/grpc/encoding"

	_ "github.com/alibaba/ilogtail/pkg/logger/test"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

type TestServerService struct {
	ch chan *protocol.LogGroup
	protocol.UnimplementedLogReportServiceServer
}

func (t *TestServerService) Collect(stream protocol.LogReportService_CollectServer) error {
	for {
		logGroup, err := stream.Recv()
		if err == io.EOF {
			return stream.SendAndClose(&protocol.Response{
				Code:    protocol.ResponseCode_Success,
				Message: "success",
			})
		}
		if err != nil {
			return err
		}
		t.ch <- logGroup
	}
}

func TestFlusher_Flush(t *testing.T) {
	encoding.RegisterCodec(new(protocol.Codec))
	server := grpc.NewServer()
	defer server.Stop()
	listener, err := net.Listen("tcp", ":8000")
	assert.NoError(t, err)
	receiveChan := make(chan *protocol.LogGroup)
	service := &TestServerService{ch: receiveChan}
	protocol.RegisterLogReportServiceServer(server, service)
	go func() {
		err = server.Serve(listener)
		assert.NoError(t, err)
	}()

	time.Sleep(time.Second)

	p := pipeline.Flushers["flusher_grpc"]().(*Flusher)
	lctx := mock.NewEmptyContext("p", "l", "c")
	err = p.Init(lctx)
	assert.NoError(t, err)

	groupList := makeTestLogGroupList().GetLogGroupList()
	receiveList := make([]*protocol.LogGroup, 0, 10)

	// send logs
	go func() {
		err = p.Flush("p", "l", "c", groupList)
		assert.NoError(t, err)
		time.Sleep(time.Second)
		close(receiveChan)
	}()

	// receiver logs
	for group := range receiveChan {
		receiveList = append(receiveList, group)
	}
	assert.Equal(t, groupList, receiveList)
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

func TestFlusherGrpc_FlushExportParity(t *testing.T) {
	v1Groups, v2Groups := makeParityLogGroups()

	flushLogs := runGrpcCapture(t, func(f *Flusher) error {
		return f.Flush("", "", "", v1Groups)
	})
	exportLogs := runGrpcCapture(t, func(f *Flusher) error {
		return f.Export(v2Groups, nil)
	})

	require.Equal(t, len(flushLogs), len(exportLogs))
	require.Equal(t, normalizedLogGroupsJSON(t, flushLogs), normalizedLogGroupsJSON(t, exportLogs))
}

func normalizedLogGroupsJSON(t testing.TB, groups []*protocol.LogGroup) []string {
	t.Helper()
	out := make([]string, 0)
	for _, group := range groups {
		for _, log := range group.Logs {
			fields := make(map[string]string, len(log.Contents))
			for _, c := range log.Contents {
				fields[c.Key] = c.Value
			}
			payload, err := json.Marshal(map[string]interface{}{
				"time":   log.GetTime(),
				"fields": fields,
			})
			require.NoError(t, err)
			out = append(out, string(payload))
		}
	}
	sort.Strings(out)
	return out
}

func runGrpcCapture(t *testing.T, run func(*Flusher) error) []*protocol.LogGroup {
	t.Helper()
	encoding.RegisterCodec(new(protocol.Codec))

	received := make(chan *protocol.LogGroup, 8)
	server := grpc.NewServer()
	service := &TestServerService{ch: received}
	protocol.RegisterLogReportServiceServer(server, service)

	listener, err := net.Listen("tcp", "127.0.0.1:0")
	require.NoError(t, err)
	go func() {
		_ = server.Serve(listener)
	}()
	defer server.Stop()

	flusher := &Flusher{Address: listener.Addr().String()}
	require.NoError(t, flusher.Init(mock.NewEmptyContext("p", "l", "c")))
	require.NoError(t, run(flusher))

	time.Sleep(100 * time.Millisecond)
	close(received)

	var groups []*protocol.LogGroup
	for group := range received {
		if group != nil && len(group.Logs) > 0 {
			groups = append(groups, group)
		}
	}
	return groups
}
