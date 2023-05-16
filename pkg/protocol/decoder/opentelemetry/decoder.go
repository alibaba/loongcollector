// Copyright 2022 iLogtail Authors
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
	"errors"
	"net/http"

	"go.opentelemetry.io/collector/pdata/plog/plogotlp"
	"go.opentelemetry.io/collector/pdata/pmetric/pmetricotlp"
	"go.opentelemetry.io/collector/pdata/ptrace/ptraceotlp"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/common"
)

const (
	pbContentType   = "application/x-protobuf"
	jsonContentType = "application/json"
)

const (
	logEventName = "log_event"
)

// Decoder impl
type Decoder struct {
	Format string
}

// Decode impl
func (d *Decoder) Decode(data []byte, req *http.Request, tags map[string]string) (logs []*protocol.Log, err error) {
	switch d.Format {
	case common.ProtocolOTLPLogV1:
		otlpLogReq := plogotlp.NewExportRequest()
		otlpLogReq, err = DecodeOtlpRequest(otlpLogReq, data, req)
		if err != nil {
			return logs, err
		}
		logs, err = ConvertOtlpLogRequestV1(otlpLogReq)
	case common.ProtocolOTLPMetricV1:
		otlpMetricReq := pmetricotlp.NewExportRequest()
		otlpMetricReq, err = DecodeOtlpRequest(otlpMetricReq, data, req)
		if err != nil {
			return logs, err
		}
		logs, err = ConvertOtlpMetricRequestV1(otlpMetricReq)
	case common.ProtocolOTLPTraceV1:
		otlpTraceReq := ptraceotlp.NewExportRequest()
		otlpTraceReq, err = DecodeOtlpRequest(otlpTraceReq, data, req)
		if err != nil {
			return logs, err
		}
		logs, err = ConvertOtlpTraceRequestV1(otlpTraceReq)
	default:
		err = errors.New("Invalid RequestURI: " + req.RequestURI)
	}
	return logs, err
}

// ParseRequest impl
func (d *Decoder) ParseRequest(res http.ResponseWriter, req *http.Request, maxBodySize int64) (data []byte, statusCode int, err error) {
	return common.CollectBody(res, req, maxBodySize)
}

// DecodeV2 impl
func (d *Decoder) DecodeV2(data []byte, req *http.Request) (groups []*models.PipelineGroupEvents, err error) {
	switch d.Format {
	case common.ProtocolOTLPLogV1:
		otlpLogReq := plogotlp.NewExportRequest()
		otlpLogReq, err = DecodeOtlpRequest(otlpLogReq, data, req)
		if err != nil {
			return groups, err
		}
		groups, err = ConvertOtlpLogRequestToGroupEvents(otlpLogReq)
	case common.ProtocolOTLPMetricV1:
		otlpMetricReq := pmetricotlp.NewExportRequest()
		otlpMetricReq, err = DecodeOtlpRequest(otlpMetricReq, data, req)
		if err != nil {
			return groups, err
		}
		groups, err = ConvertOtlpMetricRequestToGroupEvents(otlpMetricReq)
	case common.ProtocolOTLPTraceV1:
		otlpTraceReq := ptraceotlp.NewExportRequest()
		otlpTraceReq, err = DecodeOtlpRequest(otlpTraceReq, data, req)
		if err != nil {
			return groups, err
		}
		groups, err = ConvertOtlpTraceRequestToGroupEvents(otlpTraceReq)
	default:
		err = errors.New("Invalid RequestURI: " + req.RequestURI)
	}
	return groups, err
}

// DecodeOtlpRequest decodes the data and fills into the otlp logs/metrics/traces export request.
func DecodeOtlpRequest[P interface {
	UnmarshalProto(data []byte) error
	UnmarshalJSON(data []byte) error
}](des P, data []byte, req *http.Request) (P, error) {
	var err error
	contentType := req.Header.Get("Content-Type")
	switch contentType {
	case pbContentType:
		if err = des.UnmarshalProto(data); err != nil {
			return des, err
		}
	case jsonContentType:
		if err = des.UnmarshalJSON(data); err != nil {
			return des, err
		}
	default:
		err = errors.New("Invalid ContentType: " + contentType)
	}
	return des, err
}
