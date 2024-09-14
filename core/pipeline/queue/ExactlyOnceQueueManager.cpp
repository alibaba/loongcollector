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

#include "pipeline/queue/ExactlyOnceQueueManager.h"

#include "common/Flags.h"
#include "common/TimeUtil.h"
#include "plugin/input/InputFeedbackInterfaceRegistry.h"
#include "plugin/input/InputFile.h"
#include "logger/Logger.h"
#include "pipeline/queue/ProcessQueueManager.h"
#include "pipeline/queue/QueueKeyManager.h"

DEFINE_FLAG_INT32(logtail_queue_gc_threshold_sec, "2min", 2 * 60);
DEFINE_FLAG_INT64(logtail_queue_max_used_time_per_round_in_msec, "500ms", 500);

DECLARE_FLAG_INT32(bounded_process_queue_capacity);

using namespace std;

namespace logtail {

ExactlyOnceQueueManager::ExactlyOnceQueueManager() : mProcessQueueParam(INT32_FLAG(bounded_process_queue_capacity)) {
}

bool ExactlyOnceQueueManager::CreateOrUpdateQueue(QueueKey key,
                                                  uint32_t priority,
                                                  const PipelineContext& ctx,
                                                  const vector<RangeCheckpointPtr>& checkpoints) {
    {
        lock_guard<mutex> lock(mGCMux);
        mQueueDeletionTimeMap.erase(key);
    }
    vector<BoundedSenderQueueInterface*> senderQueue;
    {
        lock_guard<mutex> lock(mSenderQueueMux);
        auto iter = mSenderQueues.find(key);
        if (iter != mSenderQueues.end()) {
            iter->second.Reset(checkpoints);
        } else {
            mSenderQueues.try_emplace(key, checkpoints, key, ctx);
            iter = mSenderQueues.find(key);
        }
        // limiters are set on first push to the queue
        senderQueue.emplace_back(&iter->second);
    }
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
            iter->second->SetConfigName(ctx.GetConfigName());
            // note: do not reset process queue, to be the same as original implementation
        } else {
            // note: Ideally, queue capacity should be the same as checkpoint size. However, since process queue cannot
            // be reset during update, we temporarily use common param. If checkpoints size is larger than common
            // capacity, performance will restricted.
            mProcessPriorityQueue[priority].emplace_back(mProcessQueueParam.GetCapacity(),
                                                         mProcessQueueParam.GetLowWatermark(),
                                                         mProcessQueueParam.GetHighWatermark(),
                                                         key,
                                                         priority,
                                                         ctx);
            // mProcessPriorityQueue[priority].emplace_back(
            //     checkpoints.size(), checkpoints.size() - 1, checkpoints.size(), key, priority, config);
            mProcessQueues[key] = prev(mProcessPriorityQueue[priority].end());
        }
        // for exactly once, the feedback is one to one
        mProcessQueues[key]->SetDownStreamQueues(std::move(senderQueue));
        // exactly once can only be applied to input_file
        vector<FeedbackInterface*> feedbacks{
            InputFeedbackInterfaceRegistry::GetInstance()->GetFeedbackInterface(InputFile::sName)};
        mProcessQueues[key]->SetUpStreamFeedbacks(std::move(feedbacks));
    }
    return true;
}

bool ExactlyOnceQueueManager::DeleteQueue(QueueKey key) {
    bool isProcessQueueExisted = false, isSenderQueueExisted = false;
    {
        lock_guard<mutex> lock(mProcessQueueMux);
        isProcessQueueExisted = mProcessQueues.find(key) != mProcessQueues.end();
    }
    {
        lock_guard<mutex> lock(mSenderQueueMux);
        isSenderQueueExisted = mSenderQueues.find(key) != mSenderQueues.end();
    }
    if (!isProcessQueueExisted && !isSenderQueueExisted) {
        return false;
    }
    if (!isProcessQueueExisted || !isSenderQueueExisted) {
        // should not happen
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

void ExactlyOnceQueueManager::InvalidatePopProcessQueue(const string& configName) {
    lock_guard<mutex> lock(mProcessQueueMux);
    for (auto& iter : mProcessQueues) {
        if (iter.second->GetConfigName() == configName) {
            iter.second->InvalidatePop();
        }
    }
}

void ExactlyOnceQueueManager::ValidatePopProcessQueue(const string& configName) {
    lock_guard<mutex> lock(mProcessQueueMux);
    for (auto& iter : mProcessQueues) {
        if (iter.second->GetConfigName() == configName) {
            iter.second->ValidatePop();
        }
    }
}

int ExactlyOnceQueueManager::PushSenderQueue(QueueKey key, unique_ptr<SenderQueueItem>&& item) {
    lock_guard<mutex> lock(mSenderQueueMux);
    auto iter = mSenderQueues.find(key);
    if (iter == mSenderQueues.end()) {
        return 2;
    }
    if (!iter->second.Push(std::move(item))) {
        return 1;
    }
    return 0;
}

void ExactlyOnceQueueManager::GetAllAvailableSenderQueueItems(std::vector<SenderQueueItem*>& item, bool withLimits) {
    lock_guard<mutex> lock(mSenderQueueMux);
    for (auto iter = mSenderQueues.begin(); iter != mSenderQueues.end(); ++iter) {
        iter->second.GetAllAvailableItems(item, withLimits);
    }
}

bool ExactlyOnceQueueManager::RemoveSenderQueueItem(QueueKey key, SenderQueueItem* item) {
    lock_guard<mutex> lock(mSenderQueueMux);
    auto iter = mSenderQueues.find(key);
    if (iter == mSenderQueues.end()) {
        return false;
    }
    return iter->second.Remove(item);
}

bool ExactlyOnceQueueManager::IsAllSenderQueueEmpty() const {
    lock_guard<mutex> lock(mSenderQueueMux);
    for (const auto& q : mSenderQueues) {
        if (!q.second.Empty()) {
            return false;
        }
    }
    return true;
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
            auto itr = mProcessQueues.find(iter->first);
            if (itr == mProcessQueues.end()) {
                // should not happen
                continue;
            }
            if (!itr->second->Empty()) {
                ++iter;
                continue;
            }
        }
        {
            lock_guard<mutex> lock(mSenderQueueMux);
            auto itr = mSenderQueues.find(iter->first);
            if (itr == mSenderQueues.end()) {
                // should not happen
                continue;
            }
            if (!itr->second.Empty()) {
                ++iter;
                continue;
            }
        }
        {
            lock_guard<mutex> lock(mProcessQueueMux);
            auto queueItr = mProcessQueues.find(iter->first);
            mProcessPriorityQueue[queueItr->second->GetPriority()].erase(queueItr->second);
            mProcessQueues.erase(queueItr);
        }
        {
            lock_guard<mutex> lock(mSenderQueueMux);
            mSenderQueues.erase(iter->first);
        }
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
    {
        lock_guard<mutex> lock(mProcessQueueMux);
        mProcessQueues.clear();
        for (size_t i = 0; i <= ProcessQueueManager::sMaxPriority; ++i) {
            mProcessPriorityQueue[i].clear();
        }
    }
    {
        lock_guard<mutex> lock(mSenderQueueMux);
        mSenderQueues.clear();
    }
}
#endif

} // namespace logtail
