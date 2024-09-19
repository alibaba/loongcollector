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

#include <cstdint>
#include <ctime>

#include "pipeline/batch/BatchedEvents.h"
#include "models/PipelineEventPtr.h"

namespace logtail {

class EventBatchStatus {
public:
    virtual ~EventBatchStatus() = default;
    
    virtual void Reset() {
        mCnt = 0;
        mSizeBytes = 0;
        mCreateTime = 0;
    }

    virtual void Update(const PipelineEventPtr& e) {
        if (mCreateTime == 0) {
            mCreateTime = time(nullptr);
        }
        mSizeBytes += e->DataSize();
        ++mCnt;
    }

    uint32_t GetCnt() const { return mCnt; }
    uint32_t GetSize() const { return mSizeBytes; }
    time_t GetCreateTime() const { return mCreateTime; }

protected:
    uint32_t mCnt = 0;
    uint32_t mSizeBytes = 0;
    time_t mCreateTime = 0;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class EventFlushStrategyUnittest;
#endif
};

class GroupBatchStatus {
public:
    void Reset() {
        mSizeBytes = 0;
        mCreateTime = 0;
    }

    void Update(const BatchedEvents& g) {
        if (mCreateTime == 0) {
            mCreateTime = time(nullptr);
        }
        mSizeBytes += g.mSizeBytes;
    }

    uint32_t GetSize() const { return mSizeBytes; }
    time_t GetCreateTime() const { return mCreateTime; }

private:
    uint32_t mSizeBytes = 0;
    time_t mCreateTime = 0;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class GroupFlushStrategyUnittest;
#endif
};

class SLSEventBatchStatus : public EventBatchStatus {
public:
    void Reset() override {
        EventBatchStatus::Reset();
        mCreateTimeMinute = 0;
    }

    void Update(const PipelineEventPtr& e) override {
        if (mCreateTime == 0) {
            mCreateTime = time(nullptr);
            mCreateTimeMinute = e->GetTimestamp() / 60;
        }
        mSizeBytes += e->DataSize();
        ++mCnt;
    }

    time_t GetCreateTimeMinute() const { return mCreateTimeMinute; }

private:
    time_t mCreateTimeMinute = 0;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SLSEventFlushStrategyUnittest;
#endif
};

} // namespace logtail
