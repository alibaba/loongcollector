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
	"encoding/json"
	"fmt"
	"io"
	"strconv"
	"strings"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/protocol"
	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	loggingV3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/logging/v3"
)

type loggingHandler struct {
	context   ilogtail.Context
	collector ilogtail.Collector
}

func (l *loggingHandler) Collect(server loggingV3.LogReportService_CollectServer) error {
	defer panicRecover()

	for {
		logging, err := server.Recv()
		if err != nil {
			if err == io.EOF {
				return server.SendAndClose(&v3.Commands{})
			}
			return err
		}

		l.sendLogging(logging)
	}
}

func (l *loggingHandler) sendLogging(data *loggingV3.LogData) {
	if data == nil {
		return
	}

	l.collector.AddRawLog(l.convertFormat(data))
}

func (l *loggingHandler) convertFormat(data *loggingV3.LogData) *protocol.Log {
	r := &protocol.Log{
		Time:     uint32(data.Timestamp / 1000),
		Contents: make([]*protocol.Log_Content, 0),
	}

	r.Contents = append(r.Contents, &protocol.Log_Content{Key: "otlp.name", Value: "apache-skywalking"})
	r.Contents = append(r.Contents, &protocol.Log_Content{
		Key:   "attribute",
		Value: convertAttribute(data),
	})
	r.Contents = append(r.Contents, &protocol.Log_Content{Key: "service", Value: data.Service})
	r.Contents = append(r.Contents, &protocol.Log_Content{Key: "content", Value: convertContent(data.Body)})
	r.Contents = append(r.Contents, &protocol.Log_Content{Key: "traceID", Value: data.TraceContext.TraceId})
	r.Contents = append(r.Contents, &protocol.Log_Content{Key: "spanID", Value: fmt.Sprintf("%s.%d", data.TraceContext.TraceSegmentId, data.TraceContext.SpanId)})
	r.Contents = append(r.Contents, &protocol.Log_Content{Key: "resource", Value: convertResource(data)})
	r.Contents = append(r.Contents, &protocol.Log_Content{Key: "timeUnixNano", Value: strconv.FormatInt(data.Timestamp, 10)})
	return r
}

func convertResource(data *loggingV3.LogData) string {
	m := make(map[string]string)
	m["serviceInstance"] = data.ServiceInstance

	if r, e := json.Marshal(m); e == nil {
		return string(r)
	}

	return fmt.Sprintf("%v", m)
}

func convertContent(body *loggingV3.LogDataBody) string {
	switch strings.ToUpper(body.GetType()) {
	case "TEXT":
		return body.GetText().GetText()
	case "JSON":
		return body.GetJson().GetJson()
	case "YAML":
		return body.GetYaml().GetYaml()
	default:
		return body.String()
	}
}

func convertAttribute(data *loggingV3.LogData) string {
	attribute := make(map[string]string)
	attribute["endpoint"] = data.Endpoint
	for _, tag := range data.Tags.GetData() {
		attribute[tag.GetKey()] = tag.GetValue()
	}
	if r, e := json.Marshal(attribute); e == nil {
		return string(r)
	}

	return fmt.Sprintf("%v", attribute)
}
