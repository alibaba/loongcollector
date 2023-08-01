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
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/pipeline"
)

var metricKeys []string

// KeyValue ...
type KeyValue struct {
	Key   string
	Value string
}

// KeyValues ...
type KeyValues struct {
	keyValues []KeyValue
}

func (kv *KeyValues) Len() int { return len(kv.keyValues) }
func (kv *KeyValues) Swap(i, j int) {
	kv.keyValues[i], kv.keyValues[j] = kv.keyValues[j], kv.keyValues[i]
}
func (kv *KeyValues) Less(i, j int) bool { return kv.keyValues[i].Key < kv.keyValues[j].Key }

// Sort ...
func (kv *KeyValues) Sort() { sort.Sort(kv) }

// Replace ...
func (kv *KeyValues) Replace(key, value string) {
	findIndex := sort.Search(len(kv.keyValues), func(index int) bool {
		return kv.keyValues[index].Key >= key
	})
	if findIndex < len(kv.keyValues) && kv.keyValues[findIndex].Key == key {
		kv.keyValues[findIndex].Value = value
	}
}

// AppendMap ...
func (kv *KeyValues) AppendMap(mapVal map[string]string) {
	for key, value := range mapVal {
		kv.keyValues = append(kv.keyValues, KeyValue{
			Key:   key,
			Value: value,
		})
	}
}

// Append ...
func (kv *KeyValues) Append(key, value string) {
	kv.keyValues = append(kv.keyValues, KeyValue{
		key,
		value,
	})
}

// Clone ...
func (kv *KeyValues) Clone() KeyValues {
	var newKeyValues KeyValues
	newKeyValues.keyValues = make([]KeyValue, len(kv.keyValues))
	copy(newKeyValues.keyValues, kv.keyValues)
	return newKeyValues
}

func (kv *KeyValues) String() string {
	var builder strings.Builder
	kv.labelToStringBuilder(&builder)
	return builder.String()
}

func (kv *KeyValues) labelToStringBuilder(sb *strings.Builder) {
	if sb.Len() != 0 {
		sb.WriteByte('|')
	}
	for index, label := range kv.keyValues {
		sb.WriteString(label.Key)
		sb.WriteString("#$#")
		sb.WriteString(label.Value)
		if index != len(kv.keyValues)-1 {
			sb.WriteByte('|')
		}
	}
}

// MakeMetric ...
func MakeMetric(name string, labels string, timeNano int64, value float64) ([]string, []string) {
	values := make([]string, 4)
	values[0] = name
	values[1] = labels
	values[2] = strconv.FormatInt(timeNano, 10)
	values[3] = strconv.FormatFloat(value, 'g', -1, 64)
	return metricKeys, values
}

// AddMetric to the collector.
func AddMetric(collector pipeline.Collector,
	name string,
	time time.Time,
	labels string,
	value float64) {
	keys, vals := MakeMetric(name, labels, time.UnixNano(), value)
	collector.AddDataArray(nil, keys, vals, time)
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

func init() {
	metricKeys = append(metricKeys, "__name__")
	metricKeys = append(metricKeys, "__labels__")
	metricKeys = append(metricKeys, "__time_nano__")
	metricKeys = append(metricKeys, "__value__")
}
