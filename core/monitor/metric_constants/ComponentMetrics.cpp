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
const string& METRIC_COMPONENT_IN_EVENTS_TOTAL = METRIC_IN_EVENTS_TOTAL;
const string& METRIC_COMPONENT_IN_SIZE_BYTES = METRIC_IN_SIZE_BYTES;
const string& METRIC_COMPONENT_IN_ITEMS_TOTAL = METRIC_IN_ITEMS_TOTAL;
const string& METRIC_COMPONENT_OUT_EVENTS_TOTAL = METRIC_OUT_EVENTS_TOTAL;
const string& METRIC_COMPONENT_OUT_ITEMS_TOTAL = METRIC_OUT_ITEMS_TOTAL;
const string& METRIC_COMPONENT_OUT_SIZE_BYTES = METRIC_OUT_SIZE_BYTES;
const string& METRIC_COMPONENT_TOTAL_DELAY_MS = METRIC_TOTAL_DELAY_MS;
const string& METRIC_COMPONENT_TOTAL_PROCESS_TIME_MS = METRIC_TOTAL_PROCESS_TIME_MS;
const string& METRIC_COMPONENT_DISCARDED_ITEMS_TOTAL = METRIC_DISCARDED_ITEMS_TOTAL;
const string& METRIC_COMPONENT_DISCARDED_SIZE_BYTES = METRIC_DISCARDED_SIZE_BYTES;

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
const string& METRIC_COMPONENT_QUEUE_DISCARDED_EVENTS_TOTAL = METRIC_DISCARDED_EVENTS_TOTAL;

const string METRIC_COMPONENT_QUEUE_FETCHED_ITEMS_TOTAL = "fetched_items_total";
const string METRIC_COMPONENT_QUEUE_FETCH_TIMES_TOTAL = "fetch_times_total";
const string METRIC_COMPONENT_QUEUE_VALID_FETCH_TIMES_TOTAL = "valid_fetch_times_total";
const string METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_REGION_LIMITER_TIMES_TOTAL = "region_reject_times_total";
const string METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_PROJECT_LIMITER_TIMES_TOTAL = "project_reject_times_total";
const string METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_LOGSTORE_LIMITER_TIMES_TOTAL = "logstore_reject_times_total";
const string METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_RATE_LIMITER_TIMES_TOTAL = "rate_reject_times_total";

} // namespace logtail
