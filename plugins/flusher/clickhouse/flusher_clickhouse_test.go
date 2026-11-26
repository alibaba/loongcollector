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

package clickhouse

import (
	"context"
	"encoding/json"
	"sort"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/ClickHouse/clickhouse-go/v2/lib/driver"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

// Invalid Test
func InvalidTestConnectAndWrite(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	f := NewFlusherClickHouse()
	f.Addresses = []string{"127.0.0.1:9000"}
	f.Authentication.PlainText.Username = ""
	f.Authentication.PlainText.Password = ""
	f.Authentication.PlainText.Database = "default"
	f.Cluster = ""
	f.Table = "demo"
	// Verify that we can connect to the ClickHouse
	lctx := mock.NewEmptyContext("p", "l", "c")
	err := f.Init(lctx)
	require.NoError(t, err)

	// Verify that we can successfully write data to the ClickHouse buffer engine table
	groups := makeTestPipelineEventGroups()
	err = f.Export(groups, nil)
	require.NoError(t, err)
	_ = f.Stop()
}

func makeTestPipelineEventGroups() []*models.PipelineGroupEvents {
	groups := make([]*models.PipelineGroupEvents, 0, 10)
	timestamp := uint64(time.Now().Unix()) * uint64(time.Second)
	for i := 1; i <= 10; i++ {
		group := &models.PipelineGroupEvents{
			Group:  models.NewGroup(models.NewMetadata(), models.NewTagsWithKeyValues("host.ip", "")),
			Events: make([]models.PipelineEvent, 0, 10),
		}
		for j := 1; j <= 10; j++ {
			log := models.NewSimpleLog(nil, models.NewTags(), timestamp)
			log.GetIndices().Add("group", strconv.Itoa(i))
			log.GetIndices().Add("message", "The message: "+strconv.Itoa(j))
			group.Events = append(group.Events, log)
		}
		groups = append(groups, group)
	}
	return groups
}

func newCustomSingleConverter(t *testing.T) *converter.Converter {
	t.Helper()
	cvt, err := converter.NewConverter(
		converter.ProtocolCustomSingle,
		converter.EncodingJSON,
		nil,
		nil,
	)
	require.NoError(t, err)
	return cvt
}

func normalizeJSONLines(lines [][]byte) []string {
	out := make([]string, 0, len(lines))
	for _, line := range lines {
		var obj map[string]interface{}
		if err := json.Unmarshal(line, &obj); err != nil {
			out = append(out, string(line))
			continue
		}
		normalized, err := json.Marshal(obj)
		if err != nil {
			out = append(out, string(line))
			continue
		}
		out = append(out, string(normalized))
	}
	sort.Strings(out)
	return out
}

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
	f := NewFlusherClickHouse()
	f.context = mock.NewEmptyContext("p", "l", "c")
	f.converter = newCustomSingleConverter(t)
	f.conn = conn
	f.Table = "demo"
	f.Authentication.PlainText.Database = "default"
	return f
}

// clickHouseUnescapeStringLiteral mimics how ClickHouse's single-quoted string
// literal parser consumes backslash escape sequences, so the test can assert
// that the stored bytes are valid JSON after the server unescapes them.
func clickHouseUnescapeStringLiteral(s string) string {
	var b strings.Builder
	for i := 0; i < len(s); i++ {
		if s[i] == '\\' && i+1 < len(s) {
			i++
			b.WriteByte(s[i])
			continue
		}
		b.WriteByte(s[i])
	}
	return b.String()
}

// TestFlusherClickHouse_ExportMetricStructural reproduces the v2 e2e scenario
// (metric_mock -> Export): a Metric event is converted structurally into a
// canonical metric log (__name__/__labels__/__value__), never wrapped in a
// pass-through blob. The single-quote in the label value also stresses the
// ClickHouse SQL literal escaping so the stored _log round-trips as valid JSON.
func TestFlusherClickHouse_ExportMetricStructural(t *testing.T) {
	group := models.NewGroup(models.NewMetadata(), models.NewTags())
	metric := models.NewSingleValueMetric("hello", models.MetricTypeGauge,
		models.NewTagsWithKeyValues("region", "cn-'hangzhou'"), 1700000000*1e9, 3.14)
	groups := []*models.PipelineGroupEvents{{Group: group, Events: []models.PipelineEvent{metric}}}

	conn := &captureClickHouseConn{}
	f := newClickHouseParityFlusher(t, conn)
	require.NoError(t, f.Export(groups, nil))
	require.NotEmpty(t, conn.logs, "metric event must not be silently dropped")

	for _, raw := range conn.logs {
		stored := clickHouseUnescapeStringLiteral(string(raw))
		var obj map[string]interface{}
		require.NoErrorf(t, json.Unmarshal([]byte(stored), &obj),
			"stored _log must be valid JSON after ClickHouse unescapes the literal: %s", stored)
		contents, ok := obj["contents"].(map[string]interface{})
		require.True(t, ok, "entry must contain contents")
		require.NotContains(t, contents, "__pipeline_passthrough__",
			"metric event must not be pass-through serialized")
		require.Equal(t, "hello", contents["__name__"], "metric name must be preserved structurally")
		require.Equal(t, "3.14", contents["__value__"], "metric value must be preserved structurally")
		require.Contains(t, contents["__labels__"], "cn-'hangzhou'", "metric labels must survive SQL escaping")
	}
}

func TestFlusherClickHouseExportLogs(t *testing.T) {
	groups := makeTestPipelineEventGroups()[:1]
	capture := &captureClickHouseConn{}
	flusher := newClickHouseParityFlusher(t, capture)
	require.NoError(t, flusher.Export(groups, nil))

	expected := expectedLogRecords(t, groups)
	require.Equal(t, expected, normalizeJSONLines(capture.logs))
}

// expectedLogRecords describes the external JSON record for the log fixtures.
func expectedLogRecords(t *testing.T, groups []*models.PipelineGroupEvents) []string {
	t.Helper()
	var rows [][]byte
	for _, group := range groups {
		for _, event := range group.Events {
			log := event.(*models.Log)
			fields := log.GetIndices().Iterator()
			data, err := json.Marshal(map[string]any{
				"contents": fields, "tags": group.Group.GetTags().Iterator(), "time": log.Timestamp / uint64(time.Second),
			})
			require.NoError(t, err)
			rows = append(rows, data)
		}
	}
	return normalizeJSONLines(rows)
}
