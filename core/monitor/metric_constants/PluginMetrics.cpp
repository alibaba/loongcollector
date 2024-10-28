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
const string METRIC_LABEL_KEY_PLUGIN_ID = "plugin_id";
const string METRIC_LABEL_KEY_PLUGIN_TYPE = "plugin_type";

// label values
const string METRIC_LABEL_KEY_METRIC_CATEGORY_PLUGIN = "plugin";

// metric keys
const string METRIC_PLUGIN_IN_EVENTS_TOTAL = "plugin_in_events_total";
const string METRIC_PLUGIN_IN_EVENT_GROUPS_TOTAL = "plugin_in_event_groups_total";
const string METRIC_PLUGIN_IN_SIZE_BYTES = "plugin_in_size_bytes";
const string METRIC_PLUGIN_OUT_EVENTS_TOTAL = "plugin_out_events_total";
const string METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL = "plugin_out_event_groups_total";
const string METRIC_PLUGIN_OUT_SIZE_BYTES = "plugin_out_size_bytes";
const string METRIC_PLUGIN_TOTAL_DELAY_MS = "plugin_total_delay_ms";
const string METRIC_PLUGIN_TOTAL_PROCESS_TIME_MS = "plugin_total_process_time_ms";

/**********************************************************
 *   input_file
 *   input_container_stdio
 **********************************************************/
const string METRIC_LABEL_KEY_FILE_DEV = "file_dev";
const string METRIC_LABEL_KEY_FILE_INODE = "file_inode";
const string METRIC_LABEL_KEY_FILE_NAME = "file_name";

const string METRIC_PLUGIN_MONITOR_FILE_TOTAL = "plugin_monitor_file_total";
const string METRIC_PLUGIN_SOURCE_READ_OFFSET_BYTES = "plugin_source_read_offset_bytes";
const string METRIC_PLUGIN_SOURCE_SIZE_BYTES = "plugin_source_size_bytes";

/**********************************************************
 *   input_prometheus
 **********************************************************/
const std::string METRIC_LABEL_KEY_JOB = "job";
const std::string METRIC_LABEL_KEY_POD_NAME = "pod_name";
const std::string METRIC_LABEL_KEY_SERVICE_HOST = "service_host";
const std::string METRIC_LABEL_KEY_SERVICE_PORT = "service_port";
const std::string METRIC_LABEL_KEY_STATUS = "status";
const std::string METRIC_LABEL_KEY_INSTANCE = "instance";

const std::string METRIC_PLUGIN_PROM_SUBSCRIBE_TARGETS = "plugin_prom_subscribe_targets";
const std::string METRIC_PLUGIN_PROM_SUBSCRIBE_TOTAL = "plugin_prom_subscribe_total";
const std::string METRIC_PLUGIN_PROM_SUBSCRIBE_TIME_MS = "plugin_prom_subscribe_time_ms";
const std::string METRIC_PLUGIN_PROM_SCRAPE_TIME_MS = "plugin_prom_scrape_time_ms";
const std::string METRIC_PLUGIN_PROM_SCRAPE_DELAY_TOTAL = "plugin_prom_scrape_delay_total";

/**********************************************************
 *   input_ebpf
 *   input_network_observer
 **********************************************************/

const string METRIC_LABEL_KEY_RECV_EVENT_STAGE = "recv_event_stage";
const string METRIC_LABEL_KEY_EVENT_TYPE = "event_type";
const string METRIC_LABEL_KEY_PARSER_PROTOCOL = "parser_protocol";
const string METRIC_LABEL_KEY_PARSE_STATUS = "parser_status";

const string METRIC_LABEL_VALUE_RECV_EVENT_STAGE_POLL_KERNEL = "poll_kernel";
const string METRIC_LABEL_VALUE_RECV_EVENT_STAGE_AFTER_PERF_WORKER = "after_perf_worker";
const string METRIC_LABEL_VALUE_RECV_EVENT_STAGE_REPORT_TO_LC = "report_to_lc";
const string METRIC_LABEL_VALUE_EVENT_TYPE_CONN_STATS = "conn_stats";
const string METRIC_LABEL_VALUE_EVENT_TYPE_DATA_EVENT = "data_event";
const string METRIC_LABEL_VALUE_EVENT_TYPE_CTRL_EVENT = "ctrl_event";
const string METRIC_LABEL_VALUE_EVENT_TYPE_LOG = "log";
const string METRIC_LABEL_VALUE_EVENT_TYPE_METRIC = "metric";
const string METRIC_LABEL_VALUE_EVENT_TYPE_TRACE = "trace";
const string METRIC_LABEL_VALUE_PARSER_PROTOCOL_HTTP = "http";
const string METRIC_LABEL_VALUE_PARSE_STATUS_SUCCESS = "success";
const string METRIC_LABEL_VALUE_PARSE_STATUS_FAILED = "failed";
const string METRIC_LABEL_VALUE_PLUGIN_TYPE_NETWORK_OBSERVER = "network_observer";
const string METRIC_LABEL_VALUE_PLUGIN_TYPE_NETWORK_SECURITY = "network_security";
const string METRIC_LABEL_VALUE_PLUGIN_TYPE_FILE_OBSERVER = "file_observer";
const string METRIC_LABEL_VALUE_PLUGIN_TYPE_FILE_SECURITY = "file_security";
const string METRIC_LABEL_VALUE_PLUGIN_TYPE_PROCESS_OBSERVER = "process_observer";
const string METRIC_LABEL_VALUE_PLUGIN_TYPE_PROCESS_SECURITY = "process_security";

