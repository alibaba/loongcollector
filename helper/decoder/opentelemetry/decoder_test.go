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

package opentelemetry

import (
	"encoding/json"
	"fmt"
	"net/http"
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/helper/decoder/common"
)

var textFormat = `{"resourceLogs":[{"resource":{"attributes":[{"key":"service.name","value":{"stringValue":"OtlpExporterExample"}},{"key":"telemetry.sdk.language","value":{"stringValue":"java"}},{"key":"telemetry.sdk.name","value":{"stringValue":"opentelemetry"}},{"key":"telemetry.sdk.version","value":{"stringValue":"1.18.0"}}]},"scopeLogs":[{"scope":{"name":"io.opentelemetry.example"},"logRecords":[{"timeUnixNano":"1663904182348000000","severityNumber":9,"severityText":"INFO","body":{"stringValue":"log body1"},"attributes":[{"key":"k1","value":{"stringValue":"v1"}},{"key":"k2","value":{"stringValue":"v2"}}],"traceId":"","spanId":""},{"timeUnixNano":"1663904182348000000","severityNumber":9,"severityText":"INFO","body":{"stringValue":"log body2"},"attributes":[{"key":"k1","value":{"stringValue":"v1"}},{"key":"k2","value":{"stringValue":"v2"}}],"traceId":"","spanId":""}]}]}]}`

func TestNormal(t *testing.T) {
	httpReq, _ := http.NewRequest("Post", "", nil)
	httpReq.Header.Set("Content-Type", jsonContentType)
	decoder := &Decoder{Format: common.ProtocolOTLPLogV1}

	logs, err := decoder.Decode([]byte(textFormat), httpReq)
	assert.Nil(t, err)
	assert.Equal(t, len(logs), 2)
	log := logs[1]
	assert.Equal(t, int(log.Time), 1663904182)
	for _, cont := range log.Contents {
		if cont.Key == "attributes" {
			assert.NotEmpty(t, cont.Value)
		} else if cont.Key == "resources" {
			assert.NotEmpty(t, cont.Value)
		}
	}
	data, _ := json.Marshal(logs)
	fmt.Printf("%s\n", string(data))
	data = []byte("")
	fmt.Printf("empty = [%s]\n", string(data))
}
