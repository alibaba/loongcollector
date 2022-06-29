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
#include <json/json.h>

// JSON utility (for jsoncpp).
namespace logtail {

bool IsValidJson(const char* buffer, int32_t size);
void CheckNameExist(const Json::Value& value, const std::string& name);
bool GetBoolValue(const Json::Value& value, const std::string& name);
bool GetBoolValue(const Json::Value& value, const std::string& name, const bool defValue);
std::string GetStringValue(const Json::Value& value, const std::string& name);
std::string GetStringValue(const Json::Value& value, const std::string& name, const std::string& defValue);
int32_t GetIntValue(const Json::Value& value, const std::string& name);
int32_t GetIntValue(const Json::Value& value, const std::string& name, const int32_t defValue);
int64_t GetInt64Value(const Json::Value& value, const std::string& name);
int64_t GetInt64Value(const Json::Value& value, const std::string& name, const int64_t defValue);

// Load value from env and config JSON, priority (from low to high):
// - env named by @name.
// - env named by @envName.
// - config named by @name.
//
// @return true if succeed to load.
bool LoadInt32Parameter(int32_t& value, const Json::Value& confJSON, const char* name, const char* envName);
bool LoadStringParameter(std::string& value, const Json::Value& confJSON, const char* name, const char* envName);
bool LoadBooleanParameter(bool& value, const Json::Value& confJSON, const char* name, const char* envName);

template <typename T>
void SetNotFoundJsonMember(Json::Value& j, const std::string& k, const T& v) {
    if (!j.isMember(k)) {
        j[k] = v;
    }
}

} // namespace logtail
