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

package opentelemetry

import (
	"context"

	"go.opentelemetry.io/collector/pdata/plog"
	"go.opentelemetry.io/collector/pdata/plog/plogotlp"
	"go.opentelemetry.io/collector/pdata/pmetric"
	"go.opentelemetry.io/collector/pdata/pmetric/pmetricotlp"
	"go.opentelemetry.io/collector/pdata/ptrace"
	"go.opentelemetry.io/collector/pdata/ptrace/ptraceotlp"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

// otlpConvertLogGroupToRequest converts v1 log groups into an OTLP logs export request.
// Shared by the gRPC (flusher_otlp) and HTTP (flusher_otlp_http) flushers.
func otlpConvertLogGroupToRequest(conv *converter.Converter, logGroupList []*protocol.LogGroup) plogotlp.ExportRequest {
	logs := plog.NewLogs()
	for _, logGroup := range logGroupList {
		c, _ := conv.Do(logGroup)
		if log, ok := c.(plog.ResourceLogs); ok {
			if log.ScopeLogs().Len() > 0 {
				newLog := logs.ResourceLogs().AppendEmpty()
				log.MoveTo(newLog)
			}
		}
	}

	return plogotlp.NewExportRequestFromLogs(logs)
}

// otlpConvertPipelineEventsToRequests converts v2 pipeline group events into OTLP logs/metrics/traces
// export requests. Shared by the gRPC (flusher_otlp) and HTTP (flusher_otlp_http) flushers.
// runtimeCtx is only used for logging.
func otlpConvertPipelineEventsToRequests(conv *converter.Converter, pipelinegroupeEventSlice []*models.PipelineGroupEvents,
	runtimeCtx context.Context) (plogotlp.ExportRequest, pmetricotlp.ExportRequest, ptraceotlp.ExportRequest) {
	logs := plog.NewLogs()
	metrics := pmetric.NewMetrics()
	traces := ptrace.NewTraces()

	for _, ps := range pipelinegroupeEventSlice {
		resourceLog, resourceMetric, resourceTrace, err := converter.ConvertPipelineEventToOtlpEvent(conv, ps)
		if err != nil {
			logger.Warning(runtimeCtx, selfmonitor.FlusherInitAlarm, "convert pipeline events to otlp events fail, error", err)
		}
		if resourceLog.ScopeLogs().Len() > 0 {
			newLog := logs.ResourceLogs().AppendEmpty()
			resourceLog.MoveTo(newLog)
		}

		if resourceMetric.ScopeMetrics().Len() > 0 {
			newMetric := metrics.ResourceMetrics().AppendEmpty()
			resourceMetric.MoveTo(newMetric)
		}

		if resourceTrace.ScopeSpans().Len() > 0 {
			newTrace := traces.ResourceSpans().AppendEmpty()
			resourceTrace.MoveTo(newTrace)
		}
	}

	return plogotlp.NewExportRequestFromLogs(logs),
		pmetricotlp.NewExportRequestFromMetrics(metrics),
		ptraceotlp.NewExportRequestFromTraces(traces)
}
