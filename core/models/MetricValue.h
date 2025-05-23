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

#include <map>
#include <variant>

#ifdef APSARA_UNIT_TEST_MAIN
#include <string>

#include "json/json.h"
#endif

#include "common/StringView.h"
#include "common/memory/SourceBuffer.h"
#include "models/PipelineEvent.h"

namespace logtail {

struct UntypedSingleValue {
    double mValue;

    constexpr size_t DataSize() const { return sizeof(UntypedSingleValue); }

#ifdef APSARA_UNIT_TEST_MAIN
    Json::Value ToJson() const;
    void FromJson(const Json::Value& value);
#endif
};

enum UntypedValueMetricType { MetricTypeCounter, MetricTypeGauge };

struct UntypedMultiDoubleValue {
    UntypedValueMetricType MetricType;
    double Value;
};

struct UntypedMultiDoubleValues {
    std::map<StringView, UntypedMultiDoubleValue> mValues;
    PipelineEvent* mMetricEventPtr;

    UntypedMultiDoubleValues(PipelineEvent* ptr) : mMetricEventPtr(ptr) {}
    UntypedMultiDoubleValues(std::map<StringView, UntypedMultiDoubleValue> values, PipelineEvent* ptr)
        : mValues(values), mMetricEventPtr(ptr) {}

    bool GetValue(StringView key, UntypedMultiDoubleValue& val) const;
    bool HasValue(StringView key) const;
    void SetValue(const std::string& key, UntypedMultiDoubleValue val);
    void SetValue(StringView key, UntypedMultiDoubleValue val);
    void SetValueNoCopy(const StringBuffer& key, UntypedMultiDoubleValue val);
    void SetValueNoCopy(StringView key, UntypedMultiDoubleValue val);
    void DelValue(StringView key);

    std::map<StringView, UntypedMultiDoubleValue>::const_iterator ValuesBegin() const;
    std::map<StringView, UntypedMultiDoubleValue>::const_iterator ValuesEnd() const;
    size_t ValuesSize() const;

    size_t DataSize() const;
    void ResetPipelineEvent(PipelineEvent* ptr) { mMetricEventPtr = ptr; }

#ifdef APSARA_UNIT_TEST_MAIN
    Json::Value ToJson() const;
    void FromJson(const Json::Value& value);
#endif
};

using MetricValue = std::variant<std::monostate, UntypedSingleValue, UntypedMultiDoubleValues>;

size_t DataSize(const MetricValue& value);

} // namespace logtail
