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

package clickhouse

import (
	"context"
	"strings"
	"testing"

	"github.com/ClickHouse/clickhouse-go/v2/lib/driver"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/plugins/test/flusherparity"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

type captureClickHouseConn struct {
	logs [][]byte
}

func (c *captureClickHouseConn) Contributors() []string { return nil }

func (c *captureClickHouseConn) ServerVersion() (*driver.ServerVersion, error) { return nil, nil }

func (c *captureClickHouseConn) Select(_ context.Context, _ interface{}, _ string, _ ...interface{}) error {
	return nil
}

func (c *captureClickHouseConn) Query(_ context.Context, _ string, _ ...interface{}) (driver.Rows, error) {
	return nil, nil
}

func (c *captureClickHouseConn) QueryRow(_ context.Context, _ string, _ ...interface{}) driver.Row {
	return nil
}

func (c *captureClickHouseConn) PrepareBatch(_ context.Context, _ string) (driver.Batch, error) {
	return nil, nil
}

func (c *captureClickHouseConn) Exec(_ context.Context, _ string, _ ...interface{}) error { return nil }

func (c *captureClickHouseConn) AsyncInsert(_ context.Context, query string, _ bool) error {
	const marker = ", '"
	idx := strings.Index(query, marker)
	if idx < 0 {
		return nil
	}
	start := idx + len(marker)
	end := strings.LastIndex(query, "')")
	if end <= start {
		return nil
	}
	c.logs = append(c.logs, []byte(query[start:end]))
	return nil
}

func (c *captureClickHouseConn) Ping(_ context.Context) error { return nil }

func (c *captureClickHouseConn) Stats() driver.Stats { return driver.Stats{} }

func (c *captureClickHouseConn) Close() error { return nil }

func newClickHouseParityFlusher(t *testing.T, conn *captureClickHouseConn) *FlusherClickHouse {
	t.Helper()
	cvt, err := flusherparity.NewCustomSingleConverter()
	require.NoError(t, err)

	f := NewFlusherClickHouse()
	f.context = mock.NewEmptyContext("p", "l", "c")
	f.converter = cvt
	f.conn = conn
	f.Table = "demo"
	f.Authentication.PlainText.Database = "default"
	f.flusher = f.BufferFlush
	return f
}

func TestFlusherClickHouse_FlushExportParity(t *testing.T) {
	v1Groups, v2Groups := flusherparity.BuildCustomSingleParityFixtures()

	flushConn := &captureClickHouseConn{}
	flushFlusher := newClickHouseParityFlusher(t, flushConn)
	require.NoError(t, flushFlusher.Flush("", "", "", v1Groups))

	exportConn := &captureClickHouseConn{}
	exportFlusher := newClickHouseParityFlusher(t, exportConn)
	require.NoError(t, exportFlusher.Export(v2Groups, nil))

	require.Equal(t, flusherparity.NormalizeJSONLines(flushConn.logs), flusherparity.NormalizeJSONLines(exportConn.logs))
}
