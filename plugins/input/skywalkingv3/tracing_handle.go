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
	"runtime"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	skywalking "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/agent/v3"
)

type TracingHandler struct {
	context                      pipeline.Context
	collector                    pipeline.Collector
	cache                        *ResourcePropertiesCache
	compIDMessagingSystemMapping map[int32]string
}

type traceSegmentHandler interface {
	collectorSegment(segment *skywalking.SegmentObject) (result interface{}, e error)
}

type traceSegmentsHandler interface {
	collectorSegments(segment []*skywalking.SegmentObject) (result interface{}, e error)
}

func (h *TracingHandler) collectorSegments(segments []*skywalking.SegmentObject) (result interface{}, e error) {
	defer panicRecover()
	for _, segment := range segments {
		if e = h.collectSegment(segment, h.compIDMessagingSystemMapping); e != nil {
			return e, nil
		}
	}
	return &v3.Commands{}, nil
}

func (h *TracingHandler) collectorSegment(segment *skywalking.SegmentObject) (result interface{}, e error) {
	defer panicRecover()
	return &v3.Commands{}, h.collectSegment(segment, h.compIDMessagingSystemMapping)
}

func panicRecover() {
	if err := recover(); err != nil {
		trace := make([]byte, 2048)
		runtime.Stack(trace, true)
		logger.Error(context.Background(), util.PluginRuntimeAlarm, "skywalking v3 runtime panic error", err, "stack", string(trace))
	}
}

func (h *TracingHandler) Collect(srv skywalking.TraceSegmentReportService_CollectServer) error {
	defer panicRecover()
	for {
		segmentObject, err := srv.Recv()
		if err != nil {
			if err == io.EOF {
				return srv.SendAndClose(&v3.Commands{})
			}
			return err
		}

		err = h.collectSegment(segmentObject, h.compIDMessagingSystemMapping)
		if err != nil {
			return srv.SendAndClose(&v3.Commands{})
		}
	}
}

func (h *TracingHandler) CollectInSync(ctx context.Context, req *skywalking.SegmentCollection) (*v3.Commands, error) {
	defer panicRecover()
	for _, segment := range req.Segments {
		err := h.collectSegment(segment, h.compIDMessagingSystemMapping)
		if err != nil {
			logger.Warning(h.context.GetRuntimeContext(), util.InputCollectAlarm, "skywalking collect trace error", err)
			continue
		}
	}
	return &v3.Commands{}, nil
}

func (h *TracingHandler) collectSegment(segment *skywalking.SegmentObject, mapping map[int32]string) error {
	for _, span := range segment.Spans {
		otTrace := ParseSegment(span, segment, h.cache, mapping)
		if otTrace == nil {
			logger.Warning(h.context.GetRuntimeContext(), util.InputCollectAlarm, "skywalking resource not ready err", "resource properties not found, drop this segment")
			continue
		}
		log, err := otTrace.ToLog()
		if err != nil {
			logger.Error(h.context.GetRuntimeContext(), util.InputCollectAlarm, "skywalking to ot trace err", err)
			return err
		}
		h.collector.AddRawLog(log)
	}
	return nil
}
