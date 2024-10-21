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

// label keys
const string METRIC_LABEL_KEY_RUNNER_NAME = "runner_name";

// label values
const string METRIC_LABEL_VALUE_RUNNER_NAME_FILE_SERVER = "file_server";
const string METRIC_LABEL_VALUE_RUNNER_NAME_FLUSHER = "flusher_runner";
const string METRIC_LABEL_VALUE_RUNNER_NAME_HTTP_SINK = "http_sink";
const string METRIC_LABEL_VALUE_RUNNER_NAME_PROCESSOR = "processor_runner";
const string METRIC_LABEL_VALUE_RUNNER_NAME_PROMETHEUS = "prometheus_runner";

// metric keys
const string METRIC_RUNNER_IN_EVENTS_TOTAL = "runner_in_events_total";
const string METRIC_RUNNER_IN_EVENT_GROUPS_TOTAL = "runner_in_event_groups_total";
const string METRIC_RUNNER_IN_SIZE_BYTES = "runner_in_size_bytes";
const string METRIC_RUNNER_IN_ITEMS_TOTAL = "runner_in_items_total";
const string METRIC_RUNNER_LAST_RUN_TIME = "runner_last_run_time";
const string METRIC_RUNNER_OUT_ITEMS_TOTAL = "runner_out_items_total";
const string METRIC_RUNNER_TOTAL_DELAY_MS = "runner_total_delay_ms";
const string METRIC_RUNNER_CLIENT_REGISTER_STATE = "runner_client_register_state";
const string METRIC_RUNNER_CLIENT_REGISTER_RETRY_TOTAL = "runner_client_register_retry_total";
const string METRIC_RUNNER_JOBS_TOTAL = "runner_jobs_total";

/**********************************************************
 *   all sinks
 **********************************************************/
const string METRIC_RUNNER_SINK_OUT_SUCCESSFUL_ITEMS_TOTAL = "runner_out_successful_items_total";
const string METRIC_RUNNER_SINK_OUT_FAILED_ITEMS_TOTAL = "runner_out_failed_items_total";
const string METRIC_RUNNER_SINK_SUCCESSFUL_ITEM_TOTAL_RESPONSE_TIME_MS = "runner_successful_item_total_response_time_ms";
const string METRIC_RUNNER_SINK_FAILED_ITEM_TOTAL_RESPONSE_TIME_MS = "runner_failed_item_total_response_time_ms";
const string METRIC_RUNNER_SINK_SENDING_ITEMS_TOTAL = "runner_sending_items_total";
const string METRIC_RUNNER_SINK_SEND_CONCURRENCY = "runner_send_concurrency";

/**********************************************************
 *   flusher runner
 **********************************************************/
const string METRIC_RUNNER_FLUSHER_IN_RAW_SIZE_BYTES = "runner_in_raw_size_bytes";
const string METRIC_RUNNER_FLUSHER_WAITING_ITEMS_TOTAL = "runner_waiting_items_total";

/**********************************************************
 *   file server
 **********************************************************/
const string METRIC_RUNNER_FILE_WATCHED_DIRS_TOTAL = "runner_watched_dirs_total";
const string METRIC_RUNNER_FILE_ACTIVE_READERS_TOTAL = "runner_active_readers_total";
const string METRIC_RUNNER_FILE_ENABLE_FILE_INCLUDED_BY_MULTI_CONFIGS_FLAG
    = "runner_enable_file_included_by_multi_configs";
const string METRIC_RUNNER_FILE_POLLING_MODIFY_CACHE_SIZE = "runner_polling_modify_cache_size";
const string METRIC_RUNNER_FILE_POLLING_DIR_CACHE_SIZE = "runner_polling_dir_cache_size";
const string METRIC_RUNNER_FILE_POLLING_FILE_CACHE_SIZE = "runner_polling_file_cache_size";

} // namespace logtail