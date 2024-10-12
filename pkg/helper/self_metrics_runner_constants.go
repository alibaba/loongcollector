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

/**********************************************************
*   k8s meta
**********************************************************/
const (
	MetricRunnerK8sMetaAddEventTotal    = "runner_k8s_meta_add_event_total"
	MetricRunnerK8sMetaUpdateEventTotal = "runner_k8s_meta_update_event_total"
	MetricRunnerK8sMetaDeleteEventTotal = "runner_k8s_meta_delete_event_total"
	MetricRunnerK8sMetaCacheSize        = "runner_k8s_meta_cache_size"
	MetricRunnerK8sMetaQueueSize        = "runner_k8s_meta_queue_size"
	MetricRunnerK8sMetaHTTPRequestTotal = "runner_k8s_meta_http_request_total"
	MetricRunnerK8sMetaHTTPAvgDelayMs   = "runner_k8s_meta_avg_delay_ms"
	MetricRunnerK8sMetaHTTPMaxDelayMs   = "runner_k8s_meta_max_delay_ms"
)
