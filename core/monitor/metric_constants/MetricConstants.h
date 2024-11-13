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

//////////////////////////////////////////////////////////////////////////
// agent
//////////////////////////////////////////////////////////////////////////

extern const std::string METRIC_LABEL_KEY_ALIUIDS;
extern const std::string METRIC_LABEL_KEY_INSTANCE_ID;
extern const std::string METRIC_LABEL_KEY_START_TIME;
extern const std::string METRIC_LABEL_KEY_OS;
extern const std::string METRIC_LABEL_KEY_OS_DETAIL;
extern const std::string METRIC_LABEL_KEY_PROJECT;
extern const std::string METRIC_LABEL_KEY_USER_DEFINED_ID;
extern const std::string METRIC_LABEL_KEY_UUID;
extern const std::string METRIC_LABEL_KEY_VERSION;

// metric keys
extern const std::string METRIC_AGENT_CPU;
extern const std::string METRIC_AGENT_GO_ROUTINES_TOTAL;
extern const std::string METRIC_AGENT_INSTANCE_CONFIG_TOTAL;
extern const std::string METRIC_AGENT_MEMORY;
extern const std::string METRIC_AGENT_MEMORY_GO;
extern const std::string METRIC_AGENT_OPEN_FD_TOTAL;
extern const std::string METRIC_AGENT_PIPELINE_CONFIG_TOTAL;

//////////////////////////////////////////////////////////////////////////
// pipeline
//////////////////////////////////////////////////////////////////////////

// label keys
extern const std::string METRIC_LABEL_KEY_LOGSTORE;
extern const std::string METRIC_LABEL_KEY_PIPELINE_NAME;
extern const std::string METRIC_LABEL_KEY_REGION;

// metric keys
extern const std::string METRIC_PIPELINE_PROCESSORS_IN_EVENTS_TOTAL;
extern const std::string METRIC_PIPELINE_PROCESSORS_IN_EVENT_GROUPS_TOTAL;
extern const std::string METRIC_PIPELINE_PROCESSORS_IN_SIZE_BYTES;
extern const std::string METRIC_PIPELINE_PROCESSORS_TOTAL_PROCESS_TIME_MS;
extern const std::string METRIC_PIPELINE_FLUSHERS_IN_EVENTS_TOTAL;
extern const std::string METRIC_PIPELINE_FLUSHERS_IN_EVENT_GROUPS_TOTAL;
extern const std::string METRIC_PIPELINE_FLUSHERS_IN_SIZE_BYTES;
extern const std::string METRIC_PIPELINE_FLUSHERS_TOTAL_PACKAGE_TIME_MS;
extern const std::string METRIC_PIPELINE_START_TIME;

//////////////////////////////////////////////////////////////////////////
// plugin
//////////////////////////////////////////////////////////////////////////

// label keys
extern const std::string METRIC_LABEL_KEY_PLUGIN_ID;
extern const std::string METRIC_LABEL_KEY_PLUGIN_TYPE;

// metric keys
extern const std::string& METRIC_PLUGIN_IN_EVENTS_TOTAL;
extern const std::string& METRIC_PLUGIN_IN_EVENT_GROUPS_TOTAL;
extern const std::string& METRIC_PLUGIN_IN_SIZE_BYTES;
extern const std::string& METRIC_PLUGIN_OUT_EVENTS_TOTAL;
extern const std::string& METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL;
extern const std::string& METRIC_PLUGIN_OUT_SIZE_BYTES;
extern const std::string& METRIC_PLUGIN_TOTAL_DELAY_MS;
extern const std::string& METRIC_PLUGIN_TOTAL_PROCESS_TIME_MS;

/**********************************************************
 *   input_file
 *   input_container_stdio
 **********************************************************/
extern const std::string METRIC_LABEL_KEY_FILE_DEV;
extern const std::string METRIC_LABEL_KEY_FILE_INODE;
extern const std::string METRIC_LABEL_KEY_FILE_NAME;

