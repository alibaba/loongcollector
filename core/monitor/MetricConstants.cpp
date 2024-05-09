// Copyright 2023 iLogtail Authors
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

namespace logtail {


const std::string METRIC_FIELD_REGION = "region";
const std::string METRIC_REGION_DEFAULT = "default";
const std::string METRIC_SLS_LOGSTORE_NAME = "shennong_log_profile";
const std::string METRIC_TOPIC_TYPE = "logtail_metric";
const std::string METRIC_TOPIC_FIELD_NAME = "__topic__";

const std::string LABEL_PREFIX = "label.";
const std::string VALUE_PREFIX = "value.";


// processor common metrics
const std::string METRIC_PROC_IN_RECORDS_TOTAL = "proc_in_records_total";
const std::string METRIC_PROC_IN_RECORDS_SIZE_BYTES = "proc_in_records_size_bytes";
const std::string METRIC_PROC_OUT_RECORDS_TOTAL = "proc_out_records_total";
const std::string METRIC_PROC_OUT_RECORDS_SIZE_BYTES = "proc_out_records_size_bytes";
const std::string METRIC_PROC_DISCARD_RECORDS_TOTAL = "proc_discard_records_total";
const std::string METRIC_PROC_TIME_MS = "proc_time_ms";

// processor cunstom metrics
const std::string METRIC_PROC_PARSE_IN_SIZE_BYTES = "proc_parse_in_size_bytes";
const std::string METRIC_PROC_PARSE_OUT_SIZE_BYTES = "proc_parse_out_size_bytes";

const std::string METRIC_PROC_PARSE_ERROR_TOTAL = "proc_parse_error_total";
const std::string METRIC_PROC_PARSE_SUCCESS_TOTAL = "proc_parse_success_total";
const std::string METRIC_PROC_KEY_COUNT_NOT_MATCH_ERROR_TOTAL = "proc_key_count_not_match_error_total";
const std::string METRIC_PROC_HISTORY_FAILURE_TOTAL = "proc_history_failure_total";

const std::string METRIC_PROC_SPLIT_MULTILINE_LOG_MATCHED_RECORDS_TOTAL
    = "proc_split_multiline_log_matched_records_total";
const std::string METRIC_PROC_SPLIT_MULTILINE_LOG_MATCHED_LINES_TOTAL = "proc_split_multiline_log_matched_lines_total";
const std::string METRIC_PROC_SPLIT_MULTILINE_LOG_UNMATCHED_LINES_TOTAL
    = "proc_split_multiline_log_unmatched_lines_total";

// processor filter metrics
const std::string METRIC_PROC_FILTER_IN_SIZE_BYTES = "proc_filter_in_size_bytes";
const std::string METRIC_PROC_FILTER_OUT_SIZE_BYTES = "proc_filter_out_size_bytes";
const std::string METRIC_PROC_FILTER_ERROR_TOTAL = "proc_filter_error_total";
const std::string METRIC_PROC_FILTER_RECORDS_TOTAL = "proc_filter_records_total";


// processore plugin name
const std::string PLUGIN_PROCESSOR_PARSE_REGEX_NATIVE = "processor_parse_regex_native";

// processor desensitize metrics
const std::string METRIC_PROC_DESENSITIZE_RECORDS_TOTAL = "proc_desensitize_records_total";

// processor merge multiline log metrics
const std::string METRIC_PROC_MERGE_MULTILINE_LOG_MERGED_RECORDS_TOTAL
    = "proc_merge_multiline_log_merged_records_total";
const std::string METRIC_PROC_MERGE_MULTILINE_LOG_UNMATCHED_RECORDS_TOTAL
    = "proc_merge_multiline_log_unmatched_records_total";

// processor parse container log native metrics
const std::string METRIC_PROC_PARSE_STDOUT_TOTAL = "proc_parse_stdout_total";
const std::string METRIC_PROC_PARSE_STDERR_TOTAL = "proc_parse_stderr_total";

} // namespace logtail
