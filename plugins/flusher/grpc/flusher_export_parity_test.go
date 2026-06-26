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

package grpc

import (
	"net"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
	"google.golang.org/grpc"
	"google.golang.org/grpc/encoding"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/flusherparity"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestFlusherGrpc_FlushExportParity(t *testing.T) {
	v1Groups, v2Groups := flusherparity.BuildCustomSingleParityFixtures()

	flushLogs := runGrpcCapture(t, func(f *Flusher) error {
		return f.Flush("", "", "", v1Groups)
	})
	exportLogs := runGrpcCapture(t, func(f *Flusher) error {
		return f.Export(v2Groups, nil)
	})

	require.Equal(t, len(flushLogs), len(exportLogs))
	for i := range flushLogs {
		require.Equal(t, flushLogs[i].Logs, exportLogs[i].Logs)
	}
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
