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
#include <limits>

#include "models/PipelineEventPtr.h"
#include "pipeline/batch/BatchStatus.h"

namespace logtail {

struct DefaultFlushStrategyOptions {
    uint32_t mMaxSizeBytes = std::numeric_limits<uint32_t>::max();
    uint32_t mMinSizeBytes = 0;
    uint32_t mMinCnt = 0;
    uint32_t mTimeoutSecs = 0;
};

template <class T = EventBatchStatus>
class EventFlushStrategy {
public:
    void SetMaxSizeBytes(uint32_t size) { mMaxSizeBytes = size; }
    void SetMinSizeBytes(uint32_t size) { mMinSizeBytes = size; }
    void SetMinCnt(uint32_t cnt) { mMinCnt = cnt; }
    void SetTimeoutSecs(uint32_t secs) { mTimeoutSecs = secs; }
    uint32_t GetMaxSizeBytes() const { return mMaxSizeBytes; }
    uint32_t GetMinSizeBytes() const { return mMinSizeBytes; }
    uint32_t GetMinCnt() const { return mMinCnt; }
    uint32_t GetTimeoutSecs() const { return mTimeoutSecs; }

    // should be called after event is added
    bool NeedFlushBySize(const T& status) { return status.GetSize() >= mMinSizeBytes; }
    bool NeedFlushByCnt(const T& status) { return status.GetCnt() == mMinCnt; }
    // should be called before event is added
    bool NeedFlushByTime(const T& status, const PipelineEventPtr& e) {
        return time(nullptr) - status.GetCreateTime() >= mTimeoutSecs;
    }
    bool SizeReachingUpperLimit(const T& status) { return status.GetSize() >= mMaxSizeBytes; }

private:
    uint32_t mMaxSizeBytes = 0;
    uint32_t mMinSizeBytes = 0;
    uint32_t mMinCnt = 0;
    uint32_t mTimeoutSecs = 0;
};

class GroupFlushStrategy {
public:
    GroupFlushStrategy(uint32_t size, uint32_t timeout) : mMinSizeBytes(size), mTimeoutSecs(timeout) {}

    void SetMinSizeBytes(uint32_t size) { mMinSizeBytes = size; }
    void SetTimeoutSecs(uint32_t secs) { mTimeoutSecs = secs; }
    uint32_t GetMinSizeBytes() const { return mMinSizeBytes; }
    uint32_t GetTimeoutSecs() const { return mTimeoutSecs; }

    // should be called after event is added
    bool NeedFlushBySize(const GroupBatchStatus& status) { return status.GetSize() >= mMinSizeBytes; }
    // should be called before event is added
    bool NeedFlushByTime(const GroupBatchStatus& status) {
        return time(nullptr) - status.GetCreateTime() >= mTimeoutSecs;
    }

private:
    uint32_t mMinSizeBytes = 0;
    uint32_t mTimeoutSecs = 0;
};

template <>
bool EventFlushStrategy<SLSEventBatchStatus>::NeedFlushByTime(const SLSEventBatchStatus& status,
                                                              const PipelineEventPtr& e);

} // namespace logtail
