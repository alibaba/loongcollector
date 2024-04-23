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

#include <json/json.h>

#include <cstdint>
#include <ctime>
#include <map>
#include <memory>
#include <string>

#include "models/StringView.h"
#include "common/memory/SourceBuffer.h"

namespace logtail {

class PipelineEventGroup;

class PipelineEvent {
public:
    enum class Type { NONE, LOG, METRIC, SPAN };
    
    virtual ~PipelineEvent() = default;

    Type GetType() const { return mType; }
    time_t GetTimestamp() const { return timestamp; }
    long GetTimestampNanosecond() const { return timestampNanosecond; }
    void SetTimestamp(time_t t) { timestamp = t; }
    void SetTimestamp(time_t t, long ns) {
        timestamp = t;
        timestampNanosecond = ns; // Only nanosecond part
    }
    void ResetPipelineEventGroup(PipelineEventGroup* ptr) { mPipelineEventGroupPtr = ptr; }
    std::shared_ptr<SourceBuffer>& GetSourceBuffer();

    virtual uint64_t EventsSizeBytes() = 0;

#ifdef APSARA_UNIT_TEST_MAIN
    virtual Json::Value ToJson() const = 0;
    virtual bool FromJson(const Json::Value&) = 0;
    std::string ToJsonString() const;
    bool FromJsonString(const std::string&);
#endif

protected:
    PipelineEvent(Type type, PipelineEventGroup* ptr);

    Type mType = Type::NONE;
    time_t timestamp = 0;
    long timestampNanosecond = 0;
    PipelineEventGroup* mPipelineEventGroupPtr = nullptr;
};

const std::string& PipelineEventTypeToString(PipelineEvent::Type t);

extern StringView gEmptyStringView;

} // namespace logtail
