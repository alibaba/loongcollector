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

package baseagg

import (
	"strconv"
	"strings"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

// FindEventField resolves a configured key. The canonical metric keys allow
// rules written for metric rows to keep matching after inputs emit typed Metrics.
func FindEventField(event models.PipelineEvent, key string) (string, bool) {
	switch typed := event.(type) {
	case *models.Log:
		value, ok := typed.GetIndices().Iterator()[key]
		if ok {
			return pipeline.GetStringValue(value), true
		}
		if tag, tagKey := strings.CutPrefix(key, "__tag__:"); tagKey && typed.GetTags() != nil {
			value, ok := typed.GetTags().Iterator()[tag]
			return value, ok
		}
	case *models.Metric:
		switch key {
		case "__name__":
			return typed.GetName(), true
		case "__time_nano__":
			return strconv.FormatUint(typed.GetTimestamp(), 10), true
		case "__value__":
			if value := typed.GetValue(); value != nil && value.IsSingleValue() {
				return helper.FormatMetricLogValue(value.GetSingleValue()), true
			}
		case "__labels__":
			labels := &helper.MetricLabels{}
			if tags := typed.GetTags(); tags != nil {
				for key, value := range tags.Iterator() {
					labels.Append(key, value)
				}
			}
			return labels.String(), true
		}
	}
	return "", false
}
