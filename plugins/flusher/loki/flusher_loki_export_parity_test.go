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

package loki

import (
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/prometheus/common/model"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/plugins/test/flusherparity"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestFlusherLoki_FlushExportParity(t *testing.T) {
	v1Groups, v2Groups := flusherparity.BuildCustomSingleParityFixtures()

	flushLines := captureLokiPayloads(t, func(f *FlusherLoki) error {
		return f.Flush("", "", "", v1Groups)
	})
	exportLines := captureLokiPayloads(t, func(f *FlusherLoki) error {
		return f.Export(v2Groups, nil)
	})

	require.Equal(t, flushLines, exportLines)
}

func captureLokiPayloads(t *testing.T, run func(*FlusherLoki) error) []string {
	t.Helper()
	var bodies []string
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body, _ := io.ReadAll(r.Body)
		bodies = append(bodies, string(body))
		w.WriteHeader(http.StatusNoContent)
	}))
	defer server.Close()

	f := NewFlusherLoki()
	f.URL = server.URL + "/loki/api/v1/push"
	f.StaticLabels = model.LabelSet{"app": "parity-test"}
	f.DynamicLabels = []string{}
	f.MaxMessageWait = 10 * time.Millisecond
	require.NoError(t, f.Init(mock.NewEmptyContext("p", "l", "c")))
	require.NoError(t, run(f))
	require.NoError(t, f.Stop())
	time.Sleep(20 * time.Millisecond)

	var payloads []string
	for _, body := range bodies {
		payloads = append(payloads, extractJSONLogLine(body))
	}
	return flusherparity.NormalizeJSONLines(toBytes(payloads))
}

func extractJSONLogLine(body string) string {
	start := strings.Index(body, `{"contents"`)
	if start < 0 {
		return body
	}
	end := start
	for end < len(body) && body[end] != '}' {
		end++
	}
	// include nested closing braces
	depth := 0
	for i := start; i < len(body); i++ {
		switch body[i] {
		case '{':
			depth++
		case '}':
			depth--
			if depth == 0 {
				candidate := body[start : i+1]
				if json.Valid([]byte(candidate)) {
					return candidate
				}
			}
		}
	}
	return body[start:end]
}

func toBytes(lines []string) [][]byte {
	out := make([][]byte, len(lines))
	for i, line := range lines {
		out[i] = []byte(line)
	}
	return out
}
