// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package helper

import (
	"math"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/models"

	"github.com/stretchr/testify/require"

	"testing"
)

func TestMetricLabels_Append(t *testing.T) {
	prior := config.LoongcollectorGlobalConfig.EnableSlsMetricsFormat
	t.Cleanup(func() { config.LoongcollectorGlobalConfig.EnableSlsMetricsFormat = prior })
	config.LoongcollectorGlobalConfig.EnableSlsMetricsFormat = true

	var labels MetricLabels
	labels.Append("key2", "val")
	labels.Append("key", "val")
	require.Equal(t, "key#$#val|key2#$#val", labels.String())

	labels.Replace("key", "val2")
	require.Equal(t, "key#$#val2|key2#$#val", labels.String())
	labels.Replace("key3", "val3")
	require.Equal(t, "key#$#val2|key2#$#val|key3#$#val3", labels.String())

	clone := labels.Clone()
	clone.Replace("key3", "val4")
	require.Equal(t, "key#$#val2|key2#$#val|key3#$#val4", clone.String())
	require.Equal(t, "key#$#val2|key2#$#val|key3#$#val3", labels.String())

	var empty *MetricLabels
	require.Empty(t, empty.String())
	var escaped MetricLabels
	escaped.Append("key@", "val|")
	require.Equal(t, "key_#$#val_", escaped.String())
}

func TestNewInputMetric(t *testing.T) {
	prior := config.LoongcollectorGlobalConfig.EnableSlsMetricsFormat
	t.Cleanup(func() { config.LoongcollectorGlobalConfig.EnableSlsMetricsFormat = prior })

	for _, test := range []struct {
		name      string
		format    bool
		timestamp int64
		wantTime  uint64
		wantName  string
		wantKey   string
		wantValue string
	}{
		{"formats millisecond input", true, 1691646109945, 1691646109945000000, "test_", "a_", "last_value"},
		{"preserves nanosecond input", false, 1691646109945123456, 1691646109945123456, "test@", "a@", "last|value"},
		{"normalizes second input", true, 1691646109, 1691646109000000000, "test_", "a_", "last_value"},
	} {
		t.Run(test.name, func(t *testing.T) {
			config.LoongcollectorGlobalConfig.EnableSlsMetricsFormat = test.format
			var labels MetricLabels
			labels.Append("a@", "first")
			labels.Append("a@", "last|value")
			metric := NewInputMetric("test@", test.timestamp, math.Float64frombits(StaleNaN), &labels)
			require.Equal(t, test.wantTime, metric.GetTimestamp())
			require.Equal(t, test.wantName, metric.GetName())
			require.Equal(t, models.MetricTypeUntyped, metric.GetMetricType())
			require.Equal(t, map[string]string{test.wantKey: test.wantValue}, metric.GetTags().Iterator())
			require.Equal(t, StaleNaN, math.Float64bits(metric.GetValue().GetSingleValue()))

			labels.Replace("a@", "updated")
			require.Equal(t, test.wantValue, metric.GetTags().Get(test.wantKey))
		})
	}
}
