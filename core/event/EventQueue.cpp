// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "EventQueue.h"
#include "Event.h"
#include "common/StringTools.h"
#include "common/Flags.h"
#include "common/TimeUtil.h"
#include "logger/Logger.h"

DECLARE_FLAG_INT32(max_polling_event_queue_size);

namespace logtail {

void EventQueue::PushEvent(const std::vector<Event*>& eventVec) {
    for (size_t i = 0; i < eventVec.size(); ++i) {
        Event* pEvent = eventVec[i];
        LOG_DEBUG(sLogger,
                  ("Polling Event", ToString((int)pEvent->GetType()))(pEvent->GetSource(), pEvent->GetObject())(
                      ToString(pEvent->GetDev()), ToString(pEvent->GetInode())));
    }
    int32_t tryTime = 0;
    do {
        {
            PTScopedLock lock(mQueueLock);
            if (mEventQueue.size() < (size_t)INT32_FLAG(max_polling_event_queue_size)) {
                mEventQueue.insert(mEventQueue.end(), eventVec.begin(), eventVec.end());
                break;
            }
        }
        usleep(10 * 1000);
        if (++tryTime == 1000) {
            LOG_ERROR(sLogger, ("Push polling event blocked, drop event", eventVec.size()));
            for (size_t i = 0; i < eventVec.size(); ++i) {
                Event* pEvent = eventVec[i];
                LOG_INFO(
                    sLogger,
                    ("Drop polling Event", ToString((int)pEvent->GetType()))(pEvent->GetSource(), pEvent->GetObject())(
                        ToString(pEvent->GetDev()), ToString(pEvent->GetInode())));
                delete pEvent;
            }
            break;
        }
    } while (true);
}


void EventQueue::PopAllEvents(std::vector<Event*>& allEvents) {
    PTScopedLock lock(mQueueLock);
    allEvents.insert(allEvents.end(), mEventQueue.begin(), mEventQueue.end());
    mEventQueue.clear();
}


EventQueue::EventQueue() {
}


EventQueue::~EventQueue() {
}

void EventQueue::PushEvent(Event* pEvent) {
    LOG_DEBUG(sLogger,
              ("Polling Event", ToString((int)pEvent->GetType()))(pEvent->GetSource(), pEvent->GetObject())(
                  ToString(pEvent->GetDev()), ToString(pEvent->GetInode())));
    int32_t tryTime = 0;
    do {
        {
            PTScopedLock lock(mQueueLock);
            if (mEventQueue.size() < (size_t)INT32_FLAG(max_polling_event_queue_size)) {
                mEventQueue.push_back(pEvent);
                break;
            }
        }
        usleep(10000);
        if (++tryTime == 1000) {
            // drop event
            LOG_ERROR(sLogger,
                      ("Push polling event blocked, drop event", pEvent->GetSource() + "/" + pEvent->GetObject()));
            delete pEvent;
            break;
        }
    } while (true);
}

#ifdef APSARA_UNIT_TEST_MAIN
void EventQueue::Clear() {
    PTScopedLock lock(mQueueLock);
    for (std::deque<Event*>::iterator iter = mEventQueue.begin(); iter != mEventQueue.end(); ++iter) {
        delete *iter;
    }
    mEventQueue.clear();
}

Event* EventQueue::FindEvent(const std::string& src, const std::string& obj) {
    PTScopedLock lock(mQueueLock);
    for (std::deque<Event*>::iterator iter = mEventQueue.begin(); iter != mEventQueue.end(); ++iter) {
        Event* pEvt = *iter;
        if (pEvt->GetSource() == src && pEvt->GetObject() == obj) {
            return pEvt;
        }
    }
    return NULL;
}

Event* EventQueue::FindEvent(const std::string& src, const std::string& obj, uint64_t dev, uint64_t inode) {
    PTScopedLock lock(mQueueLock);
    for (std::deque<Event*>::iterator iter = mEventQueue.begin(); iter != mEventQueue.end(); ++iter) {
        Event* pEvt = *iter;
        if (pEvt->GetSource() == src && pEvt->GetObject() == obj && pEvt->GetDev() == dev
            && pEvt->GetInode() == inode) {
            return pEvt;
        }
    }
    return NULL;
}
#endif

} // namespace logtail
