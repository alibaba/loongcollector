// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "constants/Constants.h"

namespace logtail {

#if defined(__linux__)
const std::string OS_NAME = "Linux";
#elif defined(_MSC_VER)
const std::string OS_NAME = "Windows";
#endif

const std::string DEFAULT_CONTENT_KEY = "content";

// profile project
const std::string PROFILE_PROJECT = "profile_project";
const std::string PROFILE_PROJECT_REGION = "profile_project_region";
const std::string PROFILE_LOGSTORE = "profile_logstore";

const std::string USER_CONFIG_NODE = "metrics";

const std::string AGENT_NAME = "LoongCollector";
const std::string LOONGCOLLECTOR_CONFIG = "loongcollector_config.json";

const std::string REGION_CONFIG = "region_config";
const std::string READABLE_REGION_CONFIG = "readable_region_config";

} // namespace logtail
