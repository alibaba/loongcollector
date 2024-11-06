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
const string METRIC_LABEL_KEY_COMPONENT_NAME = "component_name";
const string METRIC_LABEL_KEY_FLUSHER_PLUGIN_ID = "flusher_plugin_id";

/**********************************************************
 *   queue
 **********************************************************/
const string METRIC_LABEL_KEY_QUEUE_TYPE = "queue_type";
const string METRIC_LABEL_KEY_EXACTLY_ONCE_ENABLED = "exactly_once_enabled";

/**********************************************************
 *   batcher
 **********************************************************/
const string METRIC_LABEL_KEY_GROUP_BATCH_ENABLED = "group_batch_enabled";

// label values
const string METRIC_LABEL_VALUE_COMPONENT_NAME_BATCHER = "batcher";
const string METRIC_LABEL_VALUE_COMPONENT_NAME_COMPRESSOR = "compressor";
const string METRIC_LABEL_VALUE_COMPONENT_NAME_PROCESS_QUEUE = "process_queue";
const string METRIC_LABEL_VALUE_COMPONENT_NAME_ROUTER = "router";
const string METRIC_LABEL_VALUE_COMPONENT_NAME_SENDER_QUEUE = "sender_queue";
const string METRIC_LABEL_VALUE_COMPONENT_NAME_SERIALIZER = "serializer";

// metric keys
const string METRIC_COMPONENT_IN_EVENTS_TOTAL = "in_events_total";
const string METRIC_COMPONENT_IN_SIZE_BYTES = "in_size_bytes";
const string METRIC_COMPONENT_IN_ITEMS_TOTAL = "in_items_total";
const string METRIC_COMPONENT_OUT_EVENTS_TOTAL = "out_events_total";
const string METRIC_COMPONENT_OUT_ITEMS_TOTAL = "out_items_total";
const string METRIC_COMPONENT_OUT_SIZE_BYTES = "out_size_bytes";
const string METRIC_COMPONENT_TOTAL_DELAY_MS = "total_delay_ms";
const string METRIC_COMPONENT_TOTAL_PROCESS_TIME_MS = "total_process_time_ms";
const string METRIC_COMPONENT_DISCARDED_ITEMS_TOTAL = "discarded_items_total";
const string METRIC_COMPONENT_DISCARDED_ITEMS_SIZE_BYTES = "discarded_item_size_bytes";

/**********************************************************
 *   batcher
 **********************************************************/
const string METRIC_COMPONENT_BATCHER_EVENT_BATCHES_TOTAL = "event_batches_total";
const string METRIC_COMPONENT_BATCHER_BUFFERED_GROUPS_TOTAL = "buffered_groups_total";
const string METRIC_COMPONENT_BATCHER_BUFFERED_EVENTS_TOTAL = "buffered_events_total";
const string METRIC_COMPONENT_BATCHER_BUFFERED_SIZE_BYTES = "buffered_size_bytes";
const string METRIC_COMPONENT_BATCHER_TOTAL_ADD_TIME_MS = "total_add_time_ms";

/**********************************************************
 *   queue
 **********************************************************/
const string METRIC_COMPONENT_QUEUE_SIZE = "queue_size";
const string METRIC_COMPONENT_QUEUE_SIZE_BYTES = "queue_size_bytes";
const string METRIC_COMPONENT_QUEUE_VALID_TO_PUSH_FLAG = "valid_to_push_status";
const string METRIC_COMPONENT_QUEUE_EXTRA_BUFFER_SIZE = "extra_buffer_size";
const string METRIC_COMPONENT_QUEUE_EXTRA_BUFFER_SIZE_BYTES = "extra_buffer_size_bytes";
const string METRIC_COMPONENT_QUEUE_DISCARDED_EVENTS_TOTAL = "discarded_events_total";

const string METRIC_COMPONENT_QUEUE_FETCHED_ITEMS_TOTAL = "fetched_items_total";
const string METRIC_COMPONENT_QUEUE_FETCH_ATTEMPTS_TOTAL = "fetch_attempts_total";
const string METRIC_COMPONENT_QUEUE_SUCCESSFUL_FETCH_TIMES_TOTAL = "successful_fetch_times_total";
const string METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_REGION_LIMITER_TIMES_TOTAL
    = "fetch_rejected_by_region_limiter_times_total";
const string METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_PROJECT_LIMITER_TIMES_TOTAL
    = "fetch_rejected_by_project_limiter_times_total";
const string METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_LOGSTORE_LIMITER_TIMES_TOTAL
    = "fetch_rejected_by_logstore_limiter_times_total";
const string METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_RATE_LIMITER_TIMES_TOTAL
    = "fetch_rejected_by_rate_limiter_times_total";

} // namespace logtail