extern const std::string METRIC_PLUGIN_MONITOR_FILE_TOTAL;
extern const std::string METRIC_PLUGIN_SOURCE_READ_OFFSET_BYTES;
extern const std::string METRIC_PLUGIN_SOURCE_SIZE_BYTES;

/**********************************************************
 *   input_prometheus
 **********************************************************/
extern const std::string METRIC_LABEL_KEY_JOB;
extern const std::string METRIC_LABEL_KEY_POD_NAME;
extern const std::string METRIC_LABEL_KEY_SERVICE_HOST;
extern const std::string METRIC_LABEL_KEY_SERVICE_PORT;
extern const std::string METRIC_LABEL_KEY_STATUS;
extern const std::string METRIC_LABEL_KEY_INSTANCE;

extern const std::string METRIC_PLUGIN_PROM_SUBSCRIBE_TARGETS;
extern const std::string METRIC_PLUGIN_PROM_SUBSCRIBE_TOTAL;
extern const std::string METRIC_PLUGIN_PROM_SUBSCRIBE_TIME_MS;
extern const std::string METRIC_PLUGIN_PROM_SCRAPE_TIME_MS;
extern const std::string METRIC_PLUGIN_PROM_SCRAPE_DELAY_TOTAL;

/**********************************************************
 *   input_ebpf
 *   input_network_observer
 **********************************************************/

extern const std::string METRIC_LABEL_KEY_RECV_EVENT_STAGE;
extern const std::string METRIC_LABEL_KEY_EVENT_TYPE;
extern const std::string METRIC_LABEL_KEY_PARSER_PROTOCOL;
extern const std::string METRIC_LABEL_KEY_PARSE_STATUS;

extern const std::string METRIC_LABEL_VALUE_RECV_EVENT_STAGE_POLL_KERNEL;
extern const std::string METRIC_LABEL_VALUE_RECV_EVENT_STAGE_AFTER_PERF_WORKER;
extern const std::string METRIC_LABEL_VALUE_RECV_EVENT_STAGE_REPORT_TO_LC;
extern const std::string METRIC_LABEL_VALUE_EVENT_TYPE_CONN_STATS;
extern const std::string METRIC_LABEL_VALUE_EVENT_TYPE_DATA_EVENT;
extern const std::string METRIC_LABEL_VALUE_EVENT_TYPE_CTRL_EVENT;
extern const std::string METRIC_LABEL_VALUE_EVENT_TYPE_LOG;
extern const std::string METRIC_LABEL_VALUE_EVENT_TYPE_METRIC;
extern const std::string METRIC_LABEL_VALUE_EVENT_TYPE_TRACE;
extern const std::string METRIC_LABEL_VALUE_PARSER_PROTOCOL_HTTP;
extern const std::string METRIC_LABEL_VALUE_PARSE_STATUS_SUCCESS;
extern const std::string METRIC_LABEL_VALUE_PARSE_STATUS_FAILED;
extern const std::string METRIC_LABEL_VALUE_PLUGIN_TYPE_NETWORK_OBSERVER;
extern const std::string METRIC_LABEL_VALUE_PLUGIN_TYPE_NETWORK_SECURITY;
extern const std::string METRIC_LABEL_VALUE_PLUGIN_TYPE_FILE_OBSERVER;
extern const std::string METRIC_LABEL_VALUE_PLUGIN_TYPE_FILE_SECURITY;
extern const std::string METRIC_LABEL_VALUE_PLUGIN_TYPE_PROCESS_OBSERVER;
extern const std::string METRIC_LABEL_VALUE_PLUGIN_TYPE_PROCESS_SECURITY;

