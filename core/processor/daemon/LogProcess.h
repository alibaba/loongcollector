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

#include "common/Lock.h"
#include "common/LogRunnable.h"
#include "common/Thread.h"
#include "models/PipelineEventGroup.h"
#include "queue/FeedbackQueueKey.h"

namespace logtail {

class LogProcess : public LogRunnable {
public:
    static LogProcess* GetInstance() {
        static LogProcess* ptr = new LogProcess();
        return ptr;
    }

    void Start();
    void HoldOn();
    void Resume();
    bool FlushOut(int32_t waitMs);

    void* ProcessLoop(int32_t threadNo);
    // TODO: replace key with configName
    bool PushBuffer(QueueKey key, size_t inputIndex, PipelineEventGroup&& group, uint32_t retryTimes = 1);

private:
    LogProcess();
    ~LogProcess();

    bool Serialize(const PipelineEventGroup& group, bool enableNanosecond, std::string& res, std::string& errorMsg);

    bool mInitialized = false;
    ThreadPtr* mProcessThreads;
    int32_t mThreadCount = 1;
    std::atomic_bool* mThreadFlags;
    ReadWriteLock mAccessProcessThreadRWL;
};

} // namespace logtail