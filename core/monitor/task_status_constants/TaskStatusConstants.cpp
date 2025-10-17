/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "monitor/task_status_constants/TaskStatusConstants.h"

#include "plugin/input/InputStaticFile.h"

namespace logtail {

// Task content keys
const std::string TASK_CONTENT_KEY_TASK_TYPE = "task_type";
const std::string TASK_CONTENT_KEY_PROJECT = "project";
const std::string TASK_CONTENT_KEY_CONFIG_NAME = "config_name";
const std::string TASK_CONTENT_KEY_STATUS = "status";
const std::string TASK_CONTENT_KEY_START_TIME = "start_time";
const std::string TASK_CONTENT_KEY_EXPIRE_TIME = "expire_time";

// Task types
const std::string& TASK_TYPE_STATIC_FILE = InputStaticFile::sName;

} // namespace logtail
