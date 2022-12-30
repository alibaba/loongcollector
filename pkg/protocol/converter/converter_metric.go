// Copyright 2022 iLogtail Authors
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

package protocol

import (
	"fmt"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

const (
	metricNameKey      = "__name__"
	metricLabelsKey    = "__labels__"
	metricTimeNanoKey  = "__time_nano__"
	metricValueKey     = "__value__"
	metricValueTypeKey = "__type__"
	metricFieldKey     = "__field__"
)

const (
	valueTypeFloat  = "float"
	valueTypeInt    = "int"
	valueTypeBool   = "bool"
	valueTypeString = "string"
)

var readerPool = sync.Pool{
	New: func() any {
		return &metricReader{}
	},
}

type metricReader struct {
	name      string
	labels    string
	value     string
	valueType string
	timestamp string
	fieldName string
}

type metricLabel struct {
	key   string
	value string
}

type metricLabels []metricLabel

func (m metricLabels) Len() int {
	return len(m)
}

func (m metricLabels) Less(i, j int) bool {
	return m[i].key < m[j].key
}

func (m metricLabels) Swap(i, j int) {
	m[i], m[j] = m[j], m[i]
}

func (r *metricReader) readNames() (metricName, fieldName string) {
	if len(r.fieldName) == 0 || r.fieldName == "value" {
		return r.name, "value"
	}
	name := strings.TrimSuffix(r.name, ":"+r.fieldName)
	return name, r.fieldName
}

func (r *metricReader) readSortedLabels() ([]metricLabel, error) {
	n := r.countLabels()
	if n == 0 {
		return nil, nil
	}

	labels := make([]metricLabel, 0, n)
	remainLabels := r.labels
	lastIndex := -1
	label := ""
	key := ""

	for len(remainLabels) > 0 {
		endIdx := strings.Index(remainLabels, "|")
		if endIdx < 0 {
			label = remainLabels
			remainLabels = ""
		} else {
			label = remainLabels[:endIdx]
			remainLabels = remainLabels[endIdx+1:]
		}
		splitIdx := strings.Index(label, "#$#")
		if splitIdx < 0 {
			if lastIndex >= 0 {
				labels[lastIndex].value += "|"
				labels[lastIndex].value += label
				continue
			}
			if len(key) == 0 {
				key = label
				continue
			}
			key += "|"
			key += label
			continue
		}

		if len(key) > 0 {
			key += "|"
			key += label[:splitIdx]
		} else {
			key = label[:splitIdx]
		}

		labels = append(labels, metricLabel{key: key, value: label[splitIdx+3:]})

		lastIndex++
		key = ""

		if endIdx < 0 {
			break
		}
	}

	sort.Sort(metricLabels(labels))

	if len(key) > 0 {
		return labels, fmt.Errorf("found miss matching key: %s", key)
	}

	return labels, nil
}

func (r *metricReader) countLabels() int {
	if len(r.labels) == 0 {
		return 0
	}
	n := strings.Count(r.labels, "|")
	return n + 1
}

func (r *metricReader) readValue() (interface{}, error) {
	switch r.valueType {
	case valueTypeBool:
		return strconv.ParseBool(r.value)
	case valueTypeString:
		return r.value, nil
	case valueTypeInt:
		return strconv.ParseInt(r.value, 10, 64)
	default:
		return strconv.ParseFloat(r.value, 64)
	}
}

func (r *metricReader) readTimestamp() (time.Time, error) {

	if len(r.timestamp) == 0 {
		return time.Time{}, nil
	}
	t, err := strconv.ParseInt(r.timestamp, 10, 64)
	if err != nil {
		return time.Time{}, err
	}
	return time.Unix(t/1e9, t%1e9).UTC(), nil
}

func (r *metricReader) recycle() {
	r.reset()
	readerPool.Put(r)
}

func (r *metricReader) reset() {
	r.labels = ""
	r.name = ""
	r.value = ""
	r.valueType = ""
	r.timestamp = ""
	r.fieldName = ""
}

func (r *metricReader) set(log *protocol.Log) error {
	r.reset()
	for _, v := range log.Contents {
		switch v.Key {
		case metricNameKey:
			r.name = v.Value
		case metricLabelsKey:
			r.labels = v.Value
		case metricTimeNanoKey:
			r.timestamp = v.Value
		case metricValueKey:
			r.value = v.Value
		case metricValueTypeKey:
			r.valueType = v.Value
		case metricFieldKey:
			r.fieldName = v.Value
		}
	}
	if len(r.name) == 0 || len(r.value) == 0 {
		return fmt.Errorf("metrics data must contains keys: %s, %s", metricNameKey, metricValueKey)
	}
	return nil
}

func newMetricReader() *metricReader {
	return readerPool.Get().(*metricReader)
}
