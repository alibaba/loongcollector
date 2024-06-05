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

#include <variant>

#ifdef APSARA_UNIT_TEST_MAIN
#include <json/json.h>

#include <string>
#endif

namespace logtail {

struct UntypedSingleValue {
    double mValue;

    constexpr size_t DataSize() const { return sizeof(UntypedSingleValue); }

#ifdef APSARA_UNIT_TEST_MAIN
    Json::Value ToJson() const;
    void FromJson(const Json::Value& value);
#endif
};

using MetricValue = std::variant<std::monostate, UntypedSingleValue>;

size_t DataSize(const MetricValue& value);

#ifdef APSARA_UNIT_TEST_MAIN
Json::Value MetricValueToJson(const MetricValue& value);
MetricValue JsonToMetricValue(const std::string& type, const Json::Value& detail);
#endif

} // namespace logtail
