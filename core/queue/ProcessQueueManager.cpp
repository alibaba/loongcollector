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

#include "queue/ProcessQueueManager.h"

#include "common/Flags.h"
#include "queue/ExactlyOnceQueueManager.h"
#include "queue/QueueKeyManager.h"
#include "queue/QueueParam.h"

DECLARE_FLAG_INT32(process_thread_count);

using namespace std;

namespace logtail {

ProcessQueueManager::ProcessQueueManager() {
    ResetCurrentQueueIndex();
}

bool ProcessQueueManager::CreateOrUpdateQueue(QueueKey key, uint32_t priority, QueueType type) {
    lock_guard<mutex> lock(mQueueMux);
    auto iter = mQueues.find(key);
    if (iter != mQueues.end()) {
        if (iter->second.second != type) {
            DeleteQueueEntity(iter->second.first);
            CreateQueue(key, priority, type);
        } else {
            if ((*iter->second.first)->GetPriority() == priority) {
                return false;
            }
            uint32_t oldPriority = (*iter->second.first)->GetPriority();
            auto queIter = iter->second.first;
            auto nextQueIter = next(queIter);
            mPriorityQueue[priority].splice(mPriorityQueue[priority].end(), mPriorityQueue[oldPriority], queIter);
            (*iter->second.first)->SetPriority(priority);
            if (mCurrentQueueIndex.first == oldPriority && mCurrentQueueIndex.second == queIter) {
                if (nextQueIter == mPriorityQueue[oldPriority].end()) {
                    mCurrentQueueIndex.second = mPriorityQueue[oldPriority].begin();
                } else {
                    mCurrentQueueIndex.second = nextQueIter;
                }
            }
        }
    } else {
        CreateQueue(key, priority, type);
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

void ProcessQueueManager::CreateQueue(QueueKey key, uint32_t priority, QueueType type) {
    switch (type) {
        case QueueType::BOUNDED:
            mPriorityQueue[priority].emplace_back(
                make_unique<BoundedProcessQueue>(ProcessQueueParam::GetInstance()->mCapacity,
                                                 ProcessQueueParam::GetInstance()->mLowWatermark,
                                                 ProcessQueueParam::GetInstance()->mHighWatermark,
                                                 key,
                                                 priority,
                                                 QueueKeyManager::GetInstance()->GetName(key)));
            break;
        case QueueType::CIRCULAR:
            // TODO: implement
            break;
    }
    mQueues[key] = make_pair(prev(mPriorityQueue[priority].end()), type);
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
