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

// OS name, Linux, Windows.
extern const std::string OS_NAME;

extern const char* SLS_EMPTY_STR_FOR_INDEX;

// profile project
extern const std::string PROFILE_PROJECT;
extern const std::string PROFILE_PROJECT_REGION;
extern const std::string PROFILE_LOGSTORE;

// global config
extern const std::string GLOBAL_CONFIG_NODE;
extern const std::string GLOBAL_CONFIG_VERSION;

// user config
extern const std::string USER_CONFIG_NODE;
extern const std::string USER_CONFIG_VERSION;
extern const std::string USER_CONFIG_GROUPTOPIC;
extern const std::string USER_CONFIG_TOPICFORMAT;

// metric config
extern const std::string ROW_MAPPING;
extern const std::string COLUMN_MAPPING;
extern const std::string GROUP_RULE;

extern const std::string TABLE_PREFIX;
extern const std::string TABLE_NAME;
extern const std::string DATE_TYPE;
extern const std::string DATE_SUFFIX;
extern const std::string INTERVAL;
extern const std::string EXPIRE_DAY;

extern const std::string DEFAULT_CONTENT_KEY; //"content"
extern const std::string DEFAULT_REG; //"(.*)"

extern const std::string AGENT_NAME;
extern const std::string LOONGCOLLECTOR_CONFIG;

} // namespace logtail
