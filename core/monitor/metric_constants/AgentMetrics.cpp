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
const string METRIC_LABEL_KEY_ALIUIDS = "aliuids";
const string METRIC_LABEL_KEY_INSTANCE_ID = "instance_id";
const string METRIC_LABEL_KEY_START_TIME = "start_time";
const string METRIC_LABEL_KEY_HOSTNAME = "hostname";
const string METRIC_LABEL_KEY_OS = "os";
const string METRIC_LABEL_KEY_OS_DETAIL = "os_detail";
const string METRIC_LABEL_KEY_PROJECT = "project";
const string METRIC_LABEL_KEY_USER_DEFINED_ID = "user_defined_id";
const string METRIC_LABEL_KEY_UUID = "uuid";
const string METRIC_LABEL_KEY_VERSION = "version";

// metric keys
const string METRIC_AGENT_CPU = "cpu";
const string METRIC_AGENT_GO_ROUTINES_TOTAL = "go_routines_total";
const string METRIC_AGENT_INSTANCE_CONFIG_TOTAL = "instance_config_total"; // Not Implemented
const string METRIC_AGENT_MEMORY = "memory_used_mb";
const string METRIC_AGENT_MEMORY_GO = "go_memory_used_mb";
const string METRIC_AGENT_OPEN_FD_TOTAL = "open_fd_total";
const string METRIC_AGENT_PIPELINE_CONFIG_TOTAL = "pipeline_config_total";
const string METRIC_AGENT_HOST_MONITOR_TOTAL = "host_monitor_total";

} // namespace logtail
