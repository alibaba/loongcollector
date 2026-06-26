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

package elasticsearch

import (
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"

	"github.com/elastic/go-elasticsearch/v8"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/plugins/test/flusherparity"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

type bulkBodyCapture struct {
	mu     sync.Mutex
	bodies []string
}

func (c *bulkBodyCapture) handler(w http.ResponseWriter, r *http.Request) {
	body, _ := io.ReadAll(r.Body)
	c.mu.Lock()
	c.bodies = append(c.bodies, string(body))
	c.mu.Unlock()
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("X-Elastic-Product", "Elasticsearch")
	w.WriteHeader(http.StatusOK)
	_, _ = w.Write([]byte(`{"errors":false,"items":[]}`))
}

func (c *bulkBodyCapture) snapshot() []string {
	c.mu.Lock()
	defer c.mu.Unlock()
	out := append([]string(nil), c.bodies...)
	return out
}

func newElasticSearchParityFlusher(t *testing.T, server *httptest.Server) *FlusherElasticSearch {
	t.Helper()
	cvt, err := flusherparity.NewCustomSingleConverter()
	require.NoError(t, err)

	client, err := elasticsearch.NewClient(elasticsearch.Config{Addresses: []string{server.URL}})
	require.NoError(t, err)

	f := NewFlusherElasticSearch()
	f.context = mock.NewEmptyContext("p", "l", "c")
	f.converter = cvt
	f.esClient = client
	f.Index = "parity-index"
	f.indexKeys = nil
	f.isDynamicIndex = false
	return f
}

func normalizeBulkBody(body string) string {
	lines := strings.Split(strings.TrimSpace(body), "\n")
	var docs []string
	for i := 0; i < len(lines); i++ {
		if strings.HasPrefix(lines[i], `{"index"`) {
			if i+1 < len(lines) {
				docs = append(docs, lines[i+1])
			}
		}
	}
	return strings.Join(flusherparity.NormalizeJSONLines(toBytes(docs)), "\n")
}

func toBytes(lines []string) [][]byte {
	out := make([][]byte, len(lines))
	for i, line := range lines {
		out[i] = []byte(line)
	}
	return out
}

func TestFlusherElasticSearch_FlushExportParity(t *testing.T) {
	v1Groups, v2Groups := flusherparity.BuildCustomSingleParityFixtures()

	flushCapture := &bulkBodyCapture{}
	flushServer := httptest.NewServer(http.HandlerFunc(flushCapture.handler))
	defer flushServer.Close()
	flushFlusher := newElasticSearchParityFlusher(t, flushServer)
	require.NoError(t, flushFlusher.Flush("", "", "", v1Groups))

	exportCapture := &bulkBodyCapture{}
	exportServer := httptest.NewServer(http.HandlerFunc(exportCapture.handler))
	defer exportServer.Close()
	exportFlusher := newElasticSearchParityFlusher(t, exportServer)
	require.NoError(t, exportFlusher.Export(v2Groups, nil))

	require.Equal(t, normalizeBulkBody(flushCapture.snapshot()[0]), normalizeBulkBody(exportCapture.snapshot()[0]))
}
