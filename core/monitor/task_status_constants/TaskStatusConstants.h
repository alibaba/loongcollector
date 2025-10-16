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
#pragma once

#include <string>

namespace logtail {

// Task content keys
extern const std::string TASK_CONTENT_KEY_TASK_TYPE;
extern const std::string TASK_CONTENT_KEY_PROJECT;
extern const std::string TASK_CONTENT_KEY_CONFIG_NAME;
extern const std::string TASK_CONTENT_KEY_STATUS;
extern const std::string TASK_CONTENT_KEY_START_TIME;
extern const std::string TASK_CONTENT_KEY_EXPIRE_TIME;

// Task types
extern const std::string& TASK_TYPE_STATIC_FILE;

} // namespace logtail
