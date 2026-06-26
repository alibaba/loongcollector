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

package exportutil

import (
	"github.com/alibaba/ilogtail/pkg/models"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
)

// PartitionForConverterProtocol splits events for converter-based flushers.
// influxdb handles Metric natively; raw handles ByteArray; other kinds pass through ExportLogOnly.
func PartitionForConverterProtocol(protocol string, events []models.PipelineEvent) (converterEvents, passthroughEvents []models.PipelineEvent) {
	switch protocol {
	case converter.ProtocolInfluxdb:
		for _, event := range events {
			if event.GetType() == models.EventTypeMetric {
				converterEvents = append(converterEvents, event)
			} else {
				passthroughEvents = append(passthroughEvents, event)
			}
		}
	case converter.ProtocolRaw:
		for _, event := range events {
			if event.GetType() == models.EventTypeByteArray {
				converterEvents = append(converterEvents, event)
			} else {
				passthroughEvents = append(passthroughEvents, event)
			}
		}
	default:
		return PartitionEvents(events, LogOnlyEventKinds)
	}
	return converterEvents, passthroughEvents
}
