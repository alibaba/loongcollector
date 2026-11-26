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

package selfmonitor

const MetricLabelValueMetricCategoryPipeline = "pipeline"

// These names match the native pipeline metrics in PipelineMetrics.cpp.
const (
	MetricPipelineProcessorInEventsTotal      = "processor_in_events_total"
	MetricPipelineProcessorInEventGroupsTotal = "processor_in_event_groups_total"
	MetricPipelineProcessorInSizeBytes        = "processor_in_size_bytes"
	MetricPipelineFlusherInEventsTotal        = "flusher_in_events_total"
	MetricPipelineFlusherInEventGroupsTotal   = "flusher_in_event_groups_total"
	MetricPipelineFlusherInSizeBytes          = "flusher_in_size_bytes"
)
