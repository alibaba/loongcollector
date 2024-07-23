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

// common plugin labels
extern const std::string METRIC_LABEL_PROJECT;
extern const std::string METRIC_LABEL_LOGSTORE;
extern const std::string METRIC_LABEL_REGION;
extern const std::string METRIC_LABEL_CONFIG_NAME;
extern const std::string METRIC_LABEL_PLUGIN_NAME;
extern const std::string METRIC_LABEL_PLUGIN_ID;

// input file plugin labels
extern const std::string METRIC_LABEL_FILE_DEV;
extern const std::string METRIC_LABEL_FILE_INODE;
extern const std::string METRIC_LABEL_FILE_NAME;

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

} // namespace logtail
