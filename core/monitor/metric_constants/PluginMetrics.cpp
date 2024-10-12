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

} // namespace logtail