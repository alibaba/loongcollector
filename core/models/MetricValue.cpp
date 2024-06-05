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

#include "models/MetricValue.h"

using namespace std;

namespace logtail {

size_t DataSize(const MetricValue& value) {
    return visit(
        [](auto&& arg) {
            using T = decay_t<decltype(arg)>;
            if constexpr (is_same_v<T, monostate>) {
                return 0UL;
            } else {
                return arg.DataSize();
            }
        },
        value);
}

#ifdef APSARA_UNIT_TEST_MAIN
Json::Value UntypedSingleValue::ToJson() const {
    return Json::Value(mValue);
}

void UntypedSingleValue::FromJson(const Json::Value& value) {
    mValue = value.asFloat();
}

Json::Value MetricValueToJson(const MetricValue& value) {
    Json::Value res;
    visit(
        [&](auto&& arg) {
            using T = decay_t<decltype(arg)>;
            if constexpr (is_same_v<T, UntypedSingleValue>) {
                res["type"] = "untyped_single_value";
                res["detail"] = get<UntypedSingleValue>(value).ToJson();
            } else if constexpr (is_same_v<T, monostate>) {
                res["type"] = "unknown";
            }
        },
        value);
    return res;
}

MetricValue JsonToMetricValue(const string& type, const Json::Value& detail) {
    if (type == "untyped_single_value") {
        UntypedSingleValue v;
        v.FromJson(detail);
        return v;
    } else {
        return MetricValue();
    }
}
#endif

} // namespace logtail
