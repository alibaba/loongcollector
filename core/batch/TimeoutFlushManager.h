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
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "plugin/interface/Flusher.h"

namespace logtail {

struct TimeoutRecord {
    Flusher* mFlusher = nullptr;
    size_t mKey;
    time_t mUpdateTime = 0;
    uint32_t mTimeoutSecs = 0;

    TimeoutRecord(Flusher* flusher, size_t key, uint32_t timeoutSecs)
        : mFlusher(flusher), mKey(key), mUpdateTime(time(nullptr)), mTimeoutSecs(timeoutSecs) {}

    void Update() { mUpdateTime = time(nullptr); }
};

class TimeoutFlushManager {
public:
    TimeoutFlushManager(const TimeoutFlushManager&) = delete;
    TimeoutFlushManager& operator=(const TimeoutFlushManager&) = delete;

    static TimeoutFlushManager* GetInstance() {
        static TimeoutFlushManager instance;
        return &instance;
    }

    void UpdateRecord(const std::string& config, size_t index, size_t key, uint32_t timeoutSecs, Flusher* f);
    void FlushTimeoutBatch();
    void ClearRecords(const std::string& config);

private:
    TimeoutFlushManager() = default;
    ~TimeoutFlushManager() = default;

    std::mutex mMux;
    std::map<std::string, std::map<std::pair<size_t, size_t>, TimeoutRecord>> mTimeoutRecords;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class TimeoutFlushManagerUnittest;
    friend class BatcherUnittest;
#endif
};

} // namespace logtail
