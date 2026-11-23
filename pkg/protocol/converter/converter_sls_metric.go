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
	"sort"
	"strings"
)

const (
	metricNameKey     = "__name__"
	metricLabelsKey   = "__labels__"
	metricTimeNanoKey = "__time_nano__"
	metricValueKey    = "__value__"
)

const (
	KeyValueSeparator = "#$#"
	LabelSeparator    = "|"
)

type MetricLabel struct {
	Key   string
	Value string
}

type MetricLabels []MetricLabel

func (m MetricLabels) Len() int {
	return len(m)
}

func (m MetricLabels) Less(i, j int) bool {
	return m[i].Key < m[j].Key
}

func (m MetricLabels) Swap(i, j int) {
	m[i], m[j] = m[j], m[i]
}

func (m MetricLabels) GetLabel() string {
	// sort label
	sort.Sort(m)
	var res []string
	for _, label := range m {
		res = append(res, label.Key+KeyValueSeparator+label.Value)
	}
	return strings.Join(res, LabelSeparator)
}
