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

package prometheus

import (
	"context"
	"sort"
	"sync"

	pb "github.com/VictoriaMetrics/VictoriaMetrics/lib/prompbmarshal"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
)

const metricNameKey = "__name__"

func marshalBatchTimeseriesData(wr *pb.WriteRequest) []byte {
	if len(wr.Timeseries) == 0 {
		return nil
	}

	data, err := wr.Marshal()
	if err != nil {
		// logger.Error(context.Background(), alarmType, "pb marshal err", err)
		return nil
	}

	return data
}

func genPromRemoteWriteTimeseries(event *models.Metric) pb.TimeSeries {
	return pb.TimeSeries{
		Labels: lexicographicalSort(append(convTagsToLabels(event.GetTags()), pb.Label{Name: metricNameKey, Value: event.GetName()})),
		Samples: []pb.Sample{{
			Value: event.GetValue().GetSingleValue(),

			// Decode (during input_prometheus stage) makes timestamp
			// with unix milliseconds into unix nanoseconds,
			// e.g. "model.Time(milliseconds).Time().UnixNano()".
			//
			// Encode (during flusher_prometheus stage) conversely makes timestamp
			// with unix nanoseconds into unix milliseconds,
			// e.g. "int64(nanoseconds)/10^6".
			Timestamp: int64(event.GetTimestamp()) / 1e6,
		}},
	}
}

func convTagsToLabels(tags models.Tags) []pb.Label {
	if tags == nil {
		logger.Debugf(context.Background(), "get nil models.Tags")
		return nil
	}

	labels := make([]pb.Label, 0, tags.Len())
	for k, v := range tags.Iterator() {
		// MUST NOT contain any empty label names or values.
		// reference: https://prometheus.io/docs/specs/remote_write_spec/#labels
		if k != "" && v != "" {
			labels = append(labels, pb.Label{Name: k, Value: v})
		}
	}

	return labels
}

// MUST have label names sorted in lexicographical order.
// reference: https://prometheus.io/docs/specs/remote_write_spec/#labels
func lexicographicalSort(labels []pb.Label) []pb.Label {
	sort.Sort(promLabels(labels))

	return labels
}

type promLabels []pb.Label

func (p promLabels) Len() int {
	return len(p)
}

func (p promLabels) Less(i, j int) bool {
	return p[i].Name < p[j].Name
}

func (p promLabels) Swap(i, j int) {
	p[i], p[j] = p[j], p[i]
}

var wrPool sync.Pool

func getWriteRequest(seriesLimit int) *pb.WriteRequest {
	wr := wrPool.Get()
	if wr == nil {
		return &pb.WriteRequest{
			Timeseries: make([]pb.TimeSeries, 0, seriesLimit),
		}
	}

	return wr.(*pb.WriteRequest)
}

func putWriteRequest(wr *pb.WriteRequest) {
	wr.Timeseries = wr.Timeseries[:0]
	wrPool.Put(wr)
}
