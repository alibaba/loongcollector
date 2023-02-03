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
	"encoding/json"
	"net/http"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"google.golang.org/protobuf/encoding/protojson"

	common "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/agent/v3"
	loggingV3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/logging/v3"
	management "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/management/v3"
)

type SkywalkingHTTPServerInput struct {
	Address          string
	server           *http.Server
	context          pipeline.Context
	ComponentMapping map[int32]string
}

type parameterParser func(request *http.Request) (interface{}, error)
type urlHandler func(parameter interface{}) (interface{}, error)

func (s *SkywalkingHTTPServerInput) Init(context pipeline.Context) (int, error) {
	s.context = context
	return 0, nil
}

func (s *SkywalkingHTTPServerInput) Description() string {
	return "This is a skywalking v3 http server input"
}

func (s *SkywalkingHTTPServerInput) Start(collector pipeline.Collector) error {
	logger.Info(s.context.GetRuntimeContext(), "Start skywalking v3 http server")
	mux := http.NewServeMux()

	resourcePropertiesCache := &ResourcePropertiesCache{
		cache:    make(map[string]map[string]string),
		cacheKey: s.context.GetConfigName() + "#" + CheckpointKey,
	}
	resourcePropertiesCache.load(s.context)

	traceHandler := &TracingHandler{
		context:                      s.context,
		collector:                    collector,
		cache:                        resourcePropertiesCache,
		compIDMessagingSystemMapping: InitComponentMapping(s.ComponentMapping),
	}

	managementHandler := &ManagementHandler{
		context:   s.context,
		collector: collector,
		cache:     resourcePropertiesCache,
	}

	loggingHandler := &loggingHandler{
		context:   s.context,
		collector: collector,
	}

	mux.HandleFunc("/v3/segment", s.registerTraceSegmentHandler(traceHandler))
	mux.HandleFunc("/v3/segments", s.registerTraceSegmentsHandler(traceHandler))
	mux.HandleFunc("/v3/management/reportProperties", s.registerReportPropertiesHandler(managementHandler))
	mux.HandleFunc("/v3/management/keepAlive", s.registerKeepAliveHandler(managementHandler))
	mux.HandleFunc("/v3/logs", s.registerLogHandler(loggingHandler))
	mux.HandleFunc("/v3/events", s.registerEventHandler())
	mux.HandleFunc("/browser/errorLog", s.registerErrorLogHandler(loggingHandler))
	mux.HandleFunc("/browser/errorLogs", s.registerErrorLogsHandler(loggingHandler))
	mux.HandleFunc("/browser/perfData", s.registerPerfData(&perfDataHandlerImpl{s.context, collector}))
	if s.Address == "" {
		s.Address = "0.0.0.0:12800"
	}

	s.server = &http.Server{
		Addr:        s.Address,
		Handler:     mux,
		ReadTimeout: 1 * time.Second,
	}
	return s.server.ListenAndServe()
}

func (s *SkywalkingHTTPServerInput) registerPerfData(handler perfDataHandler) func(http.ResponseWriter, *http.Request) {
	return s.registerHandler(func(request *http.Request) (interface{}, error) {
		var browserPerfData *v3.BrowserPerfData
		return browserPerfData, json.NewDecoder(request.Body).Decode(browserPerfData)
	}, func(parameter interface{}) (interface{}, error) {
		return handler.collectorPerfData(parameter.(*v3.BrowserPerfData))
	})
}

func (s *SkywalkingHTTPServerInput) registerErrorLogsHandler(handler errorLogsHandler) func(http.ResponseWriter, *http.Request) {
	return s.registerHandler(func(request *http.Request) (interface{}, error) {
		loggingData := make([]*v3.BrowserErrorLog, 0)
		dec := json.NewDecoder(request.Body)
		_, e := dec.Token()
		if e != nil {
			return nil, e
		}

		for dec.More() {
			var result map[string]interface{}
			if e := dec.Decode(&result); e != nil {
				logger.Error(s.context.GetRuntimeContext(), "Failed to decode error log", e)
				continue
			} else {
				d, _ := json.Marshal(result)
				log := &v3.BrowserErrorLog{}
				if e := protojson.Unmarshal(d, log); e != nil {
					continue
				}
				loggingData = append(loggingData, log)
			}
		}
		return loggingData, nil
	}, func(parameter interface{}) (interface{}, error) {
		return handler.collectorErrorLogs(parameter.([]*v3.BrowserErrorLog))
	})
}

func (s *SkywalkingHTTPServerInput) registerErrorLogHandler(handler errorLogHandler) func(http.ResponseWriter, *http.Request) {
	return s.registerHandler(func(request *http.Request) (interface{}, error) {
		loggingData := &v3.BrowserErrorLog{}
		return loggingData, json.NewDecoder(request.Body).Decode(loggingData)
	}, func(parameter interface{}) (interface{}, error) {
		return handler.collectorErrorLog(parameter.(*v3.BrowserErrorLog))
	})
}

