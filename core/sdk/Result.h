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
#include <map>
#include "Closure.h"
#include "Common.h"
#include "rapidjson/document.h"

namespace logtail {
namespace sdk {

    void ExtractJsonResult(const std::string& response, rapidjson::Document& document);

    void JsonMemberCheck(const rapidjson::Value& value, const char* name);

    void ExtractJsonResult(const rapidjson::Value& value, const char* name, int32_t& number);

    void ExtractJsonResult(const rapidjson::Value& value, const char* name, uint32_t& number);

    void ExtractJsonResult(const rapidjson::Value& value, const char* name, int64_t& number);

    void ExtractJsonResult(const rapidjson::Value& value, const char* name, uint64_t& number);

    void ExtractJsonResult(const rapidjson::Value& value, const char* name, bool& boolean);

    void ExtractJsonResult(const rapidjson::Value& value, const char* name, std::string& dst);

    const rapidjson::Value& GetJsonValue(const rapidjson::Value& value, const char* name);

    void ErrorCheck(const std::string& response, const std::string& requestId, const int32_t httpCode);

} // namespace sdk
} // namespace logtail
