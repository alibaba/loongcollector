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
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"testing"
)

func TestMetricLabels_Append(t *testing.T) {
	var ml MetricLabels
	ml.Append("key2", "val")
	ml.Append("key", "val")
	log := NewMetricLog("name", 1691646109945, 1, &ml)
	require.Equal(t, `Time:1691646109 Contents:<Key:"__name__" Value:"name" > Contents:<Key:"__time_nano__" Value:"1691646109945000000" > Contents:<Key:"__labels__" Value:"key#$#val|key2#$#val" > Contents:<Key:"__value__" Value:"1" > Time_ns:945000000 `, log.String())

	ml.Replace("key", "val2")

	log = NewMetricLog("name", 1691646109945, 1, &ml)
	require.Equal(t, `Time:1691646109 Contents:<Key:"__name__" Value:"name" > Contents:<Key:"__time_nano__" Value:"1691646109945000000" > Contents:<Key:"__labels__" Value:"key#$#val2|key2#$#val" > Contents:<Key:"__value__" Value:"1" > Time_ns:945000000 `, log.String())

	ml.Replace("key3", "val3")
	log = NewMetricLog("name", 1691646109945, 1, &ml)
	require.Equal(t, `Time:1691646109 Contents:<Key:"__name__" Value:"name" > Contents:<Key:"__time_nano__" Value:"1691646109945000000" > Contents:<Key:"__labels__" Value:"key#$#val2|key2#$#val|key3#$#val3" > Contents:<Key:"__value__" Value:"1" > Time_ns:945000000 `, log.String())

	cloneLabel := ml.Clone()
	cloneLabel.Replace("key3", "val4")
	log = NewMetricLog("name", 1691646109945, 1, cloneLabel)
	require.Equal(t, `Time:1691646109 Contents:<Key:"__name__" Value:"name" > Contents:<Key:"__time_nano__" Value:"1691646109945000000" > Contents:<Key:"__labels__" Value:"key#$#val2|key2#$#val|key3#$#val4" > Contents:<Key:"__value__" Value:"1" > Time_ns:945000000 `, log.String())

	log = NewMetricLog("name", 1691646109945, 1, &ml)
	require.Equal(t, `Time:1691646109 Contents:<Key:"__name__" Value:"name" > Contents:<Key:"__time_nano__" Value:"1691646109945000000" > Contents:<Key:"__labels__" Value:"key#$#val2|key2#$#val|key3#$#val3" > Contents:<Key:"__value__" Value:"1" > Time_ns:945000000 `, log.String())

	log = NewMetricLog("name", 1691646109945, 1, nil)
	require.Equal(t, `Time:1691646109 Contents:<Key:"__name__" Value:"name" > Contents:<Key:"__time_nano__" Value:"1691646109945000000" > Contents:<Key:"__labels__" Value:"" > Contents:<Key:"__value__" Value:"1" > Time_ns:945000000 `, log.String())

	log = NewMetricLog("name", 1691646109945, 1, nil)
	require.Equal(t, `Time:1691646109 Contents:<Key:"__name__" Value:"name" > Contents:<Key:"__time_nano__" Value:"1691646109945000000" > Contents:<Key:"__labels__" Value:"" > Contents:<Key:"__value__" Value:"1" > Time_ns:945000000 `, log.String())

	var ml2 MetricLabels
	config.LoongcollectorGlobalConfig.EnableSlsMetricsFormat = true
	ml2.Append("key@", "val|")

	log = NewMetricLog("name@", 1691646109945, 1, &ml2)
	require.Equal(t, `Time:1691646109 Contents:<Key:"__name__" Value:"name_" > Contents:<Key:"__time_nano__" Value:"1691646109945000000" > Contents:<Key:"__labels__" Value:"key_#$#val_" > Contents:<Key:"__value__" Value:"1" > Time_ns:945000000 `, log.String())

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

func Test_GetMetricName(t *testing.T) {
	type args struct {
		log *protocol.Log
	}
	tests := []struct {
		name string
		args args
		want string
	}{
		{
			name: "test1",
			args: struct {
				log *protocol.Log
			}{log: &protocol.Log{
				Contents: []*protocol.Log_Content{{Key: selfmonitor.SelfMetricNameKey, Value: "metric1"}}},
			},
			want: "metric1",
		},

		{
			name: "test2",
			args: struct {
				log *protocol.Log
			}{log: &protocol.Log{
				Contents: []*protocol.Log_Content{
					{Key: "first", Value: "metric1"},
					{Key: selfmonitor.SelfMetricNameKey, Value: "metric2"},
				}},
			},
			want: "metric2",
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			assert.Equalf(t, tt.want, GetMetricName(tt.args.log), "GetMetricName(%v, %v)", tt.args.log)
		})
	}
}
