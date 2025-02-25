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

#include "constants/EntityConstants.h"

using namespace std;

namespace logtail {

// should keep same with meta_collector_const.go
const string DEFAULT_CONTENT_KEY_ENTITY_TYPE = "__entity_type__";
const string DEFAULT_CONTENT_KEY_ENTITY_ID = "__entity_id__";
const string DEFAULT_CONTENT_KEY_DOMAIN = "__domain__";
const string DEFAULT_CONTENT_KEY_FIRST_OBSERVED_TIME = "__first_observed_time__";
const string DEFAULT_CONTENT_KEY_LAST_OBSERVED_TIME = "__last_observed_time__";
const string DEFAULT_CONTENT_KEY_KEEP_ALIVE_SECONDS = "__keep_alive_seconds__";
const string DEFAULT_CONTENT_KEY_METHOD = "__method__";

const string DEFAULT_VALUE_DOMAIN_ACS = "acs";
const string DEFAULT_VALUE_DOMAIN_INFRA = "infra";
const string DEFAULT_HOST_TYPE_ECS = "acs.ecs.instance";
const string DEFAULT_HOST_TYPE_HOST = "acs.host.instance";
const string DEFAULT_CONTENT_VALUE_METHOD_UPDATE = "update";
const string DEFAULT_CONTENT_VALUE_METHOD_EXPIRE = "expire";

// process entity
const string DEFAULT_CONTENT_VALUE_ENTITY_TYPE_ECS_PROCESS = "acs.ecs.process";
const string DEFAULT_CONTENT_VALUE_ENTITY_TYPE_HOST_PROCESS = "infra.host.process";
const string DEFAULT_CONTENT_KEY_PROCESS_PID = "pid";
const string DEFAULT_CONTENT_KEY_PROCESS_PPID = "ppid";
const string DEFAULT_CONTENT_KEY_PROCESS_USER = "user";
const string DEFAULT_CONTENT_KEY_PROCESS_COMM = "comm";
const string DEFAULT_CONTENT_KEY_PROCESS_KTIME = "ktime";
const string DEFAULT_CONTENT_KEY_PROCESS_CWD = "cwd";
const string DEFAULT_CONTENT_KEY_PROCESS_BINARY = "binary";
const string DEFAULT_CONTENT_KEY_PROCESS_ARGUMENTS = "arguments";
const string DEFAULT_CONTENT_KEY_PROCESS_LANGUAGE = "language";
const string DEFAULT_CONTENT_KEY_PROCESS_CONTAINER_ID = "container_id";

// link
const string DEFAULT_CONTENT_KEY_SRC_DOMAIN = "__src_domain__";
const string DEFAULT_CONTENT_KEY_SRC_ENTITY_TYPE = "__src_entity_type__";
const string DEFAULT_CONTENT_KEY_SRC_ENTITY_ID = "__src_entity_id__";
const string DEFAULT_CONTENT_KEY_DEST_DOMAIN = "__dest_domain__";
const string DEFAULT_CONTENT_KEY_DEST_ENTITY_TYPE = "__dest_entity_type__";
const string DEFAULT_CONTENT_KEY_DEST_ENTITY_ID = "__dest_entity_id__";
const string DEFAULT_CONTENT_KEY_RELATION_TYPE = "__relation_type__";
} // namespace logtail
