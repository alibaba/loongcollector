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

	"github.com/ClickHouse/clickhouse-go/v2/lib/driver"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
	"github.com/alibaba/ilogtail/plugins/test"
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
	f.flusher = f.BufferFlush
	// Verify that we can connect to the ClickHouse
	lctx := mock.NewEmptyContext("p", "l", "c")
	err := f.Init(lctx)
	require.NoError(t, err)

	// Verify that we can successfully write data to the ClickHouse buffer engine table
	lgl := makeTestLogGroupList()
	err = f.Flush("projectName", "logstoreName", "configName", lgl.GetLogGroupList())
	require.NoError(t, err)
	_ = f.Stop()
}

func makeTestLogGroupList() *protocol.LogGroupList {
	f := map[string]string{}
	lgl := &protocol.LogGroupList{
		LogGroupList: make([]*protocol.LogGroup, 0, 10),
	}
	for i := 1; i <= 10; i++ {
		lg := &protocol.LogGroup{
			Logs:   make([]*protocol.Log, 0, 10),
			Source: "",
		}
		for j := 1; j <= 10; j++ {
			f["group"] = strconv.Itoa(i)
			f["message"] = "The message: " + strconv.Itoa(j)
			l := test.CreateLogByFields(f)
			lg.Logs = append(lg.Logs, l)
		}
		lgl.LogGroupList = append(lgl.LogGroupList, lg)
	}
	return lgl
}

func makeParityLogGroups() ([]*protocol.LogGroup, []*models.PipelineGroupEvents) {
	v1Groups := makeTestLogGroupList().GetLogGroupList()[:1]
	return v1Groups, makePipelineGroupEventsFromLogGroups(v1Groups)
}

func makePipelineGroupEventsFromLogGroups(v1Groups []*protocol.LogGroup) []*models.PipelineGroupEvents {
	v2Groups := make([]*models.PipelineGroupEvents, 0, len(v1Groups))
	for _, lg := range v1Groups {
		groupTags := models.NewTags()
		groupTags.Add("host.ip", lg.Source)
		for _, tag := range lg.LogTags {
			groupTags.Add(tag.Key, tag.Value)
		}
		group := models.NewGroup(models.NewMetadata(), groupTags)
		events := make([]models.PipelineEvent, 0, len(lg.Logs))
		for _, log := range lg.Logs {
			ts := uint64(log.GetTime()) * 1e9
			if log.TimeNs != nil {
				ts += uint64(*log.TimeNs)
			}
			v2Log := models.NewLog("", nil, "", "", "", models.NewTags(), ts)
			for _, c := range log.Contents {
				v2Log.GetIndices().Add(c.Key, c.Value)
			}
			events = append(events, v2Log)
		}
		v2Groups = append(v2Groups, &models.PipelineGroupEvents{Group: group, Events: events})
	}
	return v2Groups
}

func newCustomSingleConverter(t *testing.T) *converter.Converter {
	t.Helper()
	cvt, err := converter.NewConverter(
		converter.ProtocolCustomSingle,
		converter.EncodingJSON,
		nil,
		nil,
		&config.GlobalConfig{},
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
	f.flusher = f.BufferFlush
	return f
}

func TestFlusherClickHouse_FlushExportParity(t *testing.T) {
	v1Groups, v2Groups := makeParityLogGroups()

	flushConn := &captureClickHouseConn{}
	flushFlusher := newClickHouseParityFlusher(t, flushConn)
	require.NoError(t, flushFlusher.Flush("", "", "", v1Groups))

	exportConn := &captureClickHouseConn{}
	exportFlusher := newClickHouseParityFlusher(t, exportConn)
	require.NoError(t, exportFlusher.Export(v2Groups, nil))

	require.Equal(t, normalizeJSONLines(flushConn.logs), normalizeJSONLines(exportConn.logs))
}
