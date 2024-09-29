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
const string METRIC_LABEL_KEY_IP = "ip";
const string METRIC_LABEL_KEY_OS = "os";
const string METRIC_LABEL_KEY_OS_DETAIL = "os_detail";
const string METRIC_LABEL_KEY_PROJECT = "project";
const string METRIC_LABEL_KEY_USER_DEFINED_ID = "user_defined_id";
const string METRIC_LABEL_KEY_UUID = "uuid";
const string METRIC_LABEL_KEY_VERSION = "version";

// metric keys
const string METRIC_AGENT_CPU = "agent_cpu_percent";
const string METRIC_AGENT_GO_ROUTINES_TOTAL = "agent_go_routines_total";
const string METRIC_AGENT_INSTANCE_CONFIG_TOTAL = "agent_instance_config_total"; // Not Implemented
const string METRIC_AGENT_MEMORY = "agent_memory_used_mb";
const string METRIC_AGENT_MEMORY_GO = "agent_go_memory_used_mb";
const string METRIC_AGENT_OPEN_FD_TOTAL = "agent_open_fd_total";
const string METRIC_AGENT_PIPELINE_CONFIG_TOTAL = "agent_pipeline_config_total";

} // namespace logtail
