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

#pragma once

#include <string>

namespace logtail {

extern const std::string HTTP_POST;
extern const std::string HTTP_GET;

extern const std::string HOST;
extern const std::string DATE;
extern const std::string USER_AGENT;
extern const std::string CONTENT_LENGTH;
extern const std::string CONTENT_TYPE;
extern const std::string AUTHORIZATION;
extern const std::string SIGNATURE;

extern const std::string TYPE_LOG_PROTOBUF;

} // namespace logtail
