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
#include <string>

#include "common/memory/SourceBuffer.h"
#include "models/MetricValue.h"
#include "models/PipelineEvent.h"
#include "models/SizedContainer.h"

namespace logtail {

class MetricEvent : public PipelineEvent {
    friend class PipelineEventGroup;
    friend class EventPool;

public:
    std::unique_ptr<PipelineEvent> Copy() const override;
    void Reset() override;
    
    StringView GetName() const { return mName; }
    void SetName(const std::string& name);
    void SetNameNoCopy(StringView name);

    template <typename T>
    bool Is() const {
        return std::holds_alternative<T>(mValue);
    }

    template <typename T>
    constexpr std::add_pointer_t<const T> GetValue() const noexcept {
        return std::get_if<T>(&mValue);
    }

    template <typename T>
    constexpr std::add_pointer_t<T> MutableValue() noexcept {
        return std::get_if<T>(&mValue);
    }

    template <typename T>
    void SetValue(const T& value) {
        mValue = value;
    }

    template <typename T, typename... Args>
    void SetValue(Args&&... args) {
        mValue = T{std::forward<Args>(args)...};
    }

    void SetValue(const std::map<StringView, double>& multiDoubleValues) {
        mValue = UntypedMultiDoubleValues{multiDoubleValues, this};
    }

    void SetValue(const UntypedMultiDoubleValues& multiDoubleValues) {
        mValue = UntypedMultiDoubleValues{multiDoubleValues.mValues, this};
    }

    StringView GetTag(StringView key) const;
    bool HasTag(StringView key) const;
    void SetTag(StringView key, StringView val);
    void SetTag(const std::string& key, const std::string& val);
    void SetTagNoCopy(const StringBuffer& key, const StringBuffer& val);
    void SetTagNoCopy(StringView key, StringView val);
    void DelTag(StringView key);

    std::map<StringView, StringView>::const_iterator TagsBegin() const { return mTags.mInner.begin(); }
    std::map<StringView, StringView>::const_iterator TagsEnd() const { return mTags.mInner.end(); }
    size_t TagsSize() const { return mTags.mInner.size(); }

    size_t DataSize() const override;

#ifdef APSARA_UNIT_TEST_MAIN
    Json::Value ToJson(bool enableEventMeta = false) const override;
    bool FromJson(const Json::Value&) override;
#endif

private:
    MetricEvent(PipelineEventGroup* ptr);

    StringView mName;
    MetricValue mValue;
    SizedMap mTags;
};

} // namespace logtail
