/*
 * Copyright 2024 iLogtail Authors
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

#include <json/json.h>

#include <filesystem>
#include <string>

namespace logtail {

enum class ConfigType { Pipeline, Task };

bool LoadConfigDetailFromFile(const std::filesystem::path& filepath, Json::Value& detail);
bool ParseConfigDetail(const std::string& content,
                       const std::string& extenstion,
                       Json::Value& detail,
                       std::string& errorMsg);
bool IsConfigEnabled(const std::string& name, const Json::Value& detail);
ConfigType GetConfigType(const Json::Value& detail);

} // namespace logtail
