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
	"io"
	"net"
	"strconv"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"google.golang.org/grpc"
	"google.golang.org/grpc/encoding"

	_ "github.com/alibaba/ilogtail/pkg/logger/test"
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
