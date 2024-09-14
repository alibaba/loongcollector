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

#include "pipeline/queue/ProcessQueueManager.h"

#include "common/Flags.h"
#include "pipeline/queue/BoundedProcessQueue.h"
#include "pipeline/queue/CircularProcessQueue.h"
#include "pipeline/queue/ExactlyOnceQueueManager.h"
#include "pipeline/queue/QueueKeyManager.h"

DEFINE_FLAG_INT32(bounded_process_queue_capacity, "", 15);

DECLARE_FLAG_INT32(process_thread_count);

using namespace std;

namespace logtail {

ProcessQueueManager::ProcessQueueManager() : mBoundedQueueParam(INT32_FLAG(bounded_process_queue_capacity)) {
    ResetCurrentQueueIndex();
}

bool ProcessQueueManager::CreateOrUpdateBoundedQueue(QueueKey key, uint32_t priority, const PipelineContext& ctx) {
    lock_guard<mutex> lock(mQueueMux);
    auto iter = mQueues.find(key);
    if (iter != mQueues.end()) {
        if (iter->second.second != QueueType::BOUNDED) {
            // queue type change only happen when all input plugin types are changed. in such case, old input data not
            // been processed can be discarded since whole pipeline is actually changed.
            DeleteQueueEntity(iter->second.first);
            CreateBoundedQueue(key, priority, ctx);
        } else {
            if ((*iter->second.first)->GetPriority() == priority) {
                return false;
            }
            AdjustQueuePriority(iter->second.first, priority);
        }
    } else {
        CreateBoundedQueue(key, priority, ctx);
    }
    if (mCurrentQueueIndex.second == mPriorityQueue[mCurrentQueueIndex.first].end()) {
        mCurrentQueueIndex.second = mPriorityQueue[mCurrentQueueIndex.first].begin();
    }
    return true;
}

bool ProcessQueueManager::CreateOrUpdateCircularQueue(QueueKey key,
                                                      uint32_t priority,
                                                      size_t capacity,
                                                      const PipelineContext& ctx) {
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
        if (iter->second.second == QueueType::BOUNDED) {
            return static_cast<BoundedProcessQueue*>(iter->second.first->get())->IsValidToPush();
        } else {
            return true;
        }
    }
    return ExactlyOnceQueueManager::GetInstance()->IsValidToPushProcessQueue(key);
}

int ProcessQueueManager::PushQueue(QueueKey key, unique_ptr<ProcessQueueItem>&& item) {
    {
        lock_guard<mutex> lock(mQueueMux);
        auto iter = mQueues.find(key);
        if (iter != mQueues.end()) {
            if (!(*iter->second.first)->Push(std::move(item))) {
                return 1;
            }
        } else {
            int res = ExactlyOnceQueueManager::GetInstance()->PushProcessQueue(key, std::move(item));
            if (res != 0) {
                return res;
            }
        }
    }
    Trigger();
    return 0;
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
    static_cast<BoundedProcessQueue*>(iter->second.first->get())->SetUpStreamFeedbacks(std::move(feedback));
    return true;
}

void ProcessQueueManager::InvalidatePop(const string& configName) {
    if (QueueKeyManager::GetInstance()->HasKey(configName)) {
        auto key = QueueKeyManager::GetInstance()->GetKey(configName);
        lock_guard<mutex> lock(mQueueMux);
        auto iter = mQueues.find(key);
        if (iter != mQueues.end()) {
            (*iter->second.first)->InvalidatePop();
        }
    } else {
        ExactlyOnceQueueManager::GetInstance()->InvalidatePopProcessQueue(configName);
    }
}

void ProcessQueueManager::ValidatePop(const string& configName) {
    if (QueueKeyManager::GetInstance()->HasKey(configName)) {
        auto key = QueueKeyManager::GetInstance()->GetKey(configName);
        lock_guard<mutex> lock(mQueueMux);
        auto iter = mQueues.find(key);
        if (iter != mQueues.end()) {
            (*iter->second.first)->ValidatePop();
        }
    } else {
        ExactlyOnceQueueManager::GetInstance()->ValidatePopProcessQueue(configName);
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

void ProcessQueueManager::CreateBoundedQueue(QueueKey key, uint32_t priority, const PipelineContext& ctx) {
    mPriorityQueue[priority].emplace_back(make_unique<BoundedProcessQueue>(mBoundedQueueParam.GetCapacity(),
                                                                           mBoundedQueueParam.GetLowWatermark(),
                                                                           mBoundedQueueParam.GetHighWatermark(),
                                                                           key,
                                                                           priority,
                                                                           ctx));
    mQueues[key] = make_pair(prev(mPriorityQueue[priority].end()), QueueType::BOUNDED);
}

void ProcessQueueManager::CreateCircularQueue(QueueKey key,
                                              uint32_t priority,
                                              size_t capacity,
                                              const PipelineContext& ctx) {
    mPriorityQueue[priority].emplace_back(make_unique<CircularProcessQueue>(capacity, key, priority, ctx));
    mQueues[key] = make_pair(prev(mPriorityQueue[priority].end()), QueueType::CIRCULAR);
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

uint32_t ProcessQueueManager::GetInvalidCnt() const {
    uint32_t res = 0;
    lock_guard<mutex> lock(mQueueMux);
    for (const auto& q : mQueues) {
        if (q.second.second == QueueType::BOUNDED
            && static_cast<BoundedProcessQueue*>(q.second.first->get())->IsValidToPush()) {
            ++res;
        }
    }
    return res;
}

uint32_t ProcessQueueManager::GetCnt() const {
    lock_guard<mutex> lock(mQueueMux);
    return mQueues.size();
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
