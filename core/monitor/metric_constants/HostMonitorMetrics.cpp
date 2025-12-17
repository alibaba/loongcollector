// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "MetricConstants.h"

using namespace std;

namespace logtail {

// Timer metrics
const string METRIC_RUNNER_TIMER_OUT_ITEMS_TOTAL = "out_items_total";
const string METRIC_RUNNER_TIMER_IN_ITEMS_TOTAL = "in_items_total";
const string METRIC_RUNNER_TIMER_LATENCY_TIME_MS = "latency_time_ms";
const string METRIC_RUNNER_TIMER_QUEUE_ITEMS_TOTAL = "queue_items_total";

// Host monitor runner metrics
const string METRIC_RUNNER_HOST_MONITOR_OUT_ITEMS_TOTAL = "out_items_total";
const string METRIC_RUNNER_HOST_MONITOR_OUT_ITEMS_SIZE = "out_items_size";
const string METRIC_RUNNER_HOST_MONITOR_DROP_ITEMS_TOTAL = "drop_items_total";
const string METRIC_RUNNER_HOST_MONITOR_LATENCY_TIME_MS = "latency_time_ms";

// System interface metrics
const string METRIC_RUNNER_SYSTEM_OP_TOTAL = "system_op_total";
const string METRIC_RUNNER_SYSTEM_OP_FAIL_TOTAL = "system_op_fail_total";
const string METRIC_RUNNER_SYSTEM_CACHE_HIT_TOTAL = "cache_hit_total";
const string METRIC_RUNNER_SYSTEM_CACHE_ITEMS_SIZE = "cache_items_size";

// Collector fail metrics
const string METRIC_PLUGIN_CPU_FAIL_TOTAL = "cpu_fail_total";
const string METRIC_PLUGIN_SYSTEM_FAIL_TOTAL = "system_fail_total";
const string METRIC_PLUGIN_MEM_FAIL_TOTAL = "mem_fail_total";
const string METRIC_PLUGIN_NET_FAIL_TOTAL = "net_fail_total";
const string METRIC_PLUGIN_PROCESS_FAIL_TOTAL = "process_fail_total";
const string METRIC_PLUGIN_DISK_FAIL_TOTAL = "disk_fail_total";

// Process entity runner metrics
const string METRIC_RUNNER_FULL_REPORT_TOTAL = "full_report_total";
const string METRIC_RUNNER_INCREMENTAL_REPORT_TOTAL = "incremental_report_total";
const string METRIC_RUNNER_PROCESS_ADDED_TOTAL = "process_added_total";
const string METRIC_RUNNER_PROCESS_REMOVED_TOTAL = "process_removed_total";
const string METRIC_RUNNER_PROCESS_REUSED_TOTAL = "process_reused_total";
const string METRIC_RUNNER_CACHED_PROCESS_COUNT = "cached_process_count";
const string METRIC_RUNNER_COLLECT_LATENCY_MS = "collect_latency_ms";

} // namespace logtail
