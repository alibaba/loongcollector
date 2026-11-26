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
	"time"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	agent "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/agent/v3"
	loggingV3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/logging/v3"
)

type loggingHandler struct {
	context   pipeline.Context
	collector pipeline.PipelineCollector
}

func (l *loggingHandler) collectorErrorLogs(logs []*agent.BrowserErrorLog) (*v3.Commands, error) {
	for _, log := range logs {
		if _, e := l.collectorErrorLog(log); e != nil {
			return nil, e
		}
	}
	return &v3.Commands{}, nil
}

func (l *loggingHandler) collectorErrorLog(data *agent.BrowserErrorLog) (*v3.Commands, error) {
	defer panicRecover()
	if data == nil {
		return &v3.Commands{}, nil
	}

	l.collector.Collect(models.NewGroup(models.NewMetadata(), models.NewTags()), l.convertBrowserErrorLog(data))

	return &v3.Commands{}, nil
}

func (l *loggingHandler) collectorLog(logging *loggingV3.LogData) (*v3.Commands, error) {
	l.sendLogging(logging)
	return &v3.Commands{}, nil
}

type loggingDataHandler interface {
	collectorLog(data *loggingV3.LogData) (*v3.Commands, error)
}

type errorLogHandler interface {
	collectorErrorLog(data *agent.BrowserErrorLog) (*v3.Commands, error)
}

type errorLogsHandler interface {
	collectorErrorLogs(data []*agent.BrowserErrorLog) (*v3.Commands, error)
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

	l.collector.Collect(models.NewGroup(models.NewMetadata(), models.NewTags()), l.convertFormat(data))
}

func (l *loggingHandler) convertFormat(data *loggingV3.LogData) *models.Log {
	log := &models.Log{
		Contents:  models.NewOrderedLogContents(16),
		Tags:      models.NewTags(),
		Timestamp: uint64(time.UnixMilli(data.Timestamp).UnixNano()),
	}
	if data.Timestamp/1000 == 0 {
		log.Timestamp = uint64(time.Now().UnixNano())
	}

	log.Contents.Add("otlp.name", "apache-skywalking")
	log.Contents.Add("attribute", convertAttribute(data))
	log.Contents.Add("service", data.Service)
	log.SetBody([]byte(convertContent(data.Body)))
	if data.TraceContext != nil {
		log.Contents.Add("traceID", data.TraceContext.TraceId)
		log.Contents.Add("spanID", fmt.Sprintf("%s.%d", data.TraceContext.TraceSegmentId, data.TraceContext.SpanId))
	}
	log.Contents.Add("resource", convertResource(data))
	log.Contents.Add("timeUnixNano", strconv.FormatInt(data.Timestamp, 10))
	return log
}

func (l *loggingHandler) convertBrowserErrorLog(data *agent.BrowserErrorLog) *models.Log {
	log := &models.Log{
		Contents:  models.NewOrderedLogContents(16),
		Tags:      models.NewTags(),
		Timestamp: uint64(time.UnixMilli(data.Time).UnixNano()),
	}
	if data.Time/1000 == 0 {
		log.Timestamp = uint64(time.Now().UnixNano())
	}

	log.Contents.Add("otlp.name", "apache-skywalking")
	log.Contents.Add("service", data.Service)
	log.Contents.Add("uniqueId", data.UniqueId)
	log.Contents.Add("serviceVersion", data.ServiceVersion)
	log.Contents.Add("category", data.Category.String())
	log.Contents.Add("pagePath", data.PagePath)
	log.Contents.Add("grade", data.Grade)
	log.Contents.Add("message", data.Message)
	log.Contents.Add("line", string(data.Line))
	log.Contents.Add("col", string(data.Col))
	log.Contents.Add("stack", data.Stack)
	log.Contents.Add("errorUrl", string(data.Col))
	log.Contents.Add("firstReportedError", strconv.FormatBool(data.FirstReportedError))
	return log
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
	if data.Tags != nil {
		for _, tag := range data.Tags.GetData() {
			attribute[tag.GetKey()] = tag.GetValue()
		}
	}
	if r, e := json.Marshal(attribute); e == nil {
		return string(r)
	}

	return fmt.Sprintf("%v", attribute)
}
