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
#include <map>
#include <memory>
#include <string>
#include <json/json.h>

#include "models/StringView.h"
#include "reader/SourceBuffer.h"

namespace logtail {

enum PipelineEventType { VOID_EVENT_TYPE = 0, LOG_EVENT_TYPE = 1, METRIC_EVENT_TYPE = 2, SPAN_EVENT_TYPE = 3 };

const std::string& PipelineEventTypeToString(PipelineEventType t);

class PipelineEvent {
public:
    virtual ~PipelineEvent() {}
    PipelineEventType GetType() const { return mType; }
    std::shared_ptr<SourceBuffer>& GetSourceBuffer() { return mSourceBuffer; }

    time_t GetTimestamp() const { return timestamp; }
    void SetTimestamp(time_t t) { timestamp = t; }
    void SetTimestamp(time_t t, long ns) { 
        timestamp = t; 
        timestampNanosecond = ns; // Only nanosecond part
    }
    long GetTimestampNanosecond() const { return timestampNanosecond; }

    // for debug and test
    virtual Json::Value ToJson() const = 0;
    virtual bool FromJson(const Json::Value&) = 0;
    std::string ToJsonString() const;
    bool FromJsonString(const std::string&);

    virtual uint64_t EventsSizeBytes() = 0;

protected:
    void SetSourceBuffer(std::shared_ptr<SourceBuffer> sourceBuffer) { mSourceBuffer = sourceBuffer; }

    time_t timestamp = 0;
    long timestampNanosecond = 0;
    PipelineEventType mType = VOID_EVENT_TYPE;
    std::shared_ptr<SourceBuffer> mSourceBuffer;
};

extern StringView gEmptyStringView;

} // namespace logtail