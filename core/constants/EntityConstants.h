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

#include <string>

namespace logtail {

extern const std::string DEFAULT_ENV_KEY_HOST_TYPE;
extern const std::string DEFAULT_ENV_VALUE_ECS;
extern const std::string DEFAULT_ENV_VALUE_HOST;
extern const std::string DEFAULT_CONTENT_KEY_ENTITY_TYPE;
extern const std::string DEFAULT_CONTENT_KEY_ENTITY_ID;
extern const std::string DEFAULT_CONTENT_KEY_DOMAIN;
extern const std::string DEFAULT_CONTENT_VALUE_DOMAIN_ACS;
extern const std::string DEFAULT_CONTENT_VALUE_DOMAIN_INFRA;
extern const std::string DEFAULT_CONTENT_KEY_FIRST_OBSERVED_TIME;
extern const std::string DEFAULT_CONTENT_KEY_LAST_OBSERVED_TIME;
extern const std::string DEFAULT_CONTENT_KEY_KEEP_ALIVE_SECONDS;
extern const std::string DEFAULT_CONTENT_KEY_METHOD;
extern const std::string DEFAULT_CONTENT_VALUE_METHOD_UPDATE;
extern const std::string DEFAULT_CONTENT_VALUE_METHOD_EXPIRE;

// for process entity
extern const std::string DEFAULT_CONTENT_VALUE_ENTITY_TYPE_PROCESS;
extern const std::string DEFAULT_CONTENT_KEY_PROCESS_PID;
extern const std::string DEFAULT_CONTENT_KEY_PROCESS_PPID;
extern const std::string DEFAULT_CONTENT_KEY_PROCESS_USER;
extern const std::string DEFAULT_CONTENT_KEY_PROCESS_COMM;
extern const std::string DEFAULT_CONTENT_KEY_PROCESS_CREATE_TIME;
extern const std::string DEFAULT_CONTENT_KEY_PROCESS_CWD;
extern const std::string DEFAULT_CONTENT_KEY_PROCESS_BINARY;
extern const std::string DEFAULT_CONTENT_KEY_PROCESS_ARGUMENTS;
extern const std::string DEFAULT_CONTENT_KEY_PROCESS_LANGUAGE;
extern const std::string DEFAULT_CONTENT_KEY_PROCESS_CONTAINER_ID;
} // namespace logtail
