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

#include "BlockEventManager.h"
#include "processor/LogProcess.h"
#include "common/HashUtil.h"
#include "common/StringTools.h"
#include "polling/PollingEventQueue.h"

DEFINE_FLAG_INT32(max_block_event_timeout, "max block event timeout, seconds", 3);

namespace logtail {

void BlockedEventManager::UpdateBlockEvent(const LogstoreFeedBackKey& logstoreKey,
                                           const std::string& configName,
                                           const Event& event,
                                           const DevInode& devInode,
                                           int32_t curTime) {
    // need to create a new event
    Event* pEvent = new Event(event);
    int64_t hashKey = pEvent->GetHashKey();
    // set dev + inode + configName to prevent unnecessary feedback
    if (pEvent->GetDev() != devInode.dev || pEvent->GetInode() != devInode.inode
        || pEvent->GetConfigName() != configName) {
        pEvent->SetConfigName(configName);
        pEvent->SetDev(devInode.dev);
        pEvent->SetInode(devInode.inode);
        std::string key;
        key.append(pEvent->GetSource())
            .append(">")
            .append(pEvent->GetObject())
            .append(">")
            .append(ToString(pEvent->GetDev()))
            .append(">")
            .append(ToString(pEvent->GetInode()))
            .append(">")
            .append(pEvent->GetConfigName());
        hashKey = HashSignatureString(key.c_str(), key.size());
    }
    //LOG_DEBUG(sLogger, ("Add block event ", pEvent->GetSource())(pEvent->GetObject(), pEvent->GetInode())(pEvent->GetConfigName(), hashKey));
    ScopedSpinLock lock(mLock);
    mBlockEventMap[hashKey].Update(logstoreKey, pEvent, curTime);
}

void BlockedEventManager::GetTimeoutEvent(std::vector<Event*>& eventVec, int32_t curTime) {
    ScopedSpinLock lock(mLock);
    LogProcess* pProcess = LogProcess::GetInstance();
    for (std::unordered_map<int64_t, BlockedEvent>::iterator iter = mBlockEventMap.begin();
         iter != mBlockEventMap.end();) {
        BlockedEvent& blockedEvent = iter->second;
        if (blockedEvent.mEvent != NULL && blockedEvent.mInvalidTime + blockedEvent.mTimeout <= curTime) {
            if (pProcess->IsValidToReadLog(blockedEvent.mLogstoreKey)) {
                eventVec.push_back(blockedEvent.mEvent);
                //LOG_DEBUG(sLogger, ("Get timeout block event  ", blockedEvent.mEvent->GetSource())(blockedEvent.mEvent->GetObject(), blockedEvent.mEvent->GetConfigName()));
                iter = mBlockEventMap.erase(iter);
                continue;
            } else {
                blockedEvent.SetInvalidAgain(curTime);
            }
        }
        ++iter;
    }
}

void BlockedEventManager::FeedBack(const LogstoreFeedBackKey& key) {
    //LOG_DEBUG(sLogger, ("Get feedback block event  ", key));
    std::vector<Event*> eventVec;
    {
        ScopedSpinLock lock(mLock);
        for (std::unordered_map<int64_t, BlockedEvent>::iterator iter = mBlockEventMap.begin();
             iter != mBlockEventMap.end();) {
            BlockedEvent& blockedEvent = iter->second;
            if (blockedEvent.mEvent != NULL && blockedEvent.mLogstoreKey == key) {
                eventVec.push_back(blockedEvent.mEvent);
                //LOG_DEBUG(sLogger, ("Get feedback block event  ", blockedEvent.mEvent->GetSource())(blockedEvent.mEvent->GetObject(), blockedEvent.mEvent->GetConfigName()));
                iter = mBlockEventMap.erase(iter);
            } else {
                ++iter;
            }
        }
    }
    if (eventVec.size() > 0) {
        // use polling event queue, it is thread safe
        PollingEventQueue::GetInstance()->PushEvent(eventVec);
    }
}

BlockedEventManager::BlockedEventManager() {
}

BlockedEventManager::~BlockedEventManager() {
    for (std::unordered_map<int64_t, BlockedEvent>::iterator iter = mBlockEventMap.begin();
         iter != mBlockEventMap.end();
         ++iter) {
        if (iter->second.mEvent != NULL) {
            delete iter->second.mEvent;
        }
    }
}

} // namespace logtail
