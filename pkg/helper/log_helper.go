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
	"math"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
)

const (
	// StaleNaN is a signaling NaN, due to the MSB of the mantissa being 0.
	// This value is chosen with many leading 0s, so we have scope to store more
	// complicated values in the future. It is 2 rather than 1 to make
	// it easier to distinguish from the NormalNaN by a human when debugging.
	StaleNaN                              uint64 = 0x7ff0000000000002
	StaleNan                                     = "__STALE_NAN__"
	SlsMetricstoreInvalidReplaceCharacter        = '_'
)

func CreateLog(t time.Time, enableTimestampNano bool, configTag map[string]string, logTags map[string]string, fields map[string]string) (*protocol.Log, error) {
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
	if enableTimestampNano {
		protocol.SetLogTimeWithNano(&slsLog, uint32(t.Unix()), uint32(t.Nanosecond()))
	} else {
		protocol.SetLogTime(&slsLog, uint32(t.Unix()))
	}
	return &slsLog, nil
}

func CreateLogByArray(t time.Time, enableTimestampNano bool, configTag map[string]string, logTags map[string]string, columns []string, values []string) (*protocol.Log, error) {
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
	if enableTimestampNano {
		protocol.SetLogTimeWithNano(&slsLog, uint32(t.Unix()), uint32(t.Nanosecond()))
	} else {
		protocol.SetLogTime(&slsLog, uint32(t.Unix()))
	}
	return &slsLog, nil
}

// Label for metric label
type MetricLabel struct {
	Name  string
	Value string
}

// Labels for metric labels
type MetricLabels struct {
	keyValues []*MetricLabel
	sorted    bool
	formatStr string
}

func (kv *MetricLabels) clearCache() {
	kv.sorted = false
	kv.formatStr = ""
}

func (kv *MetricLabels) Len() int {
	return len(kv.keyValues)
}

func (kv *MetricLabels) Swap(i int, j int) {
	kv.keyValues[i], kv.keyValues[j] = kv.keyValues[j], kv.keyValues[i]
}

func (kv *MetricLabels) Less(i int, j int) bool {
	return kv.keyValues[i].Name < kv.keyValues[j].Name
}

func (kv *MetricLabels) Replace(key, value string) {
	sort.Sort(kv)
	findIndex := sort.Search(len(kv.keyValues), func(index int) bool {
		return kv.keyValues[index].Name >= key
	})
	if findIndex < len(kv.keyValues) && kv.keyValues[findIndex].Name == key {
		kv.keyValues[findIndex].Value = value
	} else {
		kv.Append(key, value)
	}
	kv.clearCache()
}

func (kv *MetricLabels) Clone() *MetricLabels {
	if kv == nil {
		return &MetricLabels{}
	}
	var newKeyValues MetricLabels
	kv.CloneInto(&newKeyValues)
	return &newKeyValues
}

func (kv *MetricLabels) CloneInto(dst *MetricLabels) *MetricLabels {
	if kv == nil {
		return &MetricLabels{}
	}
	if dst == nil {
		return kv.Clone()
	}
	if len(kv.keyValues) < cap(dst.keyValues) {
		dst.keyValues = dst.keyValues[:len(kv.keyValues)]
	} else {
		dst.keyValues = make([]*MetricLabel, len(kv.keyValues))
	}
	dst.sorted = kv.sorted
	dst.formatStr = kv.formatStr
	for i, value := range kv.keyValues {
		cp := *value
		dst.keyValues[i] = &cp
	}
	return dst
}

// AppendMap ...
func (kv *MetricLabels) AppendMap(mapVal map[string]string) {
	for key, value := range mapVal {
		kv.Append(key, value)
	}
	kv.clearCache()
}

// Append ...
func (kv *MetricLabels) Append(key, value string) {
	kv.keyValues = append(kv.keyValues, &MetricLabel{
		formatLabelKey(key),
		formatLabelValue(value),
	})
	kv.clearCache()
}

func (kv *MetricLabels) SubSlice(begin, end int) {
	kv.keyValues = kv.keyValues[begin:end]
	kv.clearCache()
}

