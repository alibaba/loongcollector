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

package prometheus

import (
	"math"
	"sort"
	"strconv"
	"strings"
	"time"
	"unsafe"

	"github.com/alibaba/ilogtail/pkg/pipeline"

	"github.com/VictoriaMetrics/VictoriaMetrics/lib/prompbmarshal"
)

const StaleNan = "__STALE_NAN__"

var prometheusKeys []string

const (
	// NormalNaN is a quiet NaN. This is also math.NaN().
	NormalNaN uint64 = 0x7ff8000000000001

	// StaleNaN is a signaling NaN, due to the MSB of the mantissa being 0.
	// This value is chosen with many leading 0s, so we have scope to store more
	// complicated values in the future. It is 2 rather than 1 to make
	// it easier to distinguish from the NormalNaN by a human when debugging.
	StaleNaN uint64 = 0x7ff0000000000002
)

// IsStaleNaN returns true when the provided NaN value is a stale marker.
func IsStaleNaN(v float64) bool {
	return math.Float64bits(v) == StaleNaN
}

func init() {
	prometheusKeys = append(prometheusKeys, "__name__")
	prometheusKeys = append(prometheusKeys, "__labels__")
	prometheusKeys = append(prometheusKeys, "__time_nano__")
	prometheusKeys = append(prometheusKeys, "__value__")
}

func formatLabelKey(key string) string {
	var newKey []byte
	for i := 0; i < len(key); i++ {
		b := key[i]
		if (b >= 'a' && b <= 'z') ||
			(b >= 'A' && b <= 'Z') ||
			(b >= '0' && b <= '9') ||
			b == '_' {
			continue
		} else {
			if newKey == nil {
				newKey = []byte(key)
			}
			newKey[i] = '_'
		}
	}
	if newKey == nil {
		return key
	}
	return string(newKey)
}

func formatLabelValue(value string) string {
	var newValue []byte
	for i := 0; i < len(value); i++ {
		b := value[i]
		if b != '|' {
			continue
		} else {
			if newValue == nil {
				newValue = []byte(value)
			}
			newValue[i] = '_'
		}
	}
	if newValue == nil {
		return value
	}
	return string(newValue)
}

func formatNewMetricName(name string) string {
	newName := []byte(name)
	for i, b := range newName {
		if (b >= 'a' && b <= 'z') ||
			(b >= 'A' && b <= 'Z') ||
			(b >= '0' && b <= '9') ||
			b == '_' ||
			b == ':' {
			continue
		} else {
			newName[i] = '_'
		}
	}
	return *(*string)(unsafe.Pointer(&newName))
}

func appendTSDataToSlsLog(c pipeline.Collector, wr *prompbmarshal.WriteRequest) {
	values := make([]string, 4)
	for i := range wr.Timeseries {
		ts := &wr.Timeseries[i]
		sort.Slice(ts.Labels, func(i, j int) bool {
			return ts.Labels[i].Name < ts.Labels[j].Name
		})
		i := 0
		var labelsBuilder strings.Builder
		for _, label := range ts.Labels {
			if label.Name == "__name__" {
				// __name__
				// must call new for metric name, because Name is the reference of raw buffer
				values[0] = formatNewMetricName(label.Value)
				continue
			}
			if i != 0 {
				labelsBuilder.WriteByte('|')
			}
			labelsBuilder.WriteString(formatLabelKey(label.Name))
			labelsBuilder.WriteString("#$#")
			labelsBuilder.WriteString(formatLabelValue(label.Value))
			i++
		}
		values[1] = labelsBuilder.String()

		for _, sample := range ts.Samples {
			values[2] = strconv.FormatInt(sample.Timestamp, 10)
			if IsStaleNaN(sample.Value) {
				values[3] = StaleNan
			} else {
				values[3] = strconv.FormatFloat(sample.Value, 'g', -1, 64)
			}
			c.AddDataArray(nil, prometheusKeys, values, time.Unix(sample.Timestamp/(1e3), 0))
		}
	}

}
