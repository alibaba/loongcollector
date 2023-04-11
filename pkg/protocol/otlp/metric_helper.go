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

package otlp

import (
	"fmt"
	"sort"
	"strconv"
	"strings"

	"github.com/alibaba/ilogtail/pkg/models"
)

var (
	errInvalidBucket = fmt.Errorf("invalid_bucket_boundary")
)

// ComposeBucketFieldName generates the bucket count field name for histogram metrics.
func ComposeBucketFieldName(lower, upper float64, isPositive bool) string {
	if !isPositive {
		return fmt.Sprintf("[%v,%v)", -upper, -lower)
	}
	return fmt.Sprintf("(%v,%v]", lower, upper)
}

// ComputeBuckets computes the bucket boundarys and counts.
func ComputeBuckets(multiValues models.MetricFloatValues, isPositive bool) (bucketBounds []float64, bucketCounts []float64) {
	valueMap := multiValues.Iterator()
	bucketMap := make(map[float64]float64, len(bucketCounts))
	for k, v := range valueMap {
		bucketBound, err := ComputeBucketBoundary(k, isPositive)
		if err != nil {
			continue
		}
		bucketMap[bucketBound] = v
		bucketBounds = append(bucketBounds, bucketBound)
	}

	sort.Float64s(bucketBounds)
	if !isPositive {
		ReverseSlice(bucketBounds)
	}
	for _, v := range bucketBounds {
		bucketCounts = append(bucketCounts, bucketMap[v])
	}
	return
}

// ComputeBucketBoundary computes the bucket boundary from a field name.
func ComputeBucketBoundary(fieldName string, isPositive bool) (float64, error) {
	if !isPositive {
		if !strings.HasPrefix(fieldName, "[") || !strings.HasSuffix(fieldName, ")") {
			return 0, errInvalidBucket
		}

		fieldName = strings.TrimLeft(fieldName, "[")
		fieldName = strings.TrimRight(fieldName, ")")
		buckets := strings.Split(fieldName, ",")
		if len(buckets) != 2 {
			return 0, errInvalidBucket
		}
		return strconv.ParseFloat(buckets[1], 64)
	}

	if !strings.HasPrefix(fieldName, "(") || !strings.HasSuffix(fieldName, "]") {
		return 0, errInvalidBucket
	}

	fieldName = strings.TrimLeft(fieldName, "(")
	fieldName = strings.TrimRight(fieldName, "]")
	buckets := strings.Split(fieldName, ",")
	if len(buckets) != 2 {
		return 0, errInvalidBucket
	}
	return strconv.ParseFloat(buckets[0], 64)
}

func ReverseSlice[T comparable](s []T) {
	sort.SliceStable(s, func(i, j int) bool {
		return i > j
	})
}

// When count is less than 20, direct comparison faster than map.
func IsInternalTag(tagname string) bool {
	return tagname == TagKeyMetricAggregationTemporality ||
		tagname == TagKeyScopeDroppedAttributesCount ||
		tagname == TagKeyScopeName ||
		tagname == TagKeyScopeVersion ||
		tagname == TagKeySpanDroppedAttrsCount ||
		tagname == TagKeySpanDroppedEventsCount ||
		tagname == TagKeySpanDroppedLinksCount ||
		tagname == TagKeySpanStatusMessage ||
		tagname == TagKeyMetricIsMonotonic ||
		tagname == TagKeyLogFlag
}

func IsInternalField(fieldname string) bool {
	return fieldname == FieldCount ||
		fieldname == FieldMax ||
		fieldname == FieldMin ||
		fieldname == FieldSum ||
		fieldname == FieldScale ||
		fieldname == FieldPositiveOffset ||
		fieldname == FieldNegativeOffset ||
		fieldname == FieldZeroCount
}