func (kv *MetricLabels) String() string {
	if kv == nil {
		return ""
	}
	if !kv.sorted || kv.formatStr == "" {
		sort.Sort(kv)
		var builder strings.Builder
		for index, label := range kv.keyValues {
			builder.WriteString(label.Name)
			builder.WriteString("#$#")
			builder.WriteString(label.Value)
			if index != len(kv.keyValues)-1 {
				builder.WriteByte('|')
			}
		}
		kv.formatStr = builder.String()
		kv.sorted = true
	}
	return kv.formatStr
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
func (hd *HistogramData) ToMetricLogs(name string, timeMs int64, labels *MetricLabels) []*protocol.Log {
	logs := make([]*protocol.Log, 0, len(hd.Buckets)+2)
	logs = append(logs, NewMetricLog(name+"_count", timeMs, float64(hd.Count), labels))
	logs = append(logs, NewMetricLog(name+"_sum", timeMs, hd.Sum, labels))
	for _, v := range hd.Buckets {
		labels.Replace("le", strconv.FormatFloat(v.Le, 'g', -1, 64))
		logs = append(logs, NewMetricLog(name+"_bucket", timeMs, float64(v.Count), labels))
	}

	return logs
}

// NewMetricLog create a metric log, time support unix milliseconds and unix nanoseconds.
// Note: must pass safe string
func NewMetricLog(name string, t int64, value float64, labels *MetricLabels) *protocol.Log {
	var valStr string
	if math.Float64bits(value) == StaleNaN {
		valStr = StaleNan
	} else {
		valStr = strconv.FormatFloat(value, 'g', -1, 64)
	}
	return NewMetricLogStringVal(name, t, valStr, labels)
}

// NewMetricLogStringVal create a metric log with val string, time support unix milliseconds and unix nanoseconds.
// Note: must pass safe string
func NewMetricLogStringVal(name string, t int64, value string, labels *MetricLabels) *protocol.Log {
	strTime := strconv.FormatInt(t, 10)
	metric := &protocol.Log{}
	switch len(strTime) {
	case 13:
		protocol.SetLogTimeWithNano(metric, uint32(t/1000), uint32((t*1e6)%1e9))
		strTime += "000000"
	case 19:
		protocol.SetLogTimeWithNano(metric, uint32(t/1e9), uint32(t%1e9))
	default:
		t = int64(float64(t) * math.Pow10(19-len(strTime)))
		strTime = strconv.FormatInt(t, 10)
		protocol.SetLogTimeWithNano(metric, uint32(t/1e9), uint32(t%1e9))
	}
	metric.Contents = make([]*protocol.Log_Content, 0, 4)
	metric.Contents = append(metric.Contents, &protocol.Log_Content{Key: "__name__", Value: formatNewMetricName(name)})
	metric.Contents = append(metric.Contents, &protocol.Log_Content{Key: "__time_nano__", Value: strTime})
	metric.Contents = append(metric.Contents, &protocol.Log_Content{Key: "__labels__", Value: labels.String()})
	metric.Contents = append(metric.Contents, &protocol.Log_Content{Key: "__value__", Value: value})
	return metric
}

func formatLabelKey(key string) string {
	if !config.LogtailGlobalConfig.EnableSlsMetricsFormat {
		return key
	}
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
			newKey[i] = SlsMetricstoreInvalidReplaceCharacter
		}
	}
	if newKey == nil {
		return key
	}
	return util.ZeroCopyBytesToString(newKey)
}

func formatLabelValue(value string) string {
	if !config.LogtailGlobalConfig.EnableSlsMetricsFormat {
		return value
	}
	var newValue []byte
	for i := 0; i < len(value); i++ {
		b := value[i]
		if b != '|' {
			continue
		} else {
			if newValue == nil {
				newValue = []byte(value)
			}
			newValue[i] = SlsMetricstoreInvalidReplaceCharacter
		}
	}
	if newValue == nil {
		return value
	}
	return util.ZeroCopyBytesToString(newValue)
}

func formatNewMetricName(name string) string {
	if !config.LogtailGlobalConfig.EnableSlsMetricsFormat {
		return name
	}
	var newName []byte
	for i := 0; i < len(name); i++ {
		b := name[i]
		if (b >= 'a' && b <= 'z') ||
			(b >= 'A' && b <= 'Z') ||
			(b >= '0' && b <= '9') ||
			b == '_' ||
			b == ':' {
			continue
		} else {
			if newName == nil {
				newName = []byte(name)
			}
			newName[i] = SlsMetricstoreInvalidReplaceCharacter
		}
	}
	if newName == nil {
		return name
	}
	return util.ZeroCopyBytesToString(newName)
}

// ReplaceInvalidChars analog of invalidChars = regexp.MustCompile("[^a-zA-Z0-9_]")
func ReplaceInvalidChars(in *string) {
	for charIndex, char := range *in {
		charInt := int(char)
		if !((charInt >= 97 && charInt <= 122) || // a-z
			(charInt >= 65 && charInt <= 90) || // A-Z
			(charInt >= 48 && charInt <= 57) || // 0-9
			charInt == 95 || charInt == ':') { // _

			*in = (*in)[:charIndex] + "_" + (*in)[charIndex+1:]
		}
	}
}
