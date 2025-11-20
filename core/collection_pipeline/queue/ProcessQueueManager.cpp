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

#include "collection_pipeline/queue/ProcessQueueManager.h"

#include "CountBoundedProcessQueue.h"
#include "collection_pipeline/queue/BytesBoundedProcessQueue.h"
#include "collection_pipeline/queue/CircularProcessQueue.h"
#include "collection_pipeline/queue/CountBoundedProcessQueue.h"
#include "collection_pipeline/queue/ExactlyOnceQueueManager.h"
#include "collection_pipeline/queue/QueueKeyManager.h"
#include "common/Flags.h"

// For one queue, only one of the following two flags will be used.
DEFINE_FLAG_INT32(bounded_process_queue_cnt_capacity, "", 5);
DEFINE_FLAG_INT32(bounded_process_queue_bytes_capacity, "", 5 * 128 * 1024);

DECLARE_FLAG_INT32(process_thread_count);

using namespace std;

namespace logtail {

ProcessQueueManager::ProcessQueueManager()
    : mCountBoundedQueueParam(INT32_FLAG(bounded_process_queue_cnt_capacity)),
      mBytesBoundedQueueParam(INT32_FLAG(bounded_process_queue_bytes_capacity)) {
    ResetCurrentQueueIndex();
}

bool ProcessQueueManager::CreateOrUpdateCountBoundedQueue(QueueKey key,
                                                          uint32_t priority,
                                                          const CollectionPipelineContext& ctx) {
    lock_guard<mutex> lock(mQueueMux);
    auto iter = mQueues.find(key);
    if (iter != mQueues.end()) {
        if (iter->second.second != QueueType::COUNT_BOUNDED) {
            // queue type change only happen when all input plugin types are changed. in such case, old input data not
            // been processed can be discarded since whole pipeline is actually changed.
            DeleteQueueEntity(iter->second.first);
            CreateCountBoundedQueue(key, priority, ctx);
        } else {
            if ((*iter->second.first)->GetPriority() == priority) {
                return false;
            }
            AdjustQueuePriority(iter->second.first, priority);
        }
    } else {
        CreateCountBoundedQueue(key, priority, ctx);
    }
    if (mCurrentQueueIndex.second == mPriorityQueue[mCurrentQueueIndex.first].end()) {
        mCurrentQueueIndex.second = mPriorityQueue[mCurrentQueueIndex.first].begin();
    }
    return true;
}

bool ProcessQueueManager::CreateOrUpdateCircularQueue(QueueKey key,
                                                      uint32_t priority,
                                                      size_t capacity,
                                                      const CollectionPipelineContext& ctx) {
    lock_guard<mutex> lock(mQueueMux);
    auto iter = mQueues.find(key);
    if (iter != mQueues.end()) {
        if (iter->second.second != QueueType::CIRCULAR) {
            // queue type change only happen when all input plugin types are changed. in such case, old input data not
            // been processed can be discarded since whole pipeline is actually changed.
            DeleteQueueEntity(iter->second.first);
            CreateCircularQueue(key, priority, capacity, ctx);
        } else {
            static_cast<CircularProcessQueue*>(iter->second.first->get())->Reset(capacity);
            if ((*iter->second.first)->GetPriority() == priority) {
                return false;
            }
            AdjustQueuePriority(iter->second.first, priority);
        }
    } else {
        CreateCircularQueue(key, priority, capacity, ctx);
    }
    if (mCurrentQueueIndex.second == mPriorityQueue[mCurrentQueueIndex.first].end()) {
        mCurrentQueueIndex.second = mPriorityQueue[mCurrentQueueIndex.first].begin();
    }
    return true;
}

bool ProcessQueueManager::CreateOrUpdateBytesBoundedQueue(QueueKey key,
                                                          uint32_t priority,
                                                          const CollectionPipelineContext& ctx) {
    lock_guard<mutex> lock(mQueueMux);
    auto iter = mQueues.find(key);
    if (iter != mQueues.end()) {
        if (iter->second.second != QueueType::BYTES_BOUNDED) {
            // queue type change only happen when all input plugin types are changed. in such case, old input data not
            // been processed can be discarded since whole pipeline is actually changed.
            DeleteQueueEntity(iter->second.first);
            CreateBytesBoundedQueue(key, priority, ctx);
        } else {
            if ((*iter->second.first)->GetPriority() == priority) {
                return false;
            }
            AdjustQueuePriority(iter->second.first, priority);
        }
    } else {
        CreateBytesBoundedQueue(key, priority, ctx);
    }
    if (mCurrentQueueIndex.second == mPriorityQueue[mCurrentQueueIndex.first].end()) {
        mCurrentQueueIndex.second = mPriorityQueue[mCurrentQueueIndex.first].begin();
    }
    return true;
}

bool ProcessQueueManager::DeleteQueue(QueueKey key) {
    lock_guard<mutex> lock(mQueueMux);
    auto iter = mQueues.find(key);
    if (iter == mQueues.end()) {
        return false;
    }
    DeleteQueueEntity(iter->second.first);
    QueueKeyManager::GetInstance()->RemoveKey(iter->first);
    mQueues.erase(iter);
    return true;
}

bool ProcessQueueManager::IsValidToPush(QueueKey key) const {
    lock_guard<mutex> lock(mQueueMux);
    auto iter = mQueues.find(key);
    if (iter != mQueues.end()) {
        if (iter->second.second == QueueType::COUNT_BOUNDED) {
            return static_cast<CountBoundedProcessQueue*>(iter->second.first->get())->IsValidToPush();
        }
        if (iter->second.second == QueueType::BYTES_BOUNDED) {
            return static_cast<BytesBoundedProcessQueue*>(iter->second.first->get())->IsValidToPush();
        }
        return true;
    }
    return ExactlyOnceQueueManager::GetInstance()->IsValidToPushProcessQueue(key);
}

QueueStatus ProcessQueueManager::PushQueue(QueueKey key, unique_ptr<ProcessQueueItem>&& item) {
    {
        lock_guard<mutex> lock(mQueueMux);
        auto iter = mQueues.find(key);
        if (iter != mQueues.end()) {
            if (!(*iter->second.first)->Push(std::move(item))) {
                return QueueStatus::QUEUE_FULL;
            }
        } else {
            auto res = ExactlyOnceQueueManager::GetInstance()->PushProcessQueue(key, std::move(item));
            if (res != QueueStatus::OK) {
                return res;
            }
        }
    }
    Trigger();
    return QueueStatus::OK;
}

bool ProcessQueueManager::PopItem(int64_t threadNo, unique_ptr<ProcessQueueItem>& item, string& configName) {
    configName.clear();
    lock_guard<mutex> lock(mQueueMux);
    for (size_t i = 0; i <= sMaxPriority; ++i) {
        ProcessQueueIterator iter;
        if (mCurrentQueueIndex.first == i) {
            for (iter = mCurrentQueueIndex.second; iter != mPriorityQueue[i].end(); ++iter) {
                if (!(*iter)->Pop(item)) {
                    continue;
                }
                configName = (*iter)->GetConfigName();
                break;
            }
            if (configName.empty()) {
                for (iter = mPriorityQueue[i].begin(); iter != mCurrentQueueIndex.second; ++iter) {
                    if (!(*iter)->Pop(item)) {
                        continue;
                    }
                    configName = (*iter)->GetConfigName();
                    break;
                }
            }
        } else {
            for (iter = mPriorityQueue[i].begin(); iter != mPriorityQueue[i].end(); ++iter) {
                if (!(*iter)->Pop(item)) {
                    continue;
                }
                configName = (*iter)->GetConfigName();
                break;
            }
        }
        if (!configName.empty()) {
            mCurrentQueueIndex.first = i;
            mCurrentQueueIndex.second = ++iter;
            if (mCurrentQueueIndex.second == mPriorityQueue[i].end()) {
                mCurrentQueueIndex.second = mPriorityQueue[i].begin();
            }
            return true;
        }
        // find exactly once queues next
        {
            lock_guard<mutex> lock(ExactlyOnceQueueManager::GetInstance()->mProcessQueueMux);
            for (auto iter = ExactlyOnceQueueManager::GetInstance()->mProcessPriorityQueue[i].begin();
                 iter != ExactlyOnceQueueManager::GetInstance()->mProcessPriorityQueue[i].end();
                 ++iter) {
                // process queue for exactly once can only be assgined to one specific thread
                if (iter->GetKey() % INT32_FLAG(process_thread_count) != threadNo) {
                    continue;
                }
                if (!iter->Pop(item)) {
                    continue;
                }
                configName = iter->GetConfigName();
                ResetCurrentQueueIndex();
                return true;
            }
        }
    }
    ResetCurrentQueueIndex();
    {
        unique_lock<mutex> lock(mStateMux);
        mValidToPop = false;
    }
    return false;
}

bool ProcessQueueManager::IsAllQueueEmpty() const {
    {
        lock_guard<mutex> lock(mQueueMux);
        for (const auto& q : mQueues) {
            if (!(*q.second.first)->Empty()) {
                return false;
            }
        }
    }
    return ExactlyOnceQueueManager::GetInstance()->IsAllProcessQueueEmpty();
}

bool ProcessQueueManager::SetDownStreamQueues(QueueKey key, vector<BoundedSenderQueueInterface*>&& ques) {
    lock_guard<mutex> lock(mQueueMux);
    auto iter = mQueues.find(key);
    if (iter == mQueues.end()) {
        return false;
    }
    (*iter->second.first)->SetDownStreamQueues(std::move(ques));
    return true;
}

bool ProcessQueueManager::SetFeedbackInterface(QueueKey key, vector<FeedbackInterface*>&& feedback) {
    lock_guard<mutex> lock(mQueueMux);
    auto iter = mQueues.find(key);
    if (iter == mQueues.end()) {
        return false;
    }
    if (iter->second.second == QueueType::CIRCULAR) {
        return false;
    }
    if (iter->second.second == QueueType::COUNT_BOUNDED) {
        static_cast<CountBoundedProcessQueue*>(iter->second.first->get())->SetUpStreamFeedbacks(std::move(feedback));
    } else if (iter->second.second == QueueType::BYTES_BOUNDED) {
        static_cast<BytesBoundedProcessQueue*>(iter->second.first->get())->SetUpStreamFeedbacks(std::move(feedback));
    }
    return true;
}

void ProcessQueueManager::DisablePop(const string& configName, bool isPipelineRemoving) {
    if (QueueKeyManager::GetInstance()->HasKey(configName)) {
        auto key = QueueKeyManager::GetInstance()->GetKey(configName);
        lock_guard<mutex> lock(mQueueMux);
        auto iter = mQueues.find(key);
        if (iter != mQueues.end()) {
            (*iter->second.first)->DisablePop();
        }
    } else {
        ExactlyOnceQueueManager::GetInstance()->DisablePopProcessQueue(configName, isPipelineRemoving);
    }
}

void ProcessQueueManager::EnablePop(const string& configName) {
    if (QueueKeyManager::GetInstance()->HasKey(configName)) {
        auto key = QueueKeyManager::GetInstance()->GetKey(configName);
        lock_guard<mutex> lock(mQueueMux);
        auto iter = mQueues.find(key);
        if (iter != mQueues.end()) {
            (*iter->second.first)->EnablePop();
        }
    } else {
        ExactlyOnceQueueManager::GetInstance()->EnablePopProcessQueue(configName);
    }
}

bool ProcessQueueManager::Wait(uint64_t ms) {
    // TODO: use semaphore instead
    unique_lock<mutex> lock(mStateMux);
    mCond.wait_for(lock, chrono::milliseconds(ms), [this] { return mValidToPop; });
    if (mValidToPop) {
        mValidToPop = false;
        return true;
    }
    return false;
}

void ProcessQueueManager::Trigger() {
    {
        lock_guard<mutex> lock(mStateMux);
        mValidToPop = true;
    }
    mCond.notify_one();
}

void ProcessQueueManager::CreateCountBoundedQueue(QueueKey key,
                                                  uint32_t priority,
                                                  const CollectionPipelineContext& ctx) {
    mPriorityQueue[priority].emplace_back(
        make_unique<CountBoundedProcessQueue>(mCountBoundedQueueParam.GetCapacity(),
                                              mCountBoundedQueueParam.GetLowWatermark(),
                                              mCountBoundedQueueParam.GetHighWatermark(),
                                              key,
                                              priority,
                                              ctx));
    mQueues[key] = make_pair(prev(mPriorityQueue[priority].end()), QueueType::COUNT_BOUNDED);
}

void ProcessQueueManager::CreateCircularQueue(QueueKey key,
                                              uint32_t priority,
                                              size_t capacity,
                                              const CollectionPipelineContext& ctx) {
    mPriorityQueue[priority].emplace_back(make_unique<CircularProcessQueue>(capacity, key, priority, ctx));
    mQueues[key] = make_pair(prev(mPriorityQueue[priority].end()), QueueType::CIRCULAR);
}

void ProcessQueueManager::CreateBytesBoundedQueue(QueueKey key,
                                                  uint32_t priority,
                                                  const CollectionPipelineContext& ctx) {
    mPriorityQueue[priority].emplace_back(
        make_unique<BytesBoundedProcessQueue>(mBytesBoundedQueueParam.GetCapacity(),
                                              mBytesBoundedQueueParam.GetLowWatermark(),
                                              mBytesBoundedQueueParam.GetHighWatermark(),
                                              key,
                                              priority,
                                              ctx));
    mQueues[key] = make_pair(prev(mPriorityQueue[priority].end()), QueueType::BYTES_BOUNDED);
}

void ProcessQueueManager::AdjustQueuePriority(const ProcessQueueIterator& iter, uint32_t priority) {
    uint32_t oldPriority = (*iter)->GetPriority();
    auto nextQueIter = next(iter);
    mPriorityQueue[priority].splice(mPriorityQueue[priority].end(), mPriorityQueue[oldPriority], iter);
    (*iter)->SetPriority(priority);
    if (mCurrentQueueIndex.first == oldPriority && mCurrentQueueIndex.second == iter) {
        if (nextQueIter == mPriorityQueue[oldPriority].end()) {
            mCurrentQueueIndex.second = mPriorityQueue[oldPriority].begin();
        } else {
            mCurrentQueueIndex.second = nextQueIter;
        }
    }
}

void ProcessQueueManager::DeleteQueueEntity(const ProcessQueueIterator& iter) {
    uint32_t priority = (*iter)->GetPriority();
    auto nextQueIter = mPriorityQueue[priority].erase(iter);
    if (mCurrentQueueIndex.first == priority && mCurrentQueueIndex.second == iter) {
        if (nextQueIter == mPriorityQueue[priority].end()) {
            mCurrentQueueIndex.second = mPriorityQueue[priority].begin();
        } else {
            mCurrentQueueIndex.second = nextQueIter;
        }
    }
}

void ProcessQueueManager::ResetCurrentQueueIndex() {
    mCurrentQueueIndex.first = 0;
    mCurrentQueueIndex.second = mPriorityQueue[0].begin();
}

#ifdef APSARA_UNIT_TEST_MAIN
void ProcessQueueManager::Clear() {
    lock_guard<mutex> lock(mQueueMux);
    mQueues.clear();
    for (size_t i = 0; i <= sMaxPriority; ++i) {
        mPriorityQueue[i].clear();
    }
    ResetCurrentQueueIndex();
}
#endif

} // namespace logtail
