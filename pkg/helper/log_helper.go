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

package helper

import (
	"fmt"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

func CreateLog(t time.Time, configTag map[string]string, logTags map[string]string, fields map[string]string) (*protocol.Log, error) {
	var slsLog protocol.Log
	slsLog.Contents = make([]*protocol.Log_Content, 0, len(configTag)+len(logTags)+len(fields))
	for key, val := range configTag {
		cont := &protocol.Log_Content{
			Key:   key,
			Value: val,
		}
		slsLog.Contents = append(slsLog.Contents, cont)
	}

	for key, val := range logTags {
		cont := &protocol.Log_Content{
			Key:   key,
			Value: val,
		}
		slsLog.Contents = append(slsLog.Contents, cont)
	}

	for key, val := range fields {
		cont := &protocol.Log_Content{
			Key:   key,
			Value: val,
		}
		slsLog.Contents = append(slsLog.Contents, cont)
	}
	protocol.SetLogTimeWithNano(&slsLog, uint32(t.Unix()), uint32(t.Nanosecond()))
	return &slsLog, nil
}

func CreateLogByArray(t time.Time, configTag map[string]string, logTags map[string]string, columns []string, values []string) (*protocol.Log, error) {
	var slsLog protocol.Log
	slsLog.Contents = make([]*protocol.Log_Content, 0, len(configTag)+len(logTags)+len(columns))

	for key, val := range configTag {
		cont := &protocol.Log_Content{
			Key:   key,
			Value: val,
		}
		slsLog.Contents = append(slsLog.Contents, cont)
	}

	for key, val := range logTags {
		cont := &protocol.Log_Content{
			Key:   key,
			Value: val,
		}
		slsLog.Contents = append(slsLog.Contents, cont)
	}

	if len(columns) != len(values) {
		return nil, fmt.Errorf("columns and values not equal")
	}

	for index := range columns {
		cont := &protocol.Log_Content{
			Key:   columns[index],
			Value: values[index],
		}
		slsLog.Contents = append(slsLog.Contents, cont)
	}
	protocol.SetLogTimeWithNano(&slsLog, uint32(t.Unix()), uint32(t.Nanosecond()))
	return &slsLog, nil
}

// Label for metric label
type MetricLabel struct {
	Name  string
	Value string
}

// Labels for metric labels
type MetricLabels []MetricLabel

func (l MetricLabels) Len() int {
	return len(l)
}

func (l MetricLabels) Swap(i int, j int) {
	l[i], l[j] = l[j], l[i]
}

func (l MetricLabels) Less(i int, j int) bool {
	return l[i].Name < l[j].Name
}

func MinInt(a, b int) int {
	if a < b {
		return a
	}
	return b
}

// DefBucket ...
type DefBucket struct {
	Le    float64
	Count int64
}

// HistogramData ...
type HistogramData struct {
	Buckets []DefBucket
	Count   int64
	Sum     float64
}

// ToMetricLogs ..
func (hd *HistogramData) ToMetricLogs(name string, timeMs int64, labels MetricLabels) []*protocol.Log {
	logs := make([]*protocol.Log, 0, len(hd.Buckets)+2)
	sort.Sort(labels)
	for _, v := range hd.Buckets {
		newLabels := make(MetricLabels, len(labels), len(labels)+1)
		copy(newLabels, labels)
		newLabels = append(newLabels, MetricLabel{Name: "le", Value: strconv.FormatFloat(v.Le, 'g', -1, 64)})
		sort.Sort(newLabels)
		logs = append(logs, NewMetricLog(name+"_bucket", timeMs, strconv.FormatInt(v.Count, 10), newLabels))
	}
	logs = append(logs, NewMetricLog(name+"_count", timeMs, strconv.FormatInt(hd.Count, 10), labels))
	logs = append(logs, NewMetricLog(name+"_sum", timeMs, strconv.FormatFloat(hd.Sum, 'g', -1, 64), labels))
	return logs
}

// NewMetricLog caller must sort labels
func NewMetricLog(name string, timeMs int64, value string, labels []MetricLabel) *protocol.Log {
	strTime := strconv.FormatInt(timeMs, 10)
	metric := &protocol.Log{}
	protocol.SetLogTimeWithNano(metric, uint32(timeMs/1000), uint32((timeMs*1e6)%1e9))
	metric.Contents = []*protocol.Log_Content{}
	metric.Contents = append(metric.Contents, &protocol.Log_Content{Key: "__name__", Value: name})
	metric.Contents = append(metric.Contents, &protocol.Log_Content{Key: "__time_nano__", Value: strTime})

	builder := strings.Builder{}
	for index, l := range labels {
		if index != 0 {
			builder.WriteString("|")
		}
		builder.WriteString(l.Name)
		builder.WriteString("#$#")
		builder.WriteString(l.Value)

	}
	metric.Contents = append(metric.Contents, &protocol.Log_Content{Key: "__labels__", Value: builder.String()})

	metric.Contents = append(metric.Contents, &protocol.Log_Content{Key: "__value__", Value: value})
	return metric
}
