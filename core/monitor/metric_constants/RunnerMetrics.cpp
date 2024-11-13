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

#include "MetricCommonConstants.h"
#include "MetricConstants.h"

using namespace std;

namespace logtail {

// label keys
const string METRIC_LABEL_KEY_RUNNER_NAME = "runner_name";
const string METRIC_LABEL_KEY_THREAD_NO = "thread_no";

// label values
const string METRIC_LABEL_VALUE_RUNNER_NAME_FILE_SERVER = "file_server";
const string METRIC_LABEL_VALUE_RUNNER_NAME_FLUSHER = "flusher_runner";
const string METRIC_LABEL_VALUE_RUNNER_NAME_HTTP_SINK = "http_sink";
const string METRIC_LABEL_VALUE_RUNNER_NAME_PROCESSOR = "processor_runner";
const string METRIC_LABEL_VALUE_RUNNER_NAME_PROMETHEUS = "prometheus_runner";
const string METRIC_LABEL_VALUE_RUNNER_NAME_EBPF_SERVER = "ebpf_server";

// metric keys
const string& METRIC_RUNNER_IN_EVENTS_TOTAL = METRIC_IN_EVENTS_TOTAL;
const string& METRIC_RUNNER_IN_EVENT_GROUPS_TOTAL = METRIC_IN_EVENT_GROUPS_TOTAL;
const string& METRIC_RUNNER_IN_SIZE_BYTES = METRIC_IN_SIZE_BYTES;
const string& METRIC_RUNNER_IN_ITEMS_TOTAL = METRIC_IN_ITEMS_TOTAL;
const string METRIC_RUNNER_LAST_RUN_TIME = "last_run_time";
const string& METRIC_RUNNER_OUT_ITEMS_TOTAL = METRIC_OUT_ITEMS_TOTAL;
const string& METRIC_RUNNER_TOTAL_DELAY_MS = METRIC_TOTAL_DELAY_MS;
const string METRIC_RUNNER_CLIENT_REGISTER_STATE = "client_register_state";
const string METRIC_RUNNER_CLIENT_REGISTER_RETRY_TOTAL = "client_register_retry_total";
const string METRIC_RUNNER_JOBS_TOTAL = "jobs_total";

/**********************************************************
 *   all sinks
 **********************************************************/
const string METRIC_RUNNER_SINK_OUT_SUCCESSFUL_ITEMS_TOTAL = "out_successful_items_total";
const string METRIC_RUNNER_SINK_OUT_FAILED_ITEMS_TOTAL = "out_failed_items_total";
const string METRIC_RUNNER_SINK_SUCCESSFUL_ITEM_TOTAL_RESPONSE_TIME_MS = "successful_response_time_ms";
const string METRIC_RUNNER_SINK_FAILED_ITEM_TOTAL_RESPONSE_TIME_MS = "failed_response_time_ms";
const string METRIC_RUNNER_SINK_SENDING_ITEMS_TOTAL = "sending_items_total";
const string METRIC_RUNNER_SINK_SEND_CONCURRENCY = "send_concurrency";

/**********************************************************
 *   flusher runner
 **********************************************************/
const string METRIC_RUNNER_FLUSHER_IN_RAW_SIZE_BYTES = "in_raw_size_bytes";
const string METRIC_RUNNER_FLUSHER_WAITING_ITEMS_TOTAL = "waiting_items_total";

/**********************************************************
 *   file server
 **********************************************************/
const string METRIC_RUNNER_FILE_WATCHED_DIRS_TOTAL = "watched_dirs_total";
const string METRIC_RUNNER_FILE_ACTIVE_READERS_TOTAL = "active_readers_total";
const string METRIC_RUNNER_FILE_ENABLE_FILE_INCLUDED_BY_MULTI_CONFIGS_FLAG = "enable_multi_configs";
const string METRIC_RUNNER_FILE_POLLING_MODIFY_CACHE_SIZE = "polling_modify_cache_size";
const string METRIC_RUNNER_FILE_POLLING_DIR_CACHE_SIZE = "polling_dir_cache_size";
const string METRIC_RUNNER_FILE_POLLING_FILE_CACHE_SIZE = "polling_file_cache_size";

/**********************************************************
 *   ebpf server
 **********************************************************/
const string METRIC_RUNNER_EBPF_START_PLUGIN_TOTAL = "start_plugin_total";
const string METRIC_RUNNER_EBPF_STOP_PLUGIN_TOTAL = "stop_plugin_total";
const string METRIC_RUNNER_EBPF_SUSPEND_PLUGIN_TOTAL = "suspend_plugin_total";

} // namespace logtail