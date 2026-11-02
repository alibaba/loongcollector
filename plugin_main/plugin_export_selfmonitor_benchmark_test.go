// Copyright 2026 iLogtail Authors
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

package main

import (
	"fmt"
	"testing"
	"time"

	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

func BenchmarkSelfMonitorMetricsPB(b *testing.B) {
	for _, count := range []int{0, 1, 100} {
		b.Run(fmt.Sprint(count), func(b *testing.B) {
			records := make([]map[string]string, count)
			for i := range records {
				records[i] = map[string]string{
					"labels":   fmt.Sprintf("{\"metric_category\":\"plugin\",\"plugin_type\":\"flusher_stdout\",\"pipeline_name\":\"config-%d\"}", i),
					"counters": "{\"in_events_total\":\"100\",\"out_events_total\":\"90\"}",
					"gauges":   "{\"queue_size\":\"12.2500\"}",
				}
			}
			now := time.Unix(1700000000, 0)
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				buffer := toPBBuffer(buildMetricsPB(records, now))
				FreeSelfMonitorPB(buffer.data)
			}
		})
	}
}

func BenchmarkSelfMonitorAlarmsPB(b *testing.B) {
	for _, count := range []int{0, 1, 100} {
		b.Run(fmt.Sprint(count), func(b *testing.B) {
			alarms := make([]selfmonitor.AlarmExportMessage, count)
			for i := range alarms {
				alarms[i] = selfmonitor.AlarmExportMessage{
					AlarmType: "PARSE_ERROR_ALARM", AlarmLevel: "2", AlarmMessage: "parse failed",
					Count: 3, ProjectName: "project", Category: "store", Config: "config/2",
				}
			}
			now := time.Unix(1700000000, 0)
			b.ReportAllocs()
			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				buffer := toPBBuffer(buildAlarmsPB(alarms, now))
				FreeSelfMonitorPB(buffer.data)
			}
		})
	}
}
