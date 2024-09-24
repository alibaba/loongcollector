/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <string>

namespace logtail {

extern const std::string METRIC_FIELD_REGION;
extern const std::string METRIC_REGION_DEFAULT;
extern const std::string METRIC_SLS_LOGSTORE_NAME;
extern const std::string METRIC_TOPIC_TYPE;
extern const std::string METRIC_TOPIC_FIELD_NAME;

extern const std::string LABEL_PREFIX;
extern const std::string VALUE_PREFIX;

// global metrics labels

extern const std::string METRIC_LABEL_ALIUIDS;
extern const std::string METRIC_LABEL_INSTANCE_ID;
extern const std::string METRIC_LABEL_IP;
extern const std::string METRIC_LABEL_OS;
extern const std::string METRIC_LABEL_OS_DETAIL;
extern const std::string METRIC_LABEL_USER_DEFINED_ID;
extern const std::string METRIC_LABEL_UUID;
extern const std::string METRIC_LABEL_VERSION;

// global metrics values

extern const std::string METRIC_AGENT_CPU;
extern const std::string METRIC_AGENT_CPU_GO;
extern const std::string METRIC_AGENT_MEMORY;
extern const std::string METRIC_AGENT_MEMORY_GO;
extern const std::string METRIC_AGENT_GO_ROUTINES_TOTAL;
extern const std::string METRIC_AGENT_OPEN_FD_TOTAL;
extern const std::string METRIC_AGENT_INSTANCE_CONFIG_TOTAL;
extern const std::string METRIC_AGENT_PIPELINE_CONFIG_TOTAL;
extern const std::string METRIC_AGENT_ENV_PIPELINE_CONFIG_TOTAL;
extern const std::string METRIC_AGENT_CRD_PIPELINE_CONFIG_TOTAL;
extern const std::string METRIC_AGENT_CONSOLE_PIPELINE_CONFIG_TOTAL;
extern const std::string METRIC_AGENT_PLUGIN_TOTAL;

// common plugin labels
extern const std::string METRIC_LABEL_PROJECT;
extern const std::string METRIC_LABEL_LOGSTORE;
extern const std::string METRIC_LABEL_REGION;
extern const std::string METRIC_LABEL_CONFIG_NAME;
extern const std::string METRIC_LABEL_PLUGIN_NAME;
extern const std::string METRIC_LABEL_PLUGIN_ID;
extern const std::string METRIC_LABEL_NODE_ID;
extern const std::string METRIC_LABEL_CHILD_NODE_ID;

// input file plugin labels
extern const std::string METRIC_LABEL_FILE_DEV;
extern const std::string METRIC_LABEL_FILE_INODE;
extern const std::string METRIC_LABEL_FILE_NAME;

extern const std::string METRIC_LABEL_KEY_COMPONENT_NAME;
extern const std::string METRIC_LABEL_KEY_RUNNER_NAME;
extern const std::string METRIC_LABEL_KEY_QUEUE_TYPE;
extern const std::string METRIC_LABEL_KEY_EXACTLY_ONCE_FLAG;
extern const std::string METRIC_LABEL_KEY_FLUSHER_NODE_ID;

// input file metrics
extern const std::string METRIC_INPUT_RECORDS_TOTAL;
extern const std::string METRIC_INPUT_RECORDS_SIZE_BYTES;
extern const std::string METRIC_INPUT_BATCH_TOTAL;
extern const std::string METRIC_INPUT_READ_TOTAL;
extern const std::string METRIC_INPUT_FILE_SIZE_BYTES;
extern const std::string METRIC_INPUT_FILE_READ_DELAY_TIME_MS;
extern const std::string METRIC_INPUT_FILE_OFFSET_BYTES;
extern const std::string METRIC_INPUT_FILE_MONITOR_TOTAL;

// processor common metrics
extern const std::string METRIC_PROC_IN_RECORDS_TOTAL;
extern const std::string METRIC_PROC_IN_RECORDS_SIZE_BYTES;
extern const std::string METRIC_PROC_OUT_RECORDS_TOTAL;
extern const std::string METRIC_PROC_OUT_RECORDS_SIZE_BYTES;
extern const std::string METRIC_PROC_DISCARD_RECORDS_TOTAL;
extern const std::string METRIC_PROC_TIME_MS;

// processor custom metrics
extern const std::string METRIC_PROC_PARSE_IN_SIZE_BYTES;
extern const std::string METRIC_PROC_PARSE_OUT_SIZE_BYTES;
extern const std::string METRIC_PROC_PARSE_ERROR_TOTAL;
extern const std::string METRIC_PROC_PARSE_SUCCESS_TOTAL;
extern const std::string METRIC_PROC_KEY_COUNT_NOT_MATCH_ERROR_TOTAL;
extern const std::string METRIC_PROC_HISTORY_FAILURE_TOTAL;
extern const std::string METRIC_PROC_SPLIT_MULTILINE_LOG_MATCHED_RECORDS_TOTAL;
extern const std::string METRIC_PROC_SPLIT_MULTILINE_LOG_MATCHED_LINES_TOTAL;
extern const std::string METRIC_PROC_SPLIT_MULTILINE_LOG_UNMATCHED_LINES_TOTAL;

// processor filter metrics
extern const std::string METRIC_PROC_FILTER_IN_SIZE_BYTES;
extern const std::string METRIC_PROC_FILTER_OUT_SIZE_BYTES;
extern const std::string METRIC_PROC_FILTER_ERROR_TOTAL;
extern const std::string METRIC_PROC_FILTER_RECORDS_TOTAL;

// processor desensitize metrics
extern const std::string METRIC_PROC_DESENSITIZE_RECORDS_TOTAL;

// processor merge multiline log metrics
extern const std::string METRIC_PROC_MERGE_MULTILINE_LOG_MERGED_RECORDS_TOTAL;
extern const std::string METRIC_PROC_MERGE_MULTILINE_LOG_UNMATCHED_RECORDS_TOTAL;

// processor parse container log native metrics
extern const std::string METRIC_PROC_PARSE_STDOUT_TOTAL;
extern const std::string METRIC_PROC_PARSE_STDERR_TOTAL;

// flusher common metrics
extern const std::string METRIC_FLUSHER_ERROR_TOTAL;
extern const std::string METRIC_FLUSHER_DISCARD_RECORDS_TOTAL;
extern const std::string METRIC_FLUSHER_SUCCESS_RECORDS_TOTAL;
extern const std::string METRIC_FLUSHER_SUCCESS_TIME_MS;
extern const std::string METRIC_FLUSHER_ERROR_TIME_MS;

// flusher sls metrics
extern const std::string METRIC_FLUSHER_NETWORK_ERROR_TOTAL;
extern const std::string METRIC_FLUSHER_QUOTA_ERROR_TOTAL;
extern const std::string METRIC_FLUSHER_RETRIES_TOTAL;
extern const std::string METRIC_FLUSHER_RETRIES_ERROR_TOTAL;

//////////////////////////////////////////////////////////////////////////
// plugin
//////////////////////////////////////////////////////////////////////////
// common metrics
extern const std::string METRIC_PLUGIN_IN_EVENTS_CNT;
extern const std::string METRIC_PLUGIN_IN_EVENT_GROUP_SIZE_BYTES;

//////////////////////////////////////////////////////////////////////////
// component
//////////////////////////////////////////////////////////////////////////
// common metrics
extern const std::string METRIC_COMPONENT_IN_EVENTS_CNT;
extern const std::string METRIC_COMPONENT_IN_ITEMS_CNT;
extern const std::string METRIC_COMPONENT_IN_EVENT_GROUP_SIZE_BYTES;
extern const std::string METRIC_COMPONENT_IN_ITEM_SIZE_BYTES;
extern const std::string METRIC_COMPONENT_OUT_EVENTS_CNT;
extern const std::string METRIC_COMPONENT_OUT_ITEMS_CNT;
extern const std::string METRIC_COMPONENT_OUT_ITEM_SIZE_BYTES;
extern const std::string METRIC_COMPONENT_TOTAL_DELAY_MS;
extern const std::string METRIC_COMPONENT_DISCARDED_ITEMS_CNT;
extern const std::string METRIC_COMPONENT_DISCARDED_ITEMS_SIZE_BYTES;

// batcher metrics
extern const std::string METRIC_COMPONENT_BATCHER_EVENT_BATCHES_CNT;
extern const std::string METRIC_COMPONENT_BATCHER_BUFFERED_GROUPS_CNT;
extern const std::string METRIC_COMPONENT_BATCHER_BUFFERED_EVENTS_CNT;
extern const std::string METRIC_COMPONENT_BATCHER_BUFFERED_SIZE_BYTES;

// queue metrics
extern const std::string METRIC_COMPONENT_QUEUE_SIZE_CNT;
extern const std::string METRIC_COMPONENT_QUEUE_SIZE_BYTES;
extern const std::string METRIC_COMPONENT_QUEUE_VALID_TO_PUSH_FLAG;
extern const std::string METRIC_COMPONENT_QUEUE_EXTRA_BUFFER_SIZE;
extern const std::string METRIC_COMPONENT_QUEUE_EXTRA_BUFFER_SIZE_BYTES;
extern const std::string METRIC_COMPONENT_QUEUE_DISCARDED_EVENTS_CNT;

//////////////////////////////////////////////////////////////////////////
// pipeline
//////////////////////////////////////////////////////////////////////////
extern const std::string METRIC_PIPELINE_START_TIME;
extern const std::string METRIC_PIPELINE_PROCESSORS_IN_EVENTS_CNT;
extern const std::string METRIC_PIPELINE_PROCESSORS_IN_EVENT_GROUPS_CNT;
extern const std::string METRIC_PIPELINE_PROCESSORS_IN_EVENT_GROUP_SIZE_BYTES;
extern const std::string METRIC_PIPELINE_PROCESSORS_TOTAL_DELAY_MS;

//////////////////////////////////////////////////////////////////////////
// runner
//////////////////////////////////////////////////////////////////////////
// common metrics
extern const std::string METRIC_RUNNER_IN_EVENTS_CNT;
extern const std::string METRIC_RUNNER_IN_EVENT_GROUPS_CNT;
extern const std::string METRIC_RUNNER_IN_ITEMS_CNT;
extern const std::string METRIC_RUNNER_IN_EVENT_GROUP_SIZE_BYTES;
extern const std::string METRIC_RUNNER_IN_ITEM_SIZE_BYTES;
extern const std::string METRIC_RUNNER_OUT_ITEMS_CNT;
extern const std::string METRIC_RUNNER_TOTAL_DELAY_MS;
extern const std::string METRIC_RUNNER_LAST_RUN_TIME;

// http sink metrics
extern const std::string METRIC_RUNNER_HTTP_SINK_OUT_SUCCESSFUL_ITEMS_CNT;
extern const std::string METRIC_RUNNER_HTTP_SINK_OUT_FAILED_ITEMS_CNT;
extern const std::string METRIC_RUNNER_HTTP_SINK_SENDING_ITEMS_CNT;
extern const std::string METRIC_RUNNER_HTTP_SINK_SEND_CONCURRENCY;

// flusher runner metrics
extern const std::string METRIC_RUNNER_FLUSHER_IN_ITEM_RAW_SIZE_BYTES;
extern const std::string METRIC_RUNNER_FLUSHER_WAITING_ITEMS_CNT;

// file server metrics
extern const std::string METRIC_RUNNER_FILE_WATCHED_DIRS_CNT;
extern const std::string METRIC_RUNNER_FILE_ACTIVE_READERS_CNT;
extern const std::string METRIC_RUNNER_FILE_ENABLE_FILE_INCLUDED_BY_MULTI_CONFIGS_FLAG;
extern const std::string METRIC_RUNNER_FILE_POLLING_MODIFY_CACHE_SIZE;
extern const std::string METRIC_RUNNER_FILE_POLLING_DIR_CACHE_SIZE;
extern const std::string METRIC_RUNNER_FILE_POLLING_FILE_CACHE_SIZE;

} // namespace logtail
