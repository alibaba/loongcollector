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
const string METRIC_LABEL_KEY_LOGSTORE = "logstore";
const string METRIC_LABEL_KEY_PIPELINE_NAME = "pipeline_name";
const string METRIC_LABEL_KEY_REGION = "region";

// label values
const string METRIC_LABEL_KEY_METRIC_CATEGORY_PIPELINE = "pipeline";

// metric keys
const string METRIC_PIPELINE_PROCESSORS_IN_EVENTS_TOTAL = "pipeline_processors_in_events_total";
const string METRIC_PIPELINE_PROCESSORS_IN_EVENT_GROUPS_TOTAL = "pipeline_processors_in_event_groups_total";
const string METRIC_PIPELINE_PROCESSORS_IN_SIZE_BYTES = "pipeline_processors_in_size_bytes";
const string METRIC_PIPELINE_PROCESSORS_TOTAL_PROCESS_TIME_MS = "pipeline_processors_total_process_time_ms";
const string METRIC_PIPELINE_FLUSHERS_IN_EVENTS_TOTAL = "pipeline_flushers_in_events_total";
const string METRIC_PIPELINE_FLUSHERS_IN_EVENT_GROUPS_TOTAL = "pipeline_flushers_in_event_groups_total";
const string METRIC_PIPELINE_FLUSHERS_IN_SIZE_BYTES = "pipeline_flushers_in_size_bytes";
const string METRIC_PIPELINE_FLUSHERS_TOTAL_PACKAGE_TIME_MS = "pipeline_flushers_total_package_time_ms";
const string METRIC_PIPELINE_START_TIME = "pipeline_start_time";

} // namespace logtail