func (s *SkywalkingHTTPServerInput) registerEventHandler() func(http.ResponseWriter, *http.Request) {
	return s.registerHandler(func(request *http.Request) (interface{}, error) {
		return nil, nil
	}, func(parameter interface{}) (interface{}, error) {
		return &common.Commands{}, nil
	})
}

func (s *SkywalkingHTTPServerInput) registerLogHandler(handler loggingDataHandler) func(http.ResponseWriter, *http.Request) {
	return s.registerHandler(func(request *http.Request) (interface{}, error) {
		loggingData := &loggingV3.LogData{}
		return loggingData, json.NewDecoder(request.Body).Decode(loggingData)
	}, func(parameter interface{}) (interface{}, error) {
		return handler.collectorLog(parameter.(*loggingV3.LogData))
	})
}

func (s *SkywalkingHTTPServerInput) registerKeepAliveHandler(handler keepAliveHandler) func(http.ResponseWriter, *http.Request) {
	return s.registerHandler(func(request *http.Request) (interface{}, error) {
		instancePingPkg := &management.InstancePingPkg{}
		return instancePingPkg, json.NewDecoder(request.Body).Decode(instancePingPkg)
	}, func(parameter interface{}) (interface{}, error) {
		return handler.keepAliveHandler(parameter.(*management.InstancePingPkg))
	})
}

func (s *SkywalkingHTTPServerInput) registerReportPropertiesHandler(handler reportInstancePropertiesHandler) func(http.ResponseWriter, *http.Request) {
	return s.registerHandler(func(request *http.Request) (interface{}, error) {
		instanceProperties := &management.InstanceProperties{}
		return instanceProperties, json.NewDecoder(request.Body).Decode(instanceProperties)
	}, func(parameter interface{}) (interface{}, error) {
		return handler.reportInstanceProperties(parameter.(*management.InstanceProperties))
	})
}

func (s *SkywalkingHTTPServerInput) registerTraceSegmentHandler(handler traceSegmentHandler) func(writer http.ResponseWriter, request *http.Request) {
	return s.registerHandler(func(request *http.Request) (interface{}, error) {
		traceSegment := &v3.SegmentObject{}
		return traceSegment, json.NewDecoder(request.Body).Decode(traceSegment)
	}, func(parameter interface{}) (interface{}, error) {
		return handler.collectorSegment(parameter.(*v3.SegmentObject))
	})
}

func (s *SkywalkingHTTPServerInput) registerTraceSegmentsHandler(handler traceSegmentsHandler) func(writer http.ResponseWriter, request *http.Request) {
	return s.registerHandler(func(request *http.Request) (interface{}, error) {
		segments := make([]*v3.SegmentObject, 0)
		dec := json.NewDecoder(request.Body)
		_, e := dec.Token()
		if e != nil {
			return nil, e
		}

		for dec.More() {
			var result map[string]interface{}
			if e := dec.Decode(&result); e != nil {
				logger.Error(s.context.GetRuntimeContext(), "Failed to decode error log", e)
				continue
			} else {
				d, _ := json.Marshal(result)
				segment := &v3.SegmentObject{}
				if e := protojson.Unmarshal(d, segment); e != nil {
					continue
				}
				segments = append(segments, segment)
			}
		}
		return segments, nil
	}, func(data interface{}) (interface{}, error) {
		return handler.collectorSegments(data.([]*v3.SegmentObject))
	})
}

func (s *SkywalkingHTTPServerInput) registerHandler(parser parameterParser, handler urlHandler) func(writer http.ResponseWriter, request *http.Request) {
	return func(writer http.ResponseWriter, request *http.Request) {
		if parameter, e := parser(request); e != nil {
			writer.WriteHeader(http.StatusBadRequest)
			s.writeResponse(writer, []byte("Failed to parse request"))
		} else {
			if e, result := handler(parameter); e != nil {
				writer.WriteHeader(http.StatusInternalServerError)
				s.writeResponse(writer, []byte("Failed to handle request"))
			} else {
				if r, err := json.Marshal(result); err != nil {
					writer.WriteHeader(http.StatusBadRequest)
					s.writeResponse(writer, []byte("Failed to marshal result"))
				} else {
					s.writeResponse(writer, r)
				}
			}
		}
	}
}

func (s *SkywalkingHTTPServerInput) writeResponse(writer http.ResponseWriter, data []byte) {
	if _, e := writer.Write(data); e != nil {
		logger.Error(s.context.GetRuntimeContext(), "Failed to write response", e)
	}
}

func (s *SkywalkingHTTPServerInput) Stop() error {
	logger.Info(s.context.GetRuntimeContext(), "close skywalking v3 http server")
	return s.server.Shutdown(context.Background())
}

func init() {
	pipeline.ServiceInputs["service_skywalking_agent_v3_http"] = func() pipeline.ServiceInput {
		return &SkywalkingHTTPServerInput{
			Address: "0.0.0.0:12800",
		}
	}
}
