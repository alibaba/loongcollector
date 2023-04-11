// Copyright 2023 iLogtail Authors
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

// internal event tag keys of otlp logs/metrics/traces.
// don't forget to update IsInternalTag when adding new tag keys.
const (
	TagKeyScopeName                    = "otlp.scope.name"
	TagKeyScopeVersion                 = "otlp.scope.version"
	TagKeyScopeDroppedAttributesCount  = "otlp.scope.dropped.attributes.count"
	TagKeyMetricIsMonotonic            = "otlp.metric.ismonotonic"
	TagKeyMetricAggregationTemporality = "otlp.metric.aggregation.temporality"
	TagKeyMetricHistogramType          = "otlp.metric.histogram.type"
	TagKeySpanStatusMessage            = "otlp.span.status.message"
	TagKeySpanDroppedEventsCount       = "otlp.span.dropped.events.count"
	TagKeySpanDroppedLinksCount        = "otlp.span.dropped.links.count"
	TagKeySpanDroppedAttrsCount        = "otlp.span.dropped.attributes.count"
	TagKeyLogFlag                      = "otlp.log.flag"
)

// internal field names of otlp metrics.
// don't forget to update IsInternalField when adding new tag keys.
const (
	FieldCount          = "count"
	FieldSum            = "sum"
	FieldMin            = "min"
	FieldMax            = "max"
	FieldScale          = "scale"
	FieldPositiveOffset = "positive.offset"
	FieldNegativeOffset = "negative.offset"
	FieldZeroCount      = "zero.count"
)