extern const std::string METRIC_PLUGIN_EBPF_LOSS_KERNEL_EVENTS_TOTAL;
extern const std::string METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_CONNTRACKER_NUM;
extern const std::string METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_WORKER_HANDLE_EVENTS_TOTAL;
extern const std::string METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_PROTOCOL_PARSE_RECORDS_TOTAL;
extern const std::string METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_AGGREGATE_EVENTS_TOTAL;
extern const std::string METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_AGGREGATE_KEY_NUM;
extern const std::string METRIC_PLUGIN_EBPF_PROCESS_CACHE_ENTRIES_NUM;
extern const std::string METRIC_PLUGIN_EBPF_PROCESS_CACHE_MISS_TOTAL;

/**********************************************************
 *   all processor （所有解析类的处理插件通用指标。Todo：目前统计还不全、不准确）
 **********************************************************/
extern const std::string& METRIC_PLUGIN_DISCARDED_EVENTS_TOTAL;
extern const std::string METRIC_PLUGIN_OUT_FAILED_EVENTS_TOTAL;
extern const std::string METRIC_PLUGIN_OUT_KEY_NOT_FOUND_EVENTS_TOTAL;
extern const std::string METRIC_PLUGIN_OUT_SUCCESSFUL_EVENTS_TOTAL;

/**********************************************************
 *   all flusher （所有发送插件通用指标）
 **********************************************************/
extern const std::string METRIC_PLUGIN_FLUSHER_TOTAL_PACKAGE_TIME_MS;
extern const std::string METRIC_PLUGIN_FLUSHER_OUT_EVENT_GROUPS_TOTAL;
extern const std::string METRIC_PLUGIN_FLUSHER_SEND_DONE_TOTAL;
extern const std::string METRIC_PLUGIN_FLUSHER_SUCCESS_TOTAL;
extern const std::string METRIC_PLUGIN_FLUSHER_NETWORK_ERROR_TOTAL;
extern const std::string METRIC_PLUGIN_FLUSHER_SERVER_ERROR_TOTAL;
extern const std::string METRIC_PLUGIN_FLUSHER_UNAUTH_ERROR_TOTAL;
extern const std::string METRIC_PLUGIN_FLUSHER_PARAMS_ERROR_TOTAL;
extern const std::string METRIC_PLUGIN_FLUSHER_OTHER_ERROR_TOTAL;

/**********************************************************
 *   processor_parse_apsara_native
 *   processor_parse_timestamp_native
 **********************************************************/
extern const std::string METRIC_PLUGIN_HISTORY_FAILURE_TOTAL;

/**********************************************************
 *   processor_split_multiline_log_string_native
 **********************************************************/
extern const std::string METRIC_PLUGIN_MATCHED_EVENTS_TOTAL;
extern const std::string METRIC_PLUGIN_MATCHED_LINES_TOTAL;
extern const std::string METRIC_PLUGIN_UNMATCHED_LINES_TOTAL;

/**********************************************************
 *   processor_merge_multiline_log_native
 **********************************************************/
extern const std::string METRIC_PLUGIN_MERGED_EVENTS_TOTAL;
extern const std::string METRIC_PLUGIN_UNMATCHED_EVENTS_TOTAL;

/**********************************************************
 *   processor_parse_container_log_native
 **********************************************************/
extern const std::string METRIC_PLUGIN_PARSE_STDERR_TOTAL;
extern const std::string METRIC_PLUGIN_PARSE_STDOUT_TOTAL;

/**********************************************************
 *   flusher_sls
 **********************************************************/
extern const std::string METRIC_PLUGIN_FLUSHER_SLS_SHARD_WRITE_QUOTA_ERROR_TOTAL;
extern const std::string METRIC_PLUGIN_FLUSHER_SLS_PROJECT_QUOTA_ERROR_TOTAL;
extern const std::string METRIC_PLUGIN_FLUSHER_SLS_SEQUENCE_ID_ERROR_TOTAL;
extern const std::string METRIC_PLUGIN_FLUSHER_SLS_REQUEST_EXPRIRED_ERROR_TOTAL;

//////////////////////////////////////////////////////////////////////////
// component
//////////////////////////////////////////////////////////////////////////

