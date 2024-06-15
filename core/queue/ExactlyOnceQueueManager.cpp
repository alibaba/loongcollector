// Copyright 2024 iLogtail Authors
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

#include "queue/ExactlyOnceQueueManager.h"

#include "common/Flags.h"
#include "common/TimeUtil.h"
#include "input/InputFeedbackInterfaceRegistry.h"
#include "input/InputFile.h"
#include "logger/Logger.h"
#include "queue/ProcessQueueManager.h"
#include "queue/QueueKeyManager.h"
#include "sender/Sender.h"

DEFINE_FLAG_INT32(logtail_queue_gc_threshold_sec, "2min", 2 * 60);
DEFINE_FLAG_INT64(logtail_queue_max_used_time_per_round_in_msec, "500ms", 500);

using namespace std;

namespace logtail {

bool ExactlyOnceQueueManager::CreateOrUpdateQueue(QueueKey key,
                                                  uint32_t priority,
                                                  const string& config,
                                                  const vector<RangeCheckpointPtr>& checkpoints) {
    {
        lock_guard<mutex> lock(mGCMux);
        mQueueDeletionTimeMap.erase(key);
    }
    Sender::Instance()->GetQueue().ConvertToExactlyOnceQueue(key, checkpoints);
    {
        lock_guard<mutex> lock(mProcessQueueMux);
        auto iter = mProcessQueues.find(key);
        if (iter != mProcessQueues.end()) {
            if (iter->second->GetPriority() != priority) {
                mProcessPriorityQueue[priority].splice(mProcessPriorityQueue[priority].end(),
                                                       mProcessPriorityQueue[iter->second->GetPriority()],
                                                       iter->second);
                iter->second->SetPriority(priority);
            }
        } else {
            mProcessPriorityQueue[priority].emplace_back(
                checkpoints.size(), 0, checkpoints.size(), key, priority, config);
            mProcessQueues[key] = prev(mProcessPriorityQueue[priority].end());
        }
        // for exactly once, the feedback is one to one
        vector<SingleLogstoreSenderManager<SenderQueueParam>*> senderqueue{Sender::Instance()->GetSenderQueue(key)};
        mProcessQueues[key]->SetDownStreamQueues(senderqueue);

        // exactly once can only be applied to input_file
        vector<FeedbackInterface*> feedbacks{
            InputFeedbackInterfaceRegistry::GetInstance()->GetFeedbackInterface(InputFile::sName)};
        mProcessQueues[key]->SetUpStreamFeedbacks(feedbacks);
    }
    return true;
}

bool ExactlyOnceQueueManager::DeleteQueue(QueueKey key) {
    {
        lock_guard<mutex> lock(mProcessQueueMux);
        if (mProcessQueues.find(key) == mProcessQueues.end()) {
            return false;
        }
    }
    {
        lock_guard<mutex> lock(mGCMux);
        if (mQueueDeletionTimeMap.find(key) != mQueueDeletionTimeMap.end()) {
            return false;
        }
        mQueueDeletionTimeMap[key] = time(nullptr);
    }
    return true;
}

bool ExactlyOnceQueueManager::IsValidToPushProcessQueue(QueueKey key) const {
    lock_guard<mutex> lock(mProcessQueueMux);
    auto iter = mProcessQueues.find(key);
    if (iter == mProcessQueues.end()) {
        return false;
    }
    return iter->second->IsValidToPush();
}

int ExactlyOnceQueueManager::PushProcessQueue(QueueKey key, unique_ptr<ProcessQueueItem>&& item) {
    lock_guard<mutex> lock(mProcessQueueMux);
    auto iter = mProcessQueues.find(key);
    if (iter == mProcessQueues.end()) {
        return 2;
    }
    if (!iter->second->Push(std::move(item))) {
        return 1;
    }
    return 0;
}

bool ExactlyOnceQueueManager::IsAllProcessQueueEmpty() const {
    lock_guard<mutex> lock(mProcessQueueMux);
    for (const auto& q : mProcessQueues) {
        if (!q.second->Empty()) {
            return false;
        }
    }
    return true;
}

void ExactlyOnceQueueManager::InvalidatePop(const string& configName) {
    lock_guard<mutex> lock(mProcessQueueMux);
    for (auto& iter : mProcessQueues) {
        if (iter.second->GetConfigName() == configName) {
            iter.second->InvalidatePop();
        }
    }
}

void ExactlyOnceQueueManager::ValidatePop(const string& configName) {
    lock_guard<mutex> lock(mProcessQueueMux);
    for (auto& iter : mProcessQueues) {
        if (iter.second->GetConfigName() == configName) {
            iter.second->ValidatePop();
        }
    }
}

void ExactlyOnceQueueManager::ClearTimeoutQueues() {
    auto const curTime = time(nullptr);
    const auto startTimeMs = GetCurrentTimeInMilliSeconds();
    lock_guard<mutex> lock(mGCMux);
    auto iter = mQueueDeletionTimeMap.begin();
    while (iter != mQueueDeletionTimeMap.end()) {
        if (GetCurrentTimeInMilliSeconds() - startTimeMs
            >= static_cast<uint64_t>(INT64_FLAG(logtail_queue_max_used_time_per_round_in_msec))) {
            break;
        }
        if (!(curTime >= iter->second && curTime - iter->second >= INT32_FLAG(logtail_queue_gc_threshold_sec))) {
            ++iter;
            continue;
        }
        {
            lock_guard<mutex> lock(mProcessQueueMux);
            if (!mProcessQueues[iter->first]->Empty()) {
                ++iter;
                continue;
            }
        }
        if (!Sender::Instance()->GetQueue().IsEmpty(iter->first)) {
            ++iter;
            continue;
        }
        {
            lock_guard<mutex> lock(mProcessQueueMux);
            auto queueItr = mProcessQueues.find(iter->first);
            mProcessPriorityQueue[queueItr->second->GetPriority()].erase(queueItr->second);
            mProcessQueues.erase(queueItr);
        }
        Sender::Instance()->GetQueue().Delete(iter->first);
        QueueKeyManager::GetInstance()->RemoveKey(iter->first);
        iter = mQueueDeletionTimeMap.erase(iter);
    }
}

uint32_t ExactlyOnceQueueManager::GetInvalidProcessQueueCnt() const {
    uint32_t res = 0;
    lock_guard<mutex> lock(mProcessQueueMux);
    for (const auto& q : mProcessQueues) {
        if (q.second->IsValidToPush()) {
            ++res;
        }
    }
    return res;
}

uint32_t ExactlyOnceQueueManager::GetProcessQueueCnt() const {
    lock_guard<mutex> lock(mProcessQueueMux);
    return mProcessQueues.size();
}

#ifdef APSARA_UNIT_TEST_MAIN
void ExactlyOnceQueueManager::Clear() {
    lock_guard<mutex> lock(mProcessQueueMux);
    mProcessQueues.clear();
    for (size_t i = 0; i <= ProcessQueueManager::sMaxPriority; ++i) {
        mProcessPriorityQueue[i].clear();
    }
}
#endif

} // namespace logtail
