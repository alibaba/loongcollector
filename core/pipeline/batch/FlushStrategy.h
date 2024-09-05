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

#include <json/json.h>

#include <cstdint>
#include <ctime>

#include "pipeline/batch/BatchStatus.h"
#include "models/PipelineEventPtr.h"

namespace logtail {

struct DefaultFlushStrategyOptions {
    uint32_t mMaxSizeBytes = 0;
    uint32_t mMaxCnt = 0;
    uint32_t mTimeoutSecs = 0;
};

template <class T = EventBatchStatus>
class EventFlushStrategy {
public:
    void SetMaxSizeBytes(uint32_t size) { mMaxSizeBytes = size; }
    void SetMaxCnt(uint32_t cnt) { mMaxCnt = cnt; }
    void SetTimeoutSecs(uint32_t secs) { mTimeoutSecs = secs; }
    uint32_t GetMaxSizeBytes() const { return mMaxSizeBytes; }
    uint32_t GetMaxCnt() const { return mMaxCnt; }
    uint32_t GetTimeoutSecs() const { return mTimeoutSecs; }

    // should be called after event is added
    bool NeedFlushBySize(const T& status) { return status.GetSize() >= mMaxSizeBytes; }
    bool NeedFlushByCnt(const T& status) { return status.GetCnt() == mMaxCnt; }
    // should be called before event is added
    bool NeedFlushByTime(const T& status, const PipelineEventPtr& e) {
        return time(nullptr) - status.GetCreateTime() >= mTimeoutSecs;
    }

private:
    uint32_t mMaxSizeBytes = 0;
    uint32_t mMaxCnt = 0;
    uint32_t mTimeoutSecs = 0;
};

class GroupFlushStrategy {
public:
    GroupFlushStrategy(uint32_t size, uint32_t timeout) : mMaxSizeBytes(size), mTimeoutSecs(timeout) {}

    void SetMaxSizeBytes(uint32_t size) { mMaxSizeBytes = size; }
    void SetTimeoutSecs(uint32_t secs) { mTimeoutSecs = secs; }
    uint32_t GetMaxSizeBytes() const { return mMaxSizeBytes; }
    uint32_t GetTimeoutSecs() const { return mTimeoutSecs; }

    // should be called after event is added
    bool NeedFlushBySize(const GroupBatchStatus& status) { return status.GetSize() >= mMaxSizeBytes; }
    // should be called before event is added
    bool NeedFlushByTime(const GroupBatchStatus& status) {
        return time(nullptr) - status.GetCreateTime() >= mTimeoutSecs;
    }

private:
    uint32_t mMaxSizeBytes = 0;
    uint32_t mTimeoutSecs = 0;
};

template <>
bool EventFlushStrategy<SLSEventBatchStatus>::NeedFlushByTime(const SLSEventBatchStatus& status,
                                                                     const PipelineEventPtr& e);

} // namespace logtail
