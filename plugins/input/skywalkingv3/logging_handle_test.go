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

package skywalkingv3

import (
	"context"
	"io"
	"testing"

	"google.golang.org/grpc/metadata"

	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	logging "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/logging/v3"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestJsonLogging(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	ctx.InitContext("a", "b", "c")
	collector := &test.MockCollector{}

	handler := &loggingHandler{ctx, collector}

	handler.Collect(&MockRequest{1})

	validate("./testdata/logging.json", collector.RawLogs, t)
}

type MockRequest struct {
	size uint8
}

func (m *MockRequest) SendAndClose(commands *v3.Commands) error {
	return nil
}

func (m *MockRequest) Recv() (*logging.LogData, error) {
	if m.size == 0 {
		return nil, io.EOF
	}

	m.size--

	tags := make([]*v3.KeyStringValuePair, 0)
	tags = append(tags, &v3.KeyStringValuePair{Key: "test", Value: "test2"})
	return &logging.LogData{
		Timestamp:       1651902032613,
		Service:         "test",
		ServiceInstance: "123",
		Endpoint:        "test",
		Body:            &logging.LogDataBody{Type: "json", Content: &logging.LogDataBody_Json{Json: &logging.JSONLog{Json: "test"}}},
		TraceContext:    &logging.TraceContext{TraceId: "test", TraceSegmentId: "test", SpanId: 0},
		Tags:            &logging.LogTags{Data: tags},
	}, nil
}

func (m *MockRequest) SetHeader(md metadata.MD) error {
	return nil
}

func (m *MockRequest) SendHeader(md metadata.MD) error {
	return nil
}

func (m *MockRequest) SetTrailer(md metadata.MD) {

}

func (m *MockRequest) Context() context.Context {
	return nil
}

func (m *MockRequest) SendMsg(a interface{}) error {
	return nil
}

func (m *MockRequest) RecvMsg(a interface{}) error {
	return nil
}
