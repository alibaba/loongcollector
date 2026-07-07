// Copyright 2024 iLogtail Authors
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

package verify

import (
	"testing"

	"github.com/stretchr/testify/require"
	"gopkg.in/yaml.v3"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

// metricLog builds a canonical metric-log line as emitted by the v2 converter.
func metricLog(kv ...string) *protocol.Log {
	log := &protocol.Log{}
	for i := 0; i+1 < len(kv); i += 2 {
		log.Contents = append(log.Contents, &protocol.Log_Content{Key: kv[i], Value: kv[i+1]})
	}
	return log
}

func TestLogContainsExactKV(t *testing.T) {
	// A canonical multi-value metric-log row produced by metric_mock via the v2
	// export path: __name__/__value__/__labels__ carry exact values.
	log := metricLog(
		"__name__", "multi_values_metrics_mock__value",
		"__time_nano__", "1691646109945000000",
		"__labels__", "name#$#hello",
		"__value__", "log contents",
	)

	cases := []struct {
		name   string
		expect map[string]string
		want   bool
	}{
		{
			name:   "exact name and value match",
			expect: map[string]string{"__name__": "multi_values_metrics_mock__value", "__value__": "log contents"},
			want:   true,
		},
		{
			name:   "name only match",
			expect: map[string]string{"__name__": "multi_values_metrics_mock__value"},
			want:   true,
		},
		{
			name:   "name with labels match",
			expect: map[string]string{"__name__": "multi_values_metrics_mock__value", "__labels__": "name#$#hello"},
			want:   true,
		},
		{
			name:   "value mismatch is rejected (no substring/regex leniency)",
			expect: map[string]string{"__name__": "multi_values_metrics_mock__value", "__value__": "log"},
			want:   false,
		},
		{
			name:   "wrong name is rejected",
			expect: map[string]string{"__name__": "single_metrics_mock"},
			want:   false,
		},
		{
			name:   "missing key is rejected",
			expect: map[string]string{"__name__": "multi_values_metrics_mock__value", "nonexistent": "x"},
			want:   false,
		},
	}

	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			require.Equal(t, c.want, logContainsExactKV(log, c.expect))
		})
	}
}

// TestExactKVDocstringParsing verifies the docstring format used in the feature
// files unmarshals into a list of exact key-value records.
func TestExactKVDocstringParsing(t *testing.T) {
	docstring := `
- __name__: single_metrics_mock
- __name__: multi_values_metrics_mock__name
  __value__: hello
- __name__: multi_values_metrics_mock__value
  __value__: log contents
`
	records := make([]map[string]string, 0)
	require.NoError(t, yaml.Unmarshal([]byte(docstring), &records))
	require.Len(t, records, 3)
	require.Equal(t, map[string]string{"__name__": "single_metrics_mock"}, records[0])
	require.Equal(t, "hello", records[1]["__value__"])
	require.Equal(t, "log contents", records[2]["__value__"])

	// Each record must be locatable in the flushed logs by exact key-value.
	logs := []*protocol.Log{
		metricLog("__name__", "single_metrics_mock", "__value__", "42"),
		metricLog("__name__", "multi_values_metrics_mock__name", "__value__", "hello"),
		metricLog("__name__", "multi_values_metrics_mock__value", "__value__", "log contents"),
	}
	for _, expect := range records {
		matched := false
		for _, log := range logs {
			if logContainsExactKV(log, expect) {
				matched = true
				break
			}
		}
		require.Truef(t, matched, "record %v not located", expect)
	}
}
