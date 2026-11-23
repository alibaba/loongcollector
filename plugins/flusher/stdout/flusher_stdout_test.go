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

package stdout

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	jsoniter "github.com/json-iterator/go"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestWriteLogBodyContentEncoding(t *testing.T) {
	tests := []struct {
		name    string
		value   any
		decoded any
		raw     string
	}{
		{name: "empty string", value: "", decoded: ""},
		{
			name:    "escaped string",
			value:   "你好 <&> \"quote\" \\ \t\n\r \x00\x1b\x7f \u2028\u2029",
			decoded: "你好 <&> \"quote\" \\ \t\n\r \x00\x1b\x7f \u2028\u2029",
		},
		{name: "integer fallback", value: 42, decoded: float64(42)},
		{name: "boolean fallback", value: true, decoded: true},
		{name: "byte slice fallback", value: []byte{0xff}, raw: "\"field\":[]byte{0xff}"},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			log := &models.Log{Contents: models.NewLogContents()}
			log.Contents.Add("field", test.value)
			writer := jsoniter.NewStream(jsoniter.ConfigDefault, nil, 128)
			writer.WriteObjectStart()
			new(FlusherStdout).writeLogBody(writer, log)
			writer.WriteObjectEnd()

			if test.raw != "" {
				require.Contains(t, string(writer.Buffer()), test.raw)
				return
			}
			var fields map[string]any
			require.NoError(t, json.Unmarshal(writer.Buffer(), &fields))
			require.Equal(t, test.decoded, fields["field"])
		})
	}
}

func TestExportPreservesEachQueuedEvent(t *testing.T) {
	path := filepath.Join(t.TempDir(), "events.log")
	flusher := &FlusherStdout{FileName: path}
	require.NoError(t, flusher.Init(mock.NewEmptyContext("p", "l", "c")))
	group := &models.PipelineGroupEvents{}
	messages := []string{strings.Repeat("first % message ", 20), "second % message"}
	for _, message := range messages {
		fields := models.NewLogContents()
		fields.Add("message", message)
		group.Events = append(group.Events, &models.Log{TraceID: "trace-1", Tags: models.NewTags(), Contents: fields})
	}

	require.NoError(t, flusher.Export([]*models.PipelineGroupEvents{group}, nil))
	require.NoError(t, flusher.Stop())
	content, err := os.ReadFile(path)
	require.NoError(t, err)
	lines := strings.Split(strings.TrimSpace(string(content)), "\n")
	require.Len(t, lines, len(messages))
	for i, line := range lines {
		prefix, payload, found := strings.Cut(line, "{")
		require.True(t, found)
		_, err := time.Parse("2006-01-02 15:04:05", strings.TrimSpace(prefix))
		require.NoError(t, err)
		require.Equal(t, 1, strings.Count(payload, "\"traceID\":"))
		var fields map[string]any
		require.NoError(t, json.Unmarshal([]byte("{"+payload), &fields))
		require.Equal(t, messages[i], fields["message"])
		require.Equal(t, "trace-1", fields["traceID"])
	}
}

func TestFileOutputPreservesRotationAtEachRecord(t *testing.T) {
	path := filepath.Join(t.TempDir(), "events.log")
	flusher := &FlusherStdout{FileName: path, MaxSize: 1, MaxRolls: 100, Tags: true}
	require.NoError(t, flusher.Init(mock.NewEmptyContext("p", "l", "c")))
	group := &models.PipelineGroupEvents{Group: models.NewGroup(nil, models.NewTagsWithKeyValues("source", "test"))}
	want := make(map[string]bool)
	for i := 0; i < 65; i++ {
		message := fmt.Sprintf("line %d", i)
		if i == 0 {
			message += strings.Repeat(" content", 9000)
		}
		want[message] = true
		log := &models.Log{Tags: models.NewTags(), Contents: models.NewLogContents()}
		log.Contents.Add("message", message)
		group.Events = append(group.Events, log)
	}
	require.NoError(t, flusher.Export([]*models.PipelineGroupEvents{group}, nil))
	require.NoError(t, flusher.Stop())
	require.NoError(t, flusher.Stop())

	files, err := filepath.Glob(path + "*")
	require.NoError(t, err)
	require.Len(t, files, 66)
	var headings int
	for _, filename := range files {
		data, err := os.ReadFile(filename)
		require.NoError(t, err)
		require.Equal(t, 1, strings.Count(string(data), "\n"))
		require.Greater(t, len(data), len("2006-01-02 15:04:05 "))
		_, err = time.Parse("2006-01-02 15:04:05", string(data[:19]))
		require.NoError(t, err)
		payload := data[20:]
		if strings.HasPrefix(string(payload), "[Event]") {
			headings++
			require.Equal(t, "[Event] event 65, metadata map[], tags map[source:test]\n", string(payload))
			continue
		}
		var fields map[string]any
		require.NoError(t, json.Unmarshal(payload, &fields))
		message := fields["message"].(string)
		require.Contains(t, want, message)
		delete(want, message)
	}
	require.Equal(t, 1, headings)
	require.Empty(t, want)
}
