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
#include <vector>
#include <deque>
#include <mutex>

namespace logtail {

class Event;

class PollingEventQueue {
public:
    static PollingEventQueue* GetInstance() {
        static PollingEventQueue* ptr = new PollingEventQueue();
        return ptr;
    }

    void PushEvent(const std::vector<Event*>& eventVec);
    void PushEvent(Event* pEvent);

    void PopAllEvents(std::vector<Event*>& allEvents);

private:
    PollingEventQueue();
    ~PollingEventQueue();

    std::mutex mQueueLock;
    std::deque<Event*> mEventQueue;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class EventDispatcher;
    friend class EventDispatcherBase;
    friend class PollingUnittest;

    void Clear();
    Event* FindEvent(const std::string& src, const std::string& obj, int32_t eventType = -1);
    Event* FindEvent(const std::string& src, const std::string& obj, uint64_t dev, uint64_t inode);
#endif
};

} // namespace logtail
