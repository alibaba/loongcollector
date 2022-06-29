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
#include <string>
#include <unordered_map>
#include "common/LogstoreFeedbackQueue.h"
#include "common/Flags.h"
#include "common/Lock.h"
#include "Event.h"

DECLARE_FLAG_INT32(max_block_event_timeout);

namespace logtail {

class BlockedEventManager : public LogstoreFeedBackInterface {
public:
    static BlockedEventManager* GetInstance() {
        static BlockedEventManager* s_Instance = new BlockedEventManager;
        return s_Instance;
    }

    void UpdateBlockEvent(const LogstoreFeedBackKey& logstoreKey,
                          const std::string& configName,
                          const Event& event,
                          const DevInode& devInode,
                          int32_t curTime);
    void GetTimeoutEvent(std::vector<Event*>& eventVec, int32_t curTime);
    virtual void FeedBack(const LogstoreFeedBackKey& key);
    virtual bool IsValidToPush(const LogstoreFeedBackKey& key) {
        // should not be used
        return true;
    }

protected:
    struct BlockedEvent {
        BlockedEvent() : mLogstoreKey(0), mEvent(NULL), mInvalidTime(time(NULL)), mTimeout(1) {}
        void Update(const LogstoreFeedBackKey& logstoreKey, Event* pEvent, int32_t curTime) {
            if (mEvent != NULL) {
                delete mEvent;
            }
            mEvent = pEvent;
            mLogstoreKey = logstoreKey;
            mTimeout = (curTime - mInvalidTime) * 2 + 1;
            if (mTimeout > INT32_FLAG(max_block_event_timeout)) {
                mTimeout = INT32_FLAG(max_block_event_timeout);
            }
        }
        void SetInvalidAgain(int32_t curTime) {
            mTimeout *= 2;
            if (mTimeout > INT32_FLAG(max_block_event_timeout)) {
                mTimeout = INT32_FLAG(max_block_event_timeout);
            }
        }

        LogstoreFeedBackKey mLogstoreKey;
        Event* mEvent;
        int32_t mInvalidTime;
        int32_t mTimeout;
    };

    BlockedEventManager();
    virtual ~BlockedEventManager();

    std::unordered_map<int64_t, BlockedEvent> mBlockEventMap;
    SpinLock mLock;
};

} // namespace logtail
