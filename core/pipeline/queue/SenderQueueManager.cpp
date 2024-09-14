// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "pipeline/queue/SenderQueueManager.h"

#include "common/Flags.h"
#include "pipeline/queue/ExactlyOnceQueueManager.h"
#include "pipeline/queue/QueueKeyManager.h"

DEFINE_FLAG_INT32(sender_queue_gc_threshold_sec, "30s", 30);
DEFINE_FLAG_INT32(sender_queue_capacity, "", 10);

using namespace std;

namespace logtail {

SenderQueueManager::SenderQueueManager() : mQueueParam(INT32_FLAG(sender_queue_capacity)) {
}

bool SenderQueueManager::CreateQueue(QueueKey key,
                                     const string& flusherId,
                                     const PipelineContext& ctx,
                                     vector<shared_ptr<ConcurrencyLimiter>>&& concurrencyLimiters,
                                     uint32_t maxRate) {
    lock_guard<mutex> lock(mQueueMux);
    auto iter = mQueues.find(key);
    if (iter == mQueues.end()) {
        mQueues.try_emplace(key,
                            mQueueParam.GetCapacity(),
                            mQueueParam.GetLowWatermark(),
                            mQueueParam.GetHighWatermark(),
                            key,
                            flusherId,
                            ctx);
        iter = mQueues.find(key);
    }
    iter->second.SetConcurrencyLimiters(std::move(concurrencyLimiters));
    iter->second.SetRateLimiter(maxRate);
    return true;
}

SenderQueue* SenderQueueManager::GetQueue(QueueKey key) {
    lock_guard<mutex> lock(mQueueMux);
    auto iter = mQueues.find(key);
    if (iter != mQueues.end()) {
        return &iter->second;
    }
    return nullptr;
}

bool SenderQueueManager::DeleteQueue(QueueKey key) {
    {
        lock_guard<mutex> lock(mQueueMux);
        auto iter = mQueues.find(key);
        if (iter == mQueues.end()) {
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

bool SenderQueueManager::ReuseQueue(QueueKey key) {
    lock_guard<mutex> lock(mGCMux);
    auto iter = mQueueDeletionTimeMap.find(key);
    if (iter == mQueueDeletionTimeMap.end()) {
        return false;
    }
    mQueueDeletionTimeMap.erase(iter);
    return true;
}

int SenderQueueManager::PushQueue(QueueKey key, unique_ptr<SenderQueueItem>&& item) {
    {
        lock_guard<mutex> lock(mQueueMux);
        auto iter = mQueues.find(key);
        if (iter != mQueues.end()) {
            if (!iter->second.Push(std::move(item))) {
                return 1;
            }
        } else {
            int res = ExactlyOnceQueueManager::GetInstance()->PushSenderQueue(key, std::move(item));
            if (res != 0) {
                return res;
            }
        }
    }
    Trigger();
    return 0;
}

void SenderQueueManager::GetAllAvailableItems(vector<SenderQueueItem*>& items, bool withLimits) {
    {
        lock_guard<mutex> lock(mQueueMux);
        for (auto iter = mQueues.begin(); iter != mQueues.end(); ++iter) {
            iter->second.GetAllAvailableItems(items, withLimits);
        }
    }
    ExactlyOnceQueueManager::GetInstance()->GetAllAvailableSenderQueueItems(items, withLimits);
}

bool SenderQueueManager::RemoveItem(QueueKey key, SenderQueueItem* item) {
    {
        lock_guard<mutex> lock(mQueueMux);
        auto iter = mQueues.find(key);
        if (iter != mQueues.end()) {
            return iter->second.Remove(item);
        }
    }
    return ExactlyOnceQueueManager::GetInstance()->RemoveSenderQueueItem(key, item);
}

bool SenderQueueManager::IsAllQueueEmpty() const {
    {
        lock_guard<mutex> lock(mQueueMux);
        for (const auto& q : mQueues) {
            if (!q.second.Empty()) {
                return false;
            }
        }
    }
    return ExactlyOnceQueueManager::GetInstance()->IsAllSenderQueueEmpty();
}

void SenderQueueManager::ClearUnusedQueues() {
    auto const curTime = time(nullptr);
    lock_guard<mutex> lock(mGCMux);
    auto iter = mQueueDeletionTimeMap.begin();
    while (iter != mQueueDeletionTimeMap.end()) {
        if (!(curTime >= iter->second && curTime - iter->second >= INT32_FLAG(sender_queue_gc_threshold_sec))) {
            ++iter;
            continue;
        }
        {
            lock_guard<mutex> lock(mQueueMux);
            auto itr = mQueues.find(iter->first);
            if (itr == mQueues.end()) {
                // should not happen
                continue;
            }
            if (!itr->second.Empty()) {
                ++iter;
                continue;
            }
            mQueues.erase(itr);
        }
        QueueKeyManager::GetInstance()->RemoveKey(iter->first);
        iter = mQueueDeletionTimeMap.erase(iter);
    }
}

bool SenderQueueManager::IsValidToPush(QueueKey key) const {
    lock_guard<mutex> lock(mQueueMux);
    auto iter = mQueues.find(key);
    if (iter != mQueues.end()) {
        return iter->second.IsValidToPush();
    }
    // no need to check exactly once queue, since the caller does not support exactly once
    // should not happen
    return false;
}

bool SenderQueueManager::Wait(uint64_t ms) {
    // TODO: use semaphore instead
    unique_lock<mutex> lock(mStateMux);
    mCond.wait_for(lock, chrono::milliseconds(ms), [this] { return mValidToPop; });
    if (mValidToPop) {
        mValidToPop = false;
        return true;
    }
    return false;
}

void SenderQueueManager::Trigger() {
    {
        lock_guard<mutex> lock(mStateMux);
        mValidToPop = true;
    }
    mCond.notify_one();
}

#ifdef APSARA_UNIT_TEST_MAIN
void SenderQueueManager::Clear() {
    lock_guard<mutex> lock(mQueueMux);
    mQueues.clear();
    mQueueDeletionTimeMap.clear();
}

bool SenderQueueManager::IsQueueMarkedDeleted(QueueKey key) {
    lock_guard<mutex> lock(mGCMux);
    return mQueueDeletionTimeMap.find(key) != mQueueDeletionTimeMap.end();
}
#endif

} // namespace logtail
