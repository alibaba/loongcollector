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
	"encoding/json"
	"errors"
	"net/http"
	"strconv"

	"go.opentelemetry.io/collector/pdata/plog/plogotlp"

	"github.com/alibaba/ilogtail/helper/decoder/common"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const (
	pbContentType   = "application/x-protobuf"
	jsonContentType = "application/json"
)

// Decoder impl
type Decoder struct {
	Format string
}

// Decode impl
func (d *Decoder) Decode(data []byte, req *http.Request) (logs []*protocol.Log, err error) {
	if common.ProtocolOTLPLogV1 == d.Format {
		otlpLogReq := plogotlp.NewRequest()
		switch req.Header.Get("Content-Type") {
		case pbContentType:
			if err = otlpLogReq.UnmarshalProto(data); err != nil {
				return logs, err
			}
			logs, err = d.ConvertOtlpLogV1(otlpLogReq)
		case jsonContentType:
			if err = otlpLogReq.UnmarshalJSON(data); err != nil {
				return logs, err
			}
			logs, err = d.ConvertOtlpLogV1(otlpLogReq)
		default:
			err = errors.New("Invalid ContentType: " + req.Header.Get("Content-Type"))
		}
	} else {
		err = errors.New("Invalid RequestURI: " + req.RequestURI)
	}
	return logs, err
}

func (d *Decoder) ConvertOtlpLogV1(otlpLogReq plogotlp.Request) (logs []*protocol.Log, err error) {
	resLogs := otlpLogReq.Logs().ResourceLogs()
	for i := 0; i < resLogs.Len(); i++ {
		resourceLog := resLogs.At(i)
		sLogs := resourceLog.ScopeLogs()
		for j := 0; j < sLogs.Len(); j++ {
			scopeLog := sLogs.At(j)
			lRecords := scopeLog.LogRecords()
			for k := 0; k < lRecords.Len(); k++ {
				logRecord := lRecords.At(k)

				protoContents := []*protocol.Log_Content{
					{
						Key:   "time_unix_nano",
						Value: strconv.FormatInt(logRecord.Timestamp().AsTime().UnixNano(), 10),
					},
					{
						Key:   "severity_number",
						Value: strconv.FormatInt(int64(logRecord.SeverityNumber()), 10),
					},
					{
						Key:   "severity_text",
						Value: logRecord.SeverityText(),
					},
					{
						Key:   "content",
						Value: logRecord.Body().AsString(),
					},
				}

				if logRecord.Attributes().Len() != 0 {
					if d, err := json.Marshal(logRecord.Attributes().AsRaw()); err == nil {
						protoContents = append(protoContents, &protocol.Log_Content{
							Key:   "attributes",
							Value: string(d),
						})
					}
				}

				if resourceLog.Resource().Attributes().Len() != 0 {
					if d, err := json.Marshal(resourceLog.Resource().Attributes().AsRaw()); err == nil {
						protoContents = append(protoContents, &protocol.Log_Content{
							Key:   "resources",
							Value: string(d),
						})
					}
				}

				protoLog := &protocol.Log{
					Time:     uint32(logRecord.Timestamp().AsTime().Unix()),
					Contents: protoContents,
				}
				logs = append(logs, protoLog)
			}
		}
	}

	return logs, nil
}

func (d *Decoder) ParseRequest(res http.ResponseWriter, req *http.Request, maxBodySize int64) (data []byte, statusCode int, err error) {
	return common.CollectBody(res, req, maxBodySize)
}
