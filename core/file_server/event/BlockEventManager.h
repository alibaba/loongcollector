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

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/FeedbackInterface.h"
#include "file_server/event/Event.h"
#include "pipeline/queue/QueueKey.h"

namespace logtail {

class BlockedEventManager : public FeedbackInterface {
public:
    BlockedEventManager(const BlockedEventManager&) = delete;
    BlockedEventManager& operator=(const BlockedEventManager&) = delete;

    static BlockedEventManager* GetInstance() {
        static BlockedEventManager instance;
        return &instance;
    }

    void Feedback(int64_t key) override;

    void UpdateBlockEvent(QueueKey logstoreKey,
                          const std::string& configName,
                          const Event& event,
                          const DevInode& devInode,
                          int32_t curTime);
    void GetTimeoutEvent(std::vector<Event*>& eventVec, int32_t curTime);
    void GetFeedbackEvent(std::vector<Event*>& eventVec);

private:
    struct BlockedEvent {
        QueueKey mQueueKey = -1;
        Event* mEvent = nullptr;
        int32_t mInvalidTime;
        int32_t mTimeout = 1;

        BlockedEvent();

        void Update(QueueKey key, Event* pEvent, int32_t curTime);
        void SetInvalidAgain();
    };

    BlockedEventManager() = default;
    ~BlockedEventManager();

    // only used by LogInput thread
    std::unordered_map<int64_t, BlockedEvent> mEventMap;

    // race condition from Processor Runner threads and LogInput thread
    std::mutex mFeedbackQueueMux;
    std::vector<int64_t> mFeedbackQueue;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ForceReadUnittest;
    friend class BlockedEventManagerUnittest;
#endif
};

} // namespace logtail
