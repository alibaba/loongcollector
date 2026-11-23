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

package helper

import (
	"math"
	"testing"
	"time"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

func TestInputEventFromProtocolLog(t *testing.T) {
	ns := uint32(123)
	log := InputEventFromProtocolLog(&protocol.Log{Time: 10, TimeNs: &ns, Contents: []*protocol.Log_Content{
		{Key: "content", Value: "body"},
		{Key: "__tag__:__file_offset__", Value: "10"},
		{Key: "__tag__:tenant", Value: "default"},
	}}).(*models.Log)
	require.Equal(t, uint64(10_000_000_123), log.GetTimestamp())
	require.Equal(t, uint64(10), log.GetOffset())
	require.Equal(t, []byte("body"), log.GetBody())
	require.Equal(t, "default", log.GetTags().Get("tenant"))
	require.Equal(t, 1, log.GetIndices().Len())

	labels := &MetricLabels{}
	labels.Append("city", "hz")
	metric := InputEventFromProtocolLog(NewMetricLog("counter", time.Unix(10, 20).UnixNano(), 42, labels)).(*models.Metric)
	require.Equal(t, "counter", metric.Name)
	require.Equal(t, "hz", metric.GetTags().Get("city"))
	require.Equal(t, float64(42), metric.Value.GetSingleValue())

	stale := InputEventFromProtocolLog(NewMetricLog("stale", time.Now().UnixNano(), math.Float64frombits(StaleNaN), &MetricLabels{})).(*models.Metric)
	require.Equal(t, StaleNaN, math.Float64bits(stale.Value.GetSingleValue()))
}

func TestNewInputLogRowRejectsMismatchedColumns(t *testing.T) {
	for _, values := range [][]string{nil, {"one", "extra"}} {
		log, err := NewInputLogRow(nil, []string{"one"}, values)
		require.Error(t, err)
		require.Nil(t, log)
	}
}

func TestNewInputLogDoesNotShareInputFields(t *testing.T) {
	fields := map[string]string{"content": "body", "field": "value"}
	for _, name := range []string{"a", "b", "c", "d", "e", "f", "g"} {
		fields[name] = name
	}
	first := NewInputLog(nil, fields)
	second := NewInputLog(nil, fields)

	first.GetBody()[0] = 'B'
	first.Contents.Add("field", "changed")
	require.Equal(t, "body", fields["content"])
	require.Equal(t, "value", fields["field"])
	require.Equal(t, []byte("body"), second.GetBody())
	require.Equal(t, "value", second.Contents.Get("field"))
	require.Equal(t, len(fields), second.Contents.Len())
}
