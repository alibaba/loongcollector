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
	"sort"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/golang/snappy"
	"github.com/grafana/loki-client-go/pkg/logproto"
	"github.com/prometheus/common/model"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func makeTestLogGroupList() *protocol.LogGroupList {
	fields := map[string]string{}
	lgl := &protocol.LogGroupList{
		LogGroupList: make([]*protocol.LogGroup, 0, 10),
	}
	for i := 1; i <= 10; i++ {
		lg := &protocol.LogGroup{
			Logs:   make([]*protocol.Log, 0, 10),
			Source: "",
		}
		for j := 1; j <= 10; j++ {
			fields["group"] = strconv.Itoa(i)
			fields["message"] = "The message: " + strconv.Itoa(j)
			l := test.CreateLogByFields(fields)
			lg.Logs = append(lg.Logs, l)
		}
		lgl.LogGroupList = append(lgl.LogGroupList, lg)
	}
	return lgl
}

func makeParityLogGroups() ([]*protocol.LogGroup, []*models.PipelineGroupEvents) {
	lg := makeTestLogGroupList().GetLogGroupList()[0]
	single := &protocol.LogGroup{
		Logs:   []*protocol.Log{lg.Logs[0]},
		Source: "",
	}
	v1Groups := []*protocol.LogGroup{single}
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

func TestFlusherLoki_FlushExportParity(t *testing.T) {
	v1Groups, v2Groups := makeParityLogGroups()

	flushLines := captureLokiPayloads(t, func(f *FlusherLoki) error {
		return f.Flush("", "", "", v1Groups)
	})
	exportLines := captureLokiPayloads(t, func(f *FlusherLoki) error {
		return f.Export(v2Groups, nil)
	})

	require.Equal(t, flushLines, exportLines)
}

func TestFlusherLoki_ExportMetricExpansion(t *testing.T) {
	// A single Metric event expands into several serialized log lines (its single
	// value plus each multi/typed value). Regression guard: the Export loop must
	// not index groupEvents.Events by the serialized-line index, otherwise the
	// multi-value metric drives the index out of range, panics, and nothing
	// reaches loki (the failure seen in the flusher_loki e2e case).
	tags := models.NewTagsWithMap(map[string]string{"name": "hello"})
	ts := int64(1691646109945000000)
	single := models.NewSingleValueMetric("single_metrics_mock", models.MetricTypeCounter, tags, ts, 1)
	values := models.NewMetricMultiValue()
	values.Add("Index", float64(1))
	typedValues := models.NewMetricTypedValues()
	typedValues.Add("value", &models.TypedValue{Type: models.ValueTypeString, Value: "log contents"})
	multi := models.NewMetric("multi_values_metrics_mock", models.MetricTypeUntyped, tags, ts, values, typedValues)

	groups := []*models.PipelineGroupEvents{{
		Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
		Events: []models.PipelineEvent{single, multi},
	}}

	lines := captureLokiEntries(t, func(f *FlusherLoki) error {
		return f.Export(groups, nil)
	})

	// single_metrics_mock -> 1 line; multi_values_metrics_mock -> Index + value -> 2 lines.
	require.Len(t, lines, 3)
	got := map[string]string{}
	for _, line := range lines {
		var obj struct {
			Contents map[string]string `json:"contents"`
		}
		require.NoError(t, json.Unmarshal([]byte(line), &obj))
		got[obj.Contents["__name__"]] = obj.Contents["__value__"]
		require.Contains(t, obj.Contents["__labels__"], "name#$#hello")
	}
	require.Contains(t, got, "single_metrics_mock")
	require.Contains(t, got, "multi_values_metrics_mock_Index")
	require.Equal(t, "log contents", got["multi_values_metrics_mock_value"])
}

// captureLokiEntries runs the flush and decodes the loki push wire format
// (snappy-compressed logproto.PushRequest), returning every log line delivered
// across all batches. Unlike captureLokiPayloads it recovers all lines batched
// into a single request, which is required to verify metric expansion.
func captureLokiEntries(t *testing.T, run func(*FlusherLoki) error) []string {
	t.Helper()
	var bodies [][]byte
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body, _ := io.ReadAll(r.Body)
		bodies = append(bodies, body)
		w.WriteHeader(http.StatusNoContent)
	}))
	defer server.Close()

	f := NewFlusherLoki()
	f.URL = server.URL + "/loki/api/v1/push"
	f.StaticLabels = model.LabelSet{"app": "expansion-test"}
	f.DynamicLabels = []string{}
	f.MaxMessageWait = 10 * time.Millisecond
	require.NoError(t, f.Init(mock.NewEmptyContext("p", "l", "c")))
	require.NoError(t, run(f))
	require.NoError(t, f.Stop())
	time.Sleep(20 * time.Millisecond)

	var lines []string
	for _, body := range bodies {
		decoded, err := snappy.Decode(nil, body)
		require.NoError(t, err)
		var req logproto.PushRequest
		require.NoError(t, req.Unmarshal(decoded))
		for _, stream := range req.Streams {
			for _, entry := range stream.Entries {
				lines = append(lines, entry.Line)
			}
		}
	}
	return lines
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
	return normalizeJSONLines(toBytes(payloads))
}

func extractJSONLogLine(body string) string {
	start := strings.Index(body, `{"contents"`)
	if start < 0 {
		return body
	}
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
	return body[start:]
}

func toBytes(lines []string) [][]byte {
	out := make([][]byte, len(lines))
	for i, line := range lines {
		out[i] = []byte(line)
	}
	return out
}
