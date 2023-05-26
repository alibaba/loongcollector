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
	"fmt"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/pipeline/extensions"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/common"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/influxdb"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/opentelemetry"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/prometheus"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/pyroscope"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/raw"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/sls"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/statsd"
)

type Option struct {
	FieldsExtend      bool
	DisableUncompress bool
	AllowUnsafeMode   bool
}

// GetDecoder return a new decoder for specific format
func GetDecoder(format string) (extensions.Decoder, error) {
	return GetDecoderWithOptions(format, Option{})
}

func GetDecoderWithOptions(format string, option Option) (extensions.Decoder, error) {
	switch strings.TrimSpace(strings.ToLower(format)) {
	case common.ProtocolSLS:
		return &sls.Decoder{}, nil
	case common.ProtocolPrometheus:
		return &prometheus.Decoder{AllowUnsafeMode: option.AllowUnsafeMode}, nil
	case common.ProtocolInflux, common.ProtocolInfluxdb:
		return &influxdb.Decoder{FieldsExtend: option.FieldsExtend}, nil
	case common.ProtocolStatsd:
		return &statsd.Decoder{
			Time: time.Now(),
		}, nil
	case common.ProtocolOTLPLogV1:
		return &opentelemetry.Decoder{Format: common.ProtocolOTLPLogV1}, nil
	case common.ProtocolOTLPMetricV1:
		return &opentelemetry.Decoder{Format: common.ProtocolOTLPMetricV1}, nil
	case common.ProtocolOTLPTraceV1:
		return &opentelemetry.Decoder{Format: common.ProtocolOTLPTraceV1}, nil
	case common.ProtocolRaw:
		return &raw.Decoder{DisableUncompress: option.DisableUncompress}, nil

	case common.ProtocolPyroscope:
		return &pyroscope.Decoder{}, nil
	default:
		return nil, fmt.Errorf("not supported format: %s", format)
	}
}