const string METRIC_PLUGIN_EBPF_LOSS_KERNEL_EVENTS_TOTAL = "plugin_ebpf_loss_kernel_events_total";
const string METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_CONNTRACKER_NUM = "plugin_network_observer_conntracker_num";
const string METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_WORKER_HANDLE_EVENTS_TOTAL = "plugin_network_observer_worker_handle_events_total";
const string METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_PROTOCOL_PARSE_RECORDS_TOTAL = "plugin_network_observer_parse_records_total";
const string METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_AGGREGATE_EVENTS_TOTAL = "plugin_network_observer_aggregate_events_total";
const string METRIC_PLUGIN_EBPF_NETWORK_OBSERVER_AGGREGATE_KEY_NUM = "plugin_network_observer_aggregate_key_num";
const string METRIC_PLUGIN_EBPF_PROCESS_CACHE_ENTRIES_NUM = "plugin_process_cache_entries_num";
const string METRIC_PLUGIN_EBPF_PROCESS_CACHE_MISS_TOTAL = "plugin_process_cache_miss_total";

/**********************************************************
 *   all processor （所有解析类的处理插件通用指标。Todo：目前统计还不全、不准确）
 **********************************************************/
const string METRIC_PLUGIN_DISCARDED_EVENTS_TOTAL = "plugin_discarded_events_total";
const string METRIC_PLUGIN_OUT_FAILED_EVENTS_TOTAL = "plugin_out_failed_events_total";
const string METRIC_PLUGIN_OUT_KEY_NOT_FOUND_EVENTS_TOTAL = "plugin_out_key_not_found_events_total";
const string METRIC_PLUGIN_OUT_SUCCESSFUL_EVENTS_TOTAL = "plugin_out_successful_events_total";

/**********************************************************
 *   processor_parse_apsara_native
 *   processor_parse_timestamp_native
 **********************************************************/
const string METRIC_PLUGIN_HISTORY_FAILURE_TOTAL = "plugin_history_failure_total";

/**********************************************************
 *   processor_split_multiline_log_string_native
 **********************************************************/
const string METRIC_PLUGIN_MATCHED_EVENTS_TOTAL = "plugin_matched_events_total";
const string METRIC_PLUGIN_MATCHED_LINES_TOTAL = "plugin_matched_lines_total";
const string METRIC_PLUGIN_UNMATCHED_LINES_TOTAL = "plugin_unmatched_lines_total";

/**********************************************************
 *   processor_merge_multiline_log_native
 **********************************************************/
const string METRIC_PLUGIN_MERGED_EVENTS_TOTAL = "plugin_merged_events_total";
const string METRIC_PLUGIN_UNMATCHED_EVENTS_TOTAL = "plugin_unmatched_events_total";

/**********************************************************
 *   processor_parse_container_log_native
 **********************************************************/
const string METRIC_PLUGIN_PARSE_STDERR_TOTAL = "plugin_parse_stderr_total";
const string METRIC_PLUGIN_PARSE_STDOUT_TOTAL = "plugin_parse_stdout_total";


/**********************************************************
 *   all flusher （所有发送插件通用指标）
 **********************************************************/
const string METRIC_PLUGIN_FLUSHER_TOTAL_PACKAGE_TIME_MS = "plugin_flusher_total_package_time_ms";
const string METRIC_PLUGIN_FLUSHER_OUT_EVENT_GROUPS_TOTAL = "plugin_flusher_send_total";
const string METRIC_PLUGIN_FLUSHER_SEND_DONE_TOTAL = "plugin_flusher_send_done_total";
const string METRIC_PLUGIN_FLUSHER_SUCCESS_TOTAL = "plugin_flusher_success_total";
const string METRIC_PLUGIN_FLUSHER_NETWORK_ERROR_TOTAL = "plugin_flusher_network_error_total";
const string METRIC_PLUGIN_FLUSHER_SERVER_ERROR_TOTAL = "plugin_flusher_server_error_total";
const string METRIC_PLUGIN_FLUSHER_UNAUTH_ERROR_TOTAL = "plugin_flusher_unauth_error_total";
const string METRIC_PLUGIN_FLUSHER_PARAMS_ERROR_TOTAL = "plugin_flusher_params_error_total";
const string METRIC_PLUGIN_FLUSHER_OTHER_ERROR_TOTAL = "plugin_flusher_other_error_total";

/**********************************************************
 *   flusher_sls
 **********************************************************/
const string METRIC_PLUGIN_FLUSHER_SLS_SHARD_WRITE_QUOTA_ERROR_TOTAL = "plugin_flusher_sls_shard_write_quota_error_total";
const string METRIC_PLUGIN_FLUSHER_SLS_PROJECT_QUOTA_ERROR_TOTAL = "plugin_flusher_sls_project_quota_error_total";
const string METRIC_PLUGIN_FLUSHER_SLS_SEQUENCE_ID_ERROR_TOTAL = "plugin_flusher_sls_sequence_id_error_total";
const string METRIC_PLUGIN_FLUSHER_SLS_REQUEST_EXPRIRED_ERROR_TOTAL = "plugin_flusher_sls_request_exprired_error_total";

} // namespace logtail