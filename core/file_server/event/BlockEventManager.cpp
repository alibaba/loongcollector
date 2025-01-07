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

#include "file_server/event/BlockEventManager.h"

#include "common/Flags.h"
#include "common/HashUtil.h"
#include "common/StringTools.h"
#include "file_server/event_handler/LogInput.h"
#include "logger/Logger.h"
#include "pipeline/queue/ProcessQueueManager.h"

DEFINE_FLAG_INT32(max_block_event_timeout, "max block event timeout, seconds", 3);

using namespace std;

namespace logtail {

BlockedEventManager::BlockedEvent::BlockedEvent() : mInvalidTime(time(NULL)) {
}

void BlockedEventManager::BlockedEvent::Update(QueueKey key, Event* pEvent, int32_t curTime) {
    if (mEvent != NULL) {
        // There are only two situations where event coverage is possible
        // 1. the new event is not timeout event
        // 2. old event is timeout event
        if (!pEvent->IsReaderFlushTimeout() || mEvent->IsReaderFlushTimeout()) {
            delete mEvent;
        } else {
            return;
        }
    }
    mEvent = pEvent;
    mQueueKey = key;
    // will become traditional block event if processor queue is not ready
    if (mEvent->IsReaderFlushTimeout()) {
        mTimeout = curTime - mInvalidTime;
    } else {
        mTimeout = (curTime - mInvalidTime) * 2 + 1;
        if (mTimeout > INT32_FLAG(max_block_event_timeout)) {
            mTimeout = INT32_FLAG(max_block_event_timeout);
        }
    }
}

void BlockedEventManager::BlockedEvent::SetInvalidAgain() {
    mTimeout *= 2;
    if (mTimeout > INT32_FLAG(max_block_event_timeout)) {
        mTimeout = INT32_FLAG(max_block_event_timeout);
    }
}

BlockedEventManager::~BlockedEventManager() {
    for (auto iter = mEventMap.begin(); iter != mEventMap.end(); ++iter) {
        if (iter->second.mEvent != nullptr) {
            delete iter->second.mEvent;
        }
    }
}

void BlockedEventManager::Feedback(int64_t key) {
    {
        lock_guard<mutex> lock(mFeedbackQueueMux);
        mFeedbackQueue.emplace_back(key);
    }
    LogInput::GetInstance()->Trigger();
}

void BlockedEventManager::UpdateBlockEvent(
    QueueKey logstoreKey, const string& configName, const Event& event, const DevInode& devInode, int32_t curTime) {
    // need to create a new event
    Event* pEvent = new Event(event);
    int64_t hashKey = pEvent->GetHashKey();
    // set dev + inode + configName to prevent unnecessary feedback
    if (pEvent->GetDev() != devInode.dev || pEvent->GetInode() != devInode.inode
        || pEvent->GetConfigName() != configName) {
        pEvent->SetConfigName(configName);
        pEvent->SetDev(devInode.dev);
        pEvent->SetInode(devInode.inode);
        string key;
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
    LOG_DEBUG(sLogger,
              ("Add block event ", pEvent->GetSource())(pEvent->GetObject(),
                                                        pEvent->GetInode())(pEvent->GetConfigName(), hashKey));
    mEventMap[hashKey].Update(logstoreKey, pEvent, curTime);
}

void BlockedEventManager::GetTimeoutEvent(vector<Event*>& res, int32_t curTime) {
    for (auto iter = mEventMap.begin(); iter != mEventMap.end();) {
        auto& e = iter->second;
        if (e.mEvent != nullptr && e.mInvalidTime + e.mTimeout <= curTime) {
            if (ProcessQueueManager::GetInstance()->IsValidToPush(e.mQueueKey)) {
                res.push_back(e.mEvent);
                iter = mEventMap.erase(iter);
                continue;
            } else {
                e.SetInvalidAgain();
            }
        }
        ++iter;
    }
}

void BlockedEventManager::GetFeedbackEvent(vector<Event*>& res) {
    vector<int64_t> keys;
    {
        lock_guard<mutex> lock(mFeedbackQueueMux);
        keys.swap(mFeedbackQueue);
    }
    for (auto& key : keys) {
        for (auto iter = mEventMap.begin(); iter != mEventMap.end();) {
            auto& e = iter->second;
            if (e.mEvent != nullptr && e.mQueueKey == key) {
                res.push_back(e.mEvent);
                iter = mEventMap.erase(iter);
            } else {
                ++iter;
            }
        }
    }
}

} // namespace logtail
