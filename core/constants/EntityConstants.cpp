/*
 * Copyright 2024 iLogtail Authors
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

#include "EntityConstants.h"

namespace logtail {

const std::string DEFAULT_HOST_TYPE_ECS = "ecs";
const std::string DEFAULT_HOST_TYPE_HOST = "host";
const std::string DEFAULT_CONTENT_KEY_ENTITY_TYPE = "__entity_type__";
const std::string DEFAULT_CONTENT_KEY_ENTITY_ID = "__entity_id__";
const std::string DEFAULT_CONTENT_KEY_DOMAIN = "__domain__";
const std::string DEFAULT_CONTENT_VALUE_DOMAIN_ACS = "acs";
const std::string DEFAULT_CONTENT_VALUE_DOMAIN_INFRA = "infra";
const std::string DEFAULT_CONTENT_KEY_FIRST_OBSERVED_TIME = "__first_observed_time__";
const std::string DEFAULT_CONTENT_KEY_LAST_OBSERVED_TIME = "__last_observed_time__";
const std::string DEFAULT_CONTENT_KEY_KEEP_ALIVE_SECONDS = "__keep_alive_seconds__";
const std::string DEFAULT_CONTENT_KEY_METHOD = "__method__";
const std::string DEFAULT_CONTENT_VALUE_METHOD_UPDATE = "update";
const std::string DEFAULT_CONTENT_VALUE_METHOD_EXPIRE = "expire";

// for process entity
const std::string DEFAULT_CONTENT_VALUE_ENTITY_TYPE_PROCESS = "process";
const std::string DEFAULT_CONTENT_KEY_PROCESS_PID = "pid";
const std::string DEFAULT_CONTENT_KEY_PROCESS_PPID = "ppid";
const std::string DEFAULT_CONTENT_KEY_PROCESS_USER = "user";
const std::string DEFAULT_CONTENT_KEY_PROCESS_COMM = "comm";
const std::string DEFAULT_CONTENT_KEY_PROCESS_CREATE_TIME = "create_time";
const std::string DEFAULT_CONTENT_KEY_PROCESS_CWD = "cwd";
const std::string DEFAULT_CONTENT_KEY_PROCESS_BINARY = "binary";
const std::string DEFAULT_CONTENT_KEY_PROCESS_ARGUMENTS = "arguments";
const std::string DEFAULT_CONTENT_KEY_PROCESS_LANGUAGE = "language";
const std::string DEFAULT_CONTENT_KEY_PROCESS_CONTAINER_ID = "container_id";
} // namespace logtail