// label keys
extern const std::string METRIC_LABEL_KEY_COMPONENT_NAME;
extern const std::string METRIC_LABEL_KEY_FLUSHER_PLUGIN_ID;
extern const std::string METRIC_LABEL_KEY_EXACTLY_ONCE_ENABLED;
extern const std::string METRIC_LABEL_KEY_QUEUE_TYPE;
extern const std::string METRIC_LABEL_KEY_GROUP_BATCH_ENABLED;

// label values
extern const std::string METRIC_LABEL_VALUE_COMPONENT_NAME_BATCHER;
extern const std::string METRIC_LABEL_VALUE_COMPONENT_NAME_COMPRESSOR;
extern const std::string METRIC_LABEL_VALUE_COMPONENT_NAME_PROCESS_QUEUE;
extern const std::string METRIC_LABEL_VALUE_COMPONENT_NAME_ROUTER;
extern const std::string METRIC_LABEL_VALUE_COMPONENT_NAME_SENDER_QUEUE;
extern const std::string METRIC_LABEL_VALUE_COMPONENT_NAME_SERIALIZER;

// metric keys
extern const std::string& METRIC_COMPONENT_IN_EVENTS_TOTAL;
extern const std::string& METRIC_COMPONENT_IN_SIZE_BYTES;
extern const std::string& METRIC_COMPONENT_IN_ITEMS_TOTAL;
extern const std::string& METRIC_COMPONENT_OUT_EVENTS_TOTAL;
extern const std::string& METRIC_COMPONENT_OUT_ITEMS_TOTAL;
extern const std::string& METRIC_COMPONENT_OUT_SIZE_BYTES;
extern const std::string& METRIC_COMPONENT_TOTAL_DELAY_MS;
extern const std::string& METRIC_COMPONENT_TOTAL_PROCESS_TIME_MS;
extern const std::string& METRIC_COMPONENT_DISCARDED_ITEMS_TOTAL;
extern const std::string& METRIC_COMPONENT_DISCARDED_SIZE_BYTES;

/**********************************************************
 *   batcher
 **********************************************************/
extern const std::string METRIC_COMPONENT_BATCHER_EVENT_BATCHES_TOTAL;
extern const std::string METRIC_COMPONENT_BATCHER_BUFFERED_GROUPS_TOTAL;
extern const std::string METRIC_COMPONENT_BATCHER_BUFFERED_EVENTS_TOTAL;
extern const std::string METRIC_COMPONENT_BATCHER_BUFFERED_SIZE_BYTES;
extern const std::string METRIC_COMPONENT_BATCHER_TOTAL_ADD_TIME_MS;

/**********************************************************
 *   queue
 **********************************************************/
extern const std::string METRIC_COMPONENT_QUEUE_SIZE;
extern const std::string METRIC_COMPONENT_QUEUE_SIZE_BYTES;
extern const std::string METRIC_COMPONENT_QUEUE_VALID_TO_PUSH_FLAG;
extern const std::string METRIC_COMPONENT_QUEUE_EXTRA_BUFFER_SIZE;
extern const std::string METRIC_COMPONENT_QUEUE_EXTRA_BUFFER_SIZE_BYTES;
extern const std::string& METRIC_COMPONENT_QUEUE_DISCARDED_EVENTS_TOTAL;

extern const std::string METRIC_COMPONENT_QUEUE_FETCHED_ITEMS_TOTAL;
extern const std::string METRIC_COMPONENT_QUEUE_FETCH_TIMES_TOTAL;
extern const std::string METRIC_COMPONENT_QUEUE_VALID_FETCH_TIMES_TOTAL;
extern const std::string METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_REGION_LIMITER_TIMES_TOTAL;
extern const std::string METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_PROJECT_LIMITER_TIMES_TOTAL;
extern const std::string METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_LOGSTORE_LIMITER_TIMES_TOTAL;
extern const std::string METRIC_COMPONENT_QUEUE_FETCH_REJECTED_BY_RATE_LIMITER_TIMES_TOTAL;

