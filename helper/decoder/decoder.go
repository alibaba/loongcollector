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

package decoder

import (
	"errors"
	"net/http"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/helper/decoder/common"
	"github.com/alibaba/ilogtail/helper/decoder/influxdb"
	"github.com/alibaba/ilogtail/helper/decoder/opentelemetry"
	"github.com/alibaba/ilogtail/helper/decoder/prometheus"
	"github.com/alibaba/ilogtail/helper/decoder/sls"
	"github.com/alibaba/ilogtail/helper/decoder/statsd"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

// Decoder used to parse buffer to sls logs
type Decoder interface {
	// Decode reader to logs
	Decode(data []byte, req *http.Request) (logs []*protocol.Log, err error)
	ParseRequest(res http.ResponseWriter, req *http.Request, maxBodySize int64) (data []byte, statusCode int, err error)
}

var errDecoderNotFound = errors.New("no such decoder")

// GetDecoder return a new decoder for specific format
func GetDecoder(format string) (Decoder, error) {
	switch strings.TrimSpace(strings.ToLower(format)) {
	case common.ProtocolSLS:
		return &sls.Decoder{}, nil
	case common.ProtocolPrometheus:
		return &prometheus.Decoder{}, nil
	case common.ProtocolInflux, common.ProtocolInfluxdb:
		return &influxdb.Decoder{}, nil
	case common.ProtocolStatsd:
		return &statsd.Decoder{
			Time: time.Now(),
		}, nil
	case common.ProtocolOTLPLogV1:
		return &opentelemetry.Decoder{Format: common.ProtocolOTLPLogV1}, nil
	}
	return nil, errDecoderNotFound
}
