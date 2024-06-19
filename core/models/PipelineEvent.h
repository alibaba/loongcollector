/*
 * Copyright 2023 iLogtail Authors
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

#include <cstdint>
#include <ctime>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "common/memory/SourceBuffer.h"
#include "models/StringView.h"

#ifdef APSARA_UNIT_TEST_MAIN
#include <json/json.h>
#endif

namespace logtail {

class PipelineEventGroup;

class PipelineEvent {
public:
    enum class Type { NONE, LOG, METRIC, SPAN };

    virtual ~PipelineEvent() = default;

    virtual std::unique_ptr<PipelineEvent> Copy() const = 0;

    Type GetType() const { return mType; }
    time_t GetTimestamp() const { return mTimestamp; }
    std::optional<uint32_t> GetTimestampNanosecond() const { return mTimestampNanosecond; }
    void SetTimestamp(time_t t) { mTimestamp = t; }
    void SetTimestamp(time_t t, uint32_t ns) {
        mTimestamp = t;
        mTimestampNanosecond = ns; // Only nanosecond part
    }
    void SetTimestamp(time_t t, std::optional<uint32_t> ns) {
        mTimestamp = t;
        mTimestampNanosecond = ns; // Only nanosecond part
    }
    void ResetPipelineEventGroup(PipelineEventGroup* ptr) { mPipelineEventGroupPtr = ptr; }
    std::shared_ptr<SourceBuffer>& GetSourceBuffer();

    virtual size_t DataSize() const { return sizeof(decltype(mTimestamp)) + sizeof(decltype(mTimestampNanosecond)); };

#ifdef APSARA_UNIT_TEST_MAIN
    virtual Json::Value ToJson(bool enableEventMeta = false) const = 0;
    virtual bool FromJson(const Json::Value&) = 0;
    std::string ToJsonString(bool enableEventMeta = false) const;
    bool FromJsonString(const std::string&);
#endif

protected:
    PipelineEvent(Type type, PipelineEventGroup* ptr);

    Type mType = Type::NONE;
    time_t mTimestamp = 0;
    std::optional<uint32_t> mTimestampNanosecond;
    PipelineEventGroup* mPipelineEventGroupPtr = nullptr;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PipelineEventGroupUnittest;
#endif
};

const std::string& PipelineEventTypeToString(PipelineEvent::Type t);

extern StringView gEmptyStringView;

} // namespace logtail