//////////////////////////////////////////////////////////////////////////
// runner
//////////////////////////////////////////////////////////////////////////

// label keys
extern const std::string METRIC_LABEL_KEY_RUNNER_NAME;
extern const std::string METRIC_LABEL_KEY_THREAD_NO;

// label values
extern const std::string METRIC_LABEL_VALUE_RUNNER_NAME_FILE_SERVER;
extern const std::string METRIC_LABEL_VALUE_RUNNER_NAME_FLUSHER;
extern const std::string METRIC_LABEL_VALUE_RUNNER_NAME_HTTP_SINK;
extern const std::string METRIC_LABEL_VALUE_RUNNER_NAME_PROCESSOR;
extern const std::string METRIC_LABEL_VALUE_RUNNER_NAME_PROMETHEUS;
extern const std::string METRIC_LABEL_VALUE_RUNNER_NAME_EBPF_SERVER;

// metric keys
extern const std::string& METRIC_RUNNER_IN_EVENTS_TOTAL;
extern const std::string& METRIC_RUNNER_IN_EVENT_GROUPS_TOTAL;
extern const std::string& METRIC_RUNNER_IN_SIZE_BYTES;
extern const std::string& METRIC_RUNNER_IN_ITEMS_TOTAL;
extern const std::string METRIC_RUNNER_LAST_RUN_TIME;
extern const std::string& METRIC_RUNNER_OUT_ITEMS_TOTAL;
extern const std::string& METRIC_RUNNER_TOTAL_DELAY_MS;
extern const std::string METRIC_RUNNER_CLIENT_REGISTER_STATE;
extern const std::string METRIC_RUNNER_CLIENT_REGISTER_RETRY_TOTAL;
extern const std::string METRIC_RUNNER_JOBS_TOTAL;

/**********************************************************
 *   all sinks
 **********************************************************/
extern const std::string METRIC_RUNNER_SINK_OUT_SUCCESSFUL_ITEMS_TOTAL;
extern const std::string METRIC_RUNNER_SINK_OUT_FAILED_ITEMS_TOTAL;
extern const std::string METRIC_RUNNER_SINK_SUCCESSFUL_ITEM_TOTAL_RESPONSE_TIME_MS;
extern const std::string METRIC_RUNNER_SINK_FAILED_ITEM_TOTAL_RESPONSE_TIME_MS;
extern const std::string METRIC_RUNNER_SINK_SENDING_ITEMS_TOTAL;
extern const std::string METRIC_RUNNER_SINK_SEND_CONCURRENCY;

/**********************************************************
 *   flusher runner
 **********************************************************/
extern const std::string METRIC_RUNNER_FLUSHER_IN_RAW_SIZE_BYTES;
extern const std::string METRIC_RUNNER_FLUSHER_WAITING_ITEMS_TOTAL;

/**********************************************************
 *   file server
 **********************************************************/
extern const std::string METRIC_RUNNER_FILE_WATCHED_DIRS_TOTAL;
extern const std::string METRIC_RUNNER_FILE_ACTIVE_READERS_TOTAL;
extern const std::string METRIC_RUNNER_FILE_ENABLE_FILE_INCLUDED_BY_MULTI_CONFIGS_FLAG;
extern const std::string METRIC_RUNNER_FILE_POLLING_MODIFY_CACHE_SIZE;
extern const std::string METRIC_RUNNER_FILE_POLLING_DIR_CACHE_SIZE;
extern const std::string METRIC_RUNNER_FILE_POLLING_FILE_CACHE_SIZE;

/**********************************************************
 *   ebpf server
 **********************************************************/
extern const std::string METRIC_RUNNER_EBPF_START_PLUGIN_TOTAL;
extern const std::string METRIC_RUNNER_EBPF_STOP_PLUGIN_TOTAL;
extern const std::string METRIC_RUNNER_EBPF_SUSPEND_PLUGIN_TOTAL;

} // namespace logtail
