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

package helper

//////////////////////////////////////////////////////////////////////////
// runner
//////////////////////////////////////////////////////////////////////////

// lebel keys
const (
	MetricLabelKeyRunnerName = "runner_name"
)

// label values
const (
	MetricLabelValueMetricCategoryRunner = "runner"
)

/**********************************************************
*   k8s meta
**********************************************************/

// label keys
const (
	MetricLabelKeyClusterID = "cluster_id"
)

// label values
const (
	MetricLabelValueRunnerNameK8sMeta = "k8s_meta"
)

// metric keys
const (
	MetricRunnerK8sMetaAddEventTotal    = "add_event_total"
	MetricRunnerK8sMetaUpdateEventTotal = "update_event_total"
	MetricRunnerK8sMetaDeleteEventTotal = "delete_event_total"
	MetricRunnerK8sMetaCacheSize        = "cache_size"
	MetricRunnerK8sMetaQueueSize        = "queue_size"
	MetricRunnerK8sMetaHTTPRequestTotal = "http_request_total"
	MetricRunnerK8sMetaHTTPAvgDelayMs   = "avg_delay_ms"
	MetricRunnerK8sMetaHTTPMaxDelayMs   = "max_delay_ms"
)
