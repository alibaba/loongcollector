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
const string METRIC_COMPONENT_IN_EVENTS_TOTAL = "component_in_events_total";
const string METRIC_COMPONENT_IN_SIZE_BYTES = "component_in_size_bytes";
const string METRIC_COMPONENT_IN_ITEMS_TOTAL = "component_in_items_total";
const string METRIC_COMPONENT_OUT_EVENTS_TOTAL = "component_out_events_total";
const string METRIC_COMPONENT_OUT_ITEMS_TOTAL = "component_out_items_total";
const string METRIC_COMPONENT_OUT_SIZE_BYTES = "component_out_size_bytes";
const string METRIC_COMPONENT_TOTAL_DELAY_MS = "component_total_delay_ms";
const string METRIC_COMPONENT_TOTAL_PROCESS_TIME_MS = "component_total_process_time_ms";
const string METRIC_COMPONENT_DISCARDED_ITEMS_TOTAL = "component_discarded_items_total";
const string METRIC_COMPONENT_DISCARDED_ITEMS_SIZE_BYTES = "component_discarded_item_size_bytes";

/**********************************************************
 *   batcher
 **********************************************************/
const string METRIC_COMPONENT_BATCHER_EVENT_BATCHES_TOTAL = "component_event_batches_total";
const string METRIC_COMPONENT_BATCHER_BUFFERED_GROUPS_TOTAL = "component_buffered_groups_total";
const string METRIC_COMPONENT_BATCHER_BUFFERED_EVENTS_TOTAL = "component_buffered_events_total";
const string METRIC_COMPONENT_BATCHER_BUFFERED_SIZE_BYTES = "component_buffered_size_bytes";

/**********************************************************
 *   queue
 **********************************************************/
const string METRIC_COMPONENT_QUEUE_SIZE = "component_queue_size";
const string METRIC_COMPONENT_QUEUE_SIZE_BYTES = "component_queue_size_bytes";
const string METRIC_COMPONENT_QUEUE_VALID_TO_PUSH_FLAG = "component_valid_to_push_status";
const string METRIC_COMPONENT_QUEUE_EXTRA_BUFFER_SIZE = "component_extra_buffer_size";
const string METRIC_COMPONENT_QUEUE_EXTRA_BUFFER_SIZE_BYTES = "component_extra_buffer_size_bytes";
const string METRIC_COMPONENT_QUEUE_DISCARDED_EVENTS_TOTAL = "component_discarded_events_total";

const string METRIC_COMPONENT_QUEUE_FETCH_TIMES_TOTAL = "component_fetch_times_total";
const string METRIC_COMPONENT_QUEUE_FETCHED_ITEMS_TOTAL = "componet_fetched_items_total";
const string METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_REGION_LIMITER_TIMES_TOTAL = "component_fetch_rejected_by_region_limiter_times_total";
const string METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_PROJECT_LIMITER_TIMES_TOTAL = "component_fetch_rejected_by_project_limiter_times_total";
const string METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_LOGSTORE_LIMITER_TIMES_TOTAL = "component_fetch_rejected_by_logstore_limiter_times_total";
const string METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_RATE_LIMITER_TIMES_TOTAL = "component_fetch_rejected_by_rate_limiter_times_total";

} // namespace logtail
