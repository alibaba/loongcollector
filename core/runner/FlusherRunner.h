/*
 * Copyright 2022 iLogtail Authors
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

#include <atomic>
#include <cstdint>
#include <future>

#include "monitor/LogtailMetric.h"
#include "pipeline/plugin/interface/Flusher.h"
#include "pipeline/queue/SenderQueueItem.h"
#include "runner/sink/SinkType.h"

namespace logtail {

class FlusherRunner {
public:
    FlusherRunner(const FlusherRunner&) = delete;
    FlusherRunner& operator=(const FlusherRunner&) = delete;

    static FlusherRunner* GetInstance() {
        static FlusherRunner instance;
        return &instance;
    }

    bool Init();
    void Stop();

    void DecreaseHttpSendingCnt();

    // TODO: should be private
    void PushToHttpSink(SenderQueueItem* item, bool withLimit = true);

    int32_t GetSendingBufferCount() { return mHttpSendingCnt; }

private:
    FlusherRunner() = default;
    ~FlusherRunner() = default;

    void Run();
    void Dispatch(SenderQueueItem* item);

    std::future<void> mThreadRes;
    std::atomic_bool mIsFlush = false;

    std::atomic_int mHttpSendingCnt{0};

    // TODO: temporarily here
    int32_t mLastCheckSendClientTime = 0;
    int64_t mSendLastTime = 0;
    int32_t mSendLastByte = 0;

    mutable MetricsRecordRef mMetricsRecordRef;
    CounterPtr mInItemsTotal;
    CounterPtr mInItemDataSizeBytes;
    CounterPtr mInItemRawDataSizeBytes;
    CounterPtr mOutItemsTotal;
    CounterPtr mTotalDelayMs;
    IntGaugePtr mWaitingItemsTotal;
    IntGaugePtr mLastRunTime;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PluginRegistryUnittest;
    friend class FlusherRunnerUnittest;
#endif
};

} // namespace logtail
