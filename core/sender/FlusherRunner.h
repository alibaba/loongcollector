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
#include <unordered_map>

#include "plugin/interface/Flusher.h"
#include "queue/SenderQueueItem.h"
#include "sink/SinkType.h"

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

    void IncreaseSendingCnt();
    void DecreaseSendingCnt();

    void PushToHttpSink(SenderQueueItem* item);

    void RegisterSink(const std::string& flusher, SinkType type);

    int32_t GetSendingBufferCount() { return mSendingBufferCount; }
    int32_t GetLastDeamonRunTime() { return mLastDaemonRunTime; }
    int32_t GetLastSendTime() { return mLastSendDataTime; }

    void RestLastSenderTime() {
        mLastDaemonRunTime = 0;
        mLastSendDataTime = 0;
    }

private:
    FlusherRunner() = default;
    ~FlusherRunner() = default;

    void Run();
    void Dispatch(SenderQueueItem* item);

    std::future<void> mThreadRes;
    std::atomic_bool mIsFlush = false;

    std::unordered_map<std::string, SinkType> mFlusherSinkMap;

    std::atomic_int mSendingBufferCount{0};

    std::atomic_int mLastDaemonRunTime{0};
    std::atomic_int mLastSendDataTime{0};

    int32_t mLastCheckSendClientTime = 0;
    int64_t mSendLastTime = 0;
    int32_t mSendLastByte = 0;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class PluginRegistryUnittest;
    friend class FlusherRunnerUnittest;
#endif
};

} // namespace logtail
