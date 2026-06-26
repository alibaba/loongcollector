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

package doris

import (
	"io"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/plugins/test/flusherparity"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

type dorisLoadCapture struct {
	mu      sync.Mutex
	payload string
}

func (c *dorisLoadCapture) handler(w http.ResponseWriter, r *http.Request) {
	body, _ := io.ReadAll(r.Body)
	c.mu.Lock()
	c.payload = string(body)
	c.mu.Unlock()
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	_, _ = w.Write([]byte(`{"Status":"Success","NumberTotalRows":1,"NumberLoadedRows":1,"NumberFilteredRows":0,"NumberUnselectedRows":0,"LoadBytes":1,"LoadTimeMs":1,"Label":"test"}`))
}

func (c *dorisLoadCapture) snapshot() string {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.payload
}

func newDorisParityFlusher(t *testing.T, server *httptest.Server) *FlusherDoris {
	t.Helper()
	cvt, err := flusherparity.NewCustomSingleConverter()
	require.NoError(t, err)

	f := NewFlusherDoris()
	f.context = mock.NewEmptyContext("p", "l", "c")
	f.converter = cvt
	f.Addresses = []string{server.URL}
	f.Database = "test_db"
	f.Table = "test_table"
	f.Concurrency = 1
	f.GroupCommit = "off"
	f.Authentication.PlainText = &PlainTextConfig{Username: "root", Password: ""}
	require.NoError(t, f.initDorisClient())
	return f
}

func normalizeDorisPayload(payload string) []string {
	lines := strings.Split(strings.TrimSpace(payload), "\n")
	out := make([][]byte, 0, len(lines))
	for _, line := range lines {
		if line != "" {
			out = append(out, []byte(line))
		}
	}
	return flusherparity.NormalizeJSONLines(out)
}

func TestFlusherDoris_FlushExportParity(t *testing.T) {
	v1Groups, v2Groups := flusherparity.BuildCustomSingleParityFixtures()

	flushCapture := &dorisLoadCapture{}
	flushServer := httptest.NewServer(http.HandlerFunc(flushCapture.handler))
	defer flushServer.Close()
	flushFlusher := newDorisParityFlusher(t, flushServer)
	require.NoError(t, flushFlusher.Flush("", "", "", v1Groups))

	exportCapture := &dorisLoadCapture{}
	exportServer := httptest.NewServer(http.HandlerFunc(exportCapture.handler))
	defer exportServer.Close()
	exportFlusher := newDorisParityFlusher(t, exportServer)
	require.NoError(t, exportFlusher.Export(v2Groups, nil))

	require.Equal(t, normalizeDorisPayload(flushCapture.snapshot()), normalizeDorisPayload(exportCapture.snapshot()))
}
