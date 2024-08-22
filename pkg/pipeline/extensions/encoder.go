// Copyright 2024 iLogtail Authors
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

package extensions

import (
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

// Encoder encodes data of iLogtail data models into bytes.
// Different drivers with different encoding protocols implement Encoder interface.
//
// drivers: raw, influxdb, prometheus, sls, ...
type Encoder interface {
	EncoderV1
	EncoderV2
}

// EncoderV1 supports v1 pipeline plugin interface,
// encodes data of v1 model into bytes.
//
// drivers: sls, influxdb, ...
type EncoderV1 interface {
	EncodeV1(*protocol.LogGroup) ([][]byte, error)
	EncodeBatchV1([]*protocol.LogGroup) ([][]byte, error)
}

// EncoderV2 supports v2 pipeline plugin interface,
// encodes data of v2 model into bytes.
//
// drivers: raw, influxdb, prometheus, ...
type EncoderV2 interface {
	EncodeV2(*models.PipelineGroupEvents) ([][]byte, error)
	EncodeBatchV2([]*models.PipelineGroupEvents) ([][]byte, error)
}

type EncoderExtension interface {
	Encoder
	pipeline.Extension
}
