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
#include <unordered_map>
#include <string>
#include <stdlib.h>
#include <vector>
#include "WaitObject.h"
#include "MemoryBarrier.h"
#include "Lock.h"
#include "QueueManager.h"

#define MAX_CONFIG_PRIORITY_LEVEL (3)

namespace logtail {

class LogstoreFeedBackInterface {
public:
    LogstoreFeedBackInterface() {}
    virtual ~LogstoreFeedBackInterface() {}

    virtual void FeedBack(const LogstoreFeedBackKey& key) = 0;

    virtual bool IsValidToPush(const LogstoreFeedBackKey& key) = 0;
};

class TriggerEvent {
public:
    TriggerEvent() : mTriggerState(false) {}

    bool Wait(int32_t waitMs) {
        WaitObject::Lock lock(mWaitObj);
        ReadWriteBarrier();
        if (mTriggerState) {
            mTriggerState = false;
            return true;
        }
        mWaitObj.wait(lock, (int64_t)waitMs * (int64_t)1000);
        if (mTriggerState) {
            mTriggerState = false;
            return true;
        }
        return false;
    }

    void Trigger() {
        WaitObject::Lock lock(mWaitObj);
        {
            ReadWriteBarrier();
            mTriggerState = true;
        }
        mWaitObj.signal();
    }

protected:
    WaitObject mWaitObj;
    volatile bool mTriggerState;
};

template <class TT, class PARAM>
class SingleLogstoreFeedbackQueue {
public:
    SingleLogstoreFeedbackQueue() : mArray(NULL), mType(QueueType::Normal) {
        ResetParam(PARAM::GetInstance()->GetLowSize(),
                   PARAM::GetInstance()->GetHighSize(),
                   PARAM::GetInstance()->GetMaxSize());
    }

    ~SingleLogstoreFeedbackQueue() { free(mArray); }

    SingleLogstoreFeedbackQueue(const SingleLogstoreFeedbackQueue& que) = delete;
    SingleLogstoreFeedbackQueue& operator=(const SingleLogstoreFeedbackQueue&) = delete;

    void SetType(QueueType type) { mType = type; }

    void ConvertToExactlyOnceQueue(const size_t lowSize, const size_t highSize, const size_t maxSize) {
        SetType(QueueType::ExactlyOnce);
        ResetParam(lowSize, highSize, maxSize);
    }

    void ResetParam(const size_t lowSize, const size_t highSize, const size_t maxSize) {
        LOW_SIZE = lowSize;
        HIGH_SIZE = highSize;
        SIZE = maxSize;

        if (mArray != NULL) {
            free(mArray);
        }
        mArray = (TT*)malloc(SIZE * sizeof(TT));
        memset(mArray, 0, SIZE * sizeof(TT));
        Reset();
    }

    void Reset() {
        mValid = true;
        mWrite = mRead = mSize = 0;
    }

    bool IsValid() const { return mValid; }

    bool IsEmpty() const { return mSize == 0; }

    bool IsFull() const { return mSize == SIZE; }

    bool PushItem(const TT& item) {
        if (IsFull()) {
            return false;
        }

        mArray[mWrite % SIZE] = item;
        ++mWrite;
        ++mSize;
        if (mSize == HIGH_SIZE) {
            mValid = false;
        }
        return true;
    }

    // Pop an item from queue.
    //
    // @return:
    // - 0: the queue is empty.
    // - 1: item is poped and the queue's status is unchanged.
    // - 2: item is poped and the queue becomes valid.
    int PopItem(TT& item) {
        if (IsEmpty()) {
            return 0;
        }

        int rst = 1;
        item = mArray[mRead % SIZE];
        ++mRead;
        if (--mSize == LOW_SIZE && !mValid) {
            mValid = true;
            rst = 2;
        }
        return rst;
    }

    size_t GetSize() const { return mSize; }

    QueueType GetQueueType() const { return mType; }

protected:
    size_t LOW_SIZE;
    size_t HIGH_SIZE;
    size_t SIZE;

    TT* mArray;
    volatile bool mValid;
    volatile uint64_t mWrite;
    volatile uint64_t mRead;
    volatile size_t mSize;
    volatile QueueType mType;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class QueueManagerUnittest;
#endif
};

class LogstoreFeedbackQueueParam {
public:
    LogstoreFeedbackQueueParam() : SIZE(100), LOW_SIZE(10), HIGH_SIZE(20) {}

    static LogstoreFeedbackQueueParam* GetInstance() {
        static LogstoreFeedbackQueueParam* sQueueParam = new LogstoreFeedbackQueueParam;
        return sQueueParam;
    }

    size_t GetLowSize() { return LOW_SIZE; }

    size_t GetHighSize() { return HIGH_SIZE; }

    size_t GetMaxSize() { return SIZE; }

    void SetLowSize(size_t lowSize) { LOW_SIZE = lowSize; }

    void SetHighSize(size_t highSize) { HIGH_SIZE = highSize; }

    void SetMaxSize(size_t maxSize) { SIZE = maxSize; }

    size_t SIZE;
    size_t LOW_SIZE;
    size_t HIGH_SIZE;
};

template <class T, class PARAM = LogstoreFeedbackQueueParam>
class LogstoreFeedbackQueue : public LogstoreFeedBackInterface {
protected:
    typedef SingleLogstoreFeedbackQueue<T, PARAM> SingleLogStoreQueue;
    typedef std::unordered_map<LogstoreFeedBackKey, SingleLogStoreQueue> LogstoreFeedBackQueueMap;
    typedef typename std::unordered_map<LogstoreFeedBackKey, SingleLogStoreQueue>::iterator
        LogstoreFeedBackQueueMapIterator;

    struct SingleLogStorePriorityQueue {
        SingleLogStoreQueue* mQueue;
        LogstoreFeedBackKey mKey;
        int32_t mPriority;

        SingleLogStorePriorityQueue() {}

        SingleLogStorePriorityQueue(int32_t priority, const LogstoreFeedBackKey& key, SingleLogStoreQueue* queue)
            : mQueue(queue), mKey(key), mPriority(priority) {}
    };

    typedef std::vector<SingleLogStorePriorityQueue> LogstoreFeedBackQueueVector;
    typedef typename std::vector<SingleLogStorePriorityQueue>::iterator LogstoreFeedBackQueueVectorIterator;

public:
    LogstoreFeedbackQueue() : mFeedBackObj(NULL) {}

    void SetParam(const size_t lowSize, const size_t highSize, const size_t maxSize) {
        PARAM* pParam = PARAM::GetInstance();
        pParam->SetLowSize(lowSize);
        pParam->SetHighSize(highSize);
        pParam->SetMaxSize(maxSize);
    }

    void SetFeedBackObject(LogstoreFeedBackInterface* pFeedbackObj) {
        PTScopedLock dataLock(mLock);
        mFeedBackObj = pFeedbackObj;
    }

    bool Wait(int32_t waitMs) { return mTrigger.Wait(waitMs); }

    void Signal() { mTrigger.Trigger(); }

    bool IsValid(const LogstoreFeedBackKey& key) {
        PTScopedLock dataLock(mLock);
        SingleLogStoreQueue& singleQueue = mLogstoreQueueMap[key];
        return singleQueue.IsValid();
    }

    bool PushItem(const LogstoreFeedBackKey& key, const T& item) {
        {
            PTScopedLock dataLock(mLock);
            SingleLogStoreQueue& singleQueue = mLogstoreQueueMap[key];

            if (!singleQueue.PushItem(item)) {
                return false;
            }
        }
        mTrigger.Trigger();
        return true;
    }

    bool PopItem(LogstoreFeedBackKey& startKey, T& item) {
        PTScopedLock dataLock(mLock);
        for (size_t i = 0; i < MAX_CONFIG_PRIORITY_LEVEL; ++i) {
            for (LogstoreFeedBackQueueVectorIterator iter = mPriorityQueueArray[i].begin();
                 iter != mPriorityQueueArray[i].end();
                 ++iter) {
                int rst = iter->mQueue->PopItem(item);
                if (rst == 0) {
                    continue;
                }
                if (rst == 2 && mFeedBackObj != NULL) {
                    mFeedBackObj->FeedBack(iter->mKey);
                }
                return true;
            }
        }

        LogstoreFeedBackQueueMapIterator startKeyIter = mLogstoreQueueMap.find(startKey);
        for (LogstoreFeedBackQueueMapIterator iter = startKeyIter; iter != mLogstoreQueueMap.end(); ++iter) {
            int rst = iter->second.PopItem(item);
            if (rst == 0) {
                continue;
            }
            startKey = iter->first;
            if (rst == 2 && mFeedBackObj != NULL) {
                mFeedBackObj->FeedBack(startKey);
            }
            return true;
        }
        for (LogstoreFeedBackQueueMapIterator iter = mLogstoreQueueMap.begin(); iter != startKeyIter; ++iter) {
            int rst = iter->second.PopItem(item);
            if (rst == 0) {
                continue;
            }
            startKey = iter->first;
            if (rst == 2 && mFeedBackObj != NULL) {
                mFeedBackObj->FeedBack(startKey);
            }
            return true;
        }
        return false;
    }

    bool CheckAndPopItem(LogstoreFeedBackKey& startKey, T& item, LogstoreFeedBackInterface* pCheckObj) {
        if (pCheckObj == NULL) {
            return false;
        }
        PTScopedLock dataLock(mLock);
        for (size_t i = 0; i < MAX_CONFIG_PRIORITY_LEVEL; ++i) {
            for (LogstoreFeedBackQueueVectorIterator iter = mPriorityQueueArray[i].begin();
                 iter != mPriorityQueueArray[i].end();
                 ++iter) {
                if (!pCheckObj->IsValidToPush(iter->mKey)) {
                    continue;
                }
                int rst = iter->mQueue->PopItem(item);
                if (rst == 0) {
                    continue;
                }
                if (rst == 2 && mFeedBackObj != NULL) {
                    mFeedBackObj->FeedBack(iter->mKey);
                }
                return true;
            }
        }
        LogstoreFeedBackQueueMapIterator startKeyIter = mLogstoreQueueMap.find(startKey);
        for (LogstoreFeedBackQueueMapIterator iter = startKeyIter; iter != mLogstoreQueueMap.end(); ++iter) {
            if (!pCheckObj->IsValidToPush(iter->first)) {
                continue;
            }
            int rst = iter->second.PopItem(item);
            if (rst == 0) {
                continue;
            }
            startKey = iter->first;
            if (rst == 2 && mFeedBackObj != NULL) {
                mFeedBackObj->FeedBack(startKey);
            }
            return true;
        }
        for (LogstoreFeedBackQueueMapIterator iter = mLogstoreQueueMap.begin(); iter != startKeyIter; ++iter) {
            if (!pCheckObj->IsValidToPush(iter->first)) {
                continue;
            }
            int rst = iter->second.PopItem(item);
            if (rst == 0) {
                continue;
            }
            startKey = iter->first;
            if (rst == 2 && mFeedBackObj != NULL) {
                mFeedBackObj->FeedBack(startKey);
            }
            return true;
        }
        return false;
    }

    bool CheckAndPopNextItem(LogstoreFeedBackKey& startKey,
                             T& item,
                             LogstoreFeedBackInterface* pCheckObj,
                             int32_t threadNo,
                             int32_t threadNum) {
        if (pCheckObj == NULL) {
            return false;
        }

        int rst = 0;
        do {
            PTScopedLock dataLock(mLock);
            if (mLogstoreQueueMap.empty()) {
                return false;
            }

            // Iterate queues by priority at first.
            for (size_t i = 0; i < MAX_CONFIG_PRIORITY_LEVEL; ++i) {
                for (LogstoreFeedBackQueueVectorIterator iter = mPriorityQueueArray[i].begin();
                     iter != mPriorityQueueArray[i].end();
                     ++iter) {
                    if (!CanPopItem(threadNo, threadNum, pCheckObj, iter->mKey, *(iter->mQueue))) {
                        continue;
                    }

                    rst = iter->mQueue->PopItem(item);
                    if (rst == 0) {
                        continue;
                    }
                    if (rst == 2 && mFeedBackObj != NULL) {
                        mFeedBackObj->FeedBack(iter->mKey);
                    }
                    return true;
                }
            }

            // Iterate queues, startKey is used for fairness.
            LogstoreFeedBackQueueMapIterator startKeyIter = mLogstoreQueueMap.find(startKey);
            if (startKeyIter == mLogstoreQueueMap.end()) {
                startKeyIter = mLogstoreQueueMap.begin();
            } else {
                ++startKeyIter;
            }

            // Range: the key of last poped item -> end.
            for (auto iter = startKeyIter; iter != mLogstoreQueueMap.end(); ++iter) {
                if (!CanPopItem(threadNo, threadNum, pCheckObj, iter->first, iter->second)) {
                    continue;
                }

                rst = iter->second.PopItem(item);
                if (rst == 0) {
                    continue;
                }
                startKey = iter->first;
                break;
            }
            if (rst != 0) {
                break;
            }

            // Range: begin -> the key of last poped item.
            for (auto iter = mLogstoreQueueMap.begin(); iter != startKeyIter; ++iter) {
                if (!CanPopItem(threadNo, threadNum, pCheckObj, iter->first, iter->second)) {
                    continue;
                }

                rst = iter->second.PopItem(item);
                if (rst == 0) {
                    continue;
                }
                startKey = iter->first;
                break;
            }
        } while (false);

        if (rst == 2 && mFeedBackObj != NULL) {
            mFeedBackObj->FeedBack(startKey);
        }
        return rst != 0;
    }

    bool IsEmpty() {
        PTScopedLock dataLock(mLock);
        for (LogstoreFeedBackQueueMapIterator iter = mLogstoreQueueMap.begin(); iter != mLogstoreQueueMap.end();
             ++iter) {
            if (!iter->second.IsEmpty()) {
                return false;
            }
        }
        return true;
    }

    bool IsEmpty(const LogstoreFeedBackKey& key) {
        PTScopedLock dataLock(mLock);
        auto iter = mLogstoreQueueMap.find(key);
        return iter == mLogstoreQueueMap.end() || iter->second.IsEmpty();
    }

    void Lock() { mLock.lock(); }

    void Unlock() { mLock.unlock(); }

    virtual void FeedBack(const LogstoreFeedBackKey& key) { mTrigger.Trigger(); }

    virtual bool IsValidToPush(const LogstoreFeedBackKey& key) {
        PTScopedLock dataLock(mLock);
        auto& singleQueue = mLogstoreQueueMap[key];
        return singleQueue.IsValid();
    }

    void
    GetStatus(int32_t& normalInvalidCount, int32_t& normalTotalCount, int32_t& eoInvalidCount, int32_t& eoTotalCount) {
        PTScopedLock dataLock(mLock);
        for (LogstoreFeedBackQueueMapIterator iter = mLogstoreQueueMap.begin(); iter != mLogstoreQueueMap.end();
             ++iter) {
            bool isExactlyOnceQueue = iter->second.GetQueueType() == QueueType::ExactlyOnce;
            auto& invalid = isExactlyOnceQueue ? eoInvalidCount : normalInvalidCount;
            auto& total = isExactlyOnceQueue ? eoTotalCount : normalTotalCount;

            ++total;
            if (!iter->second.IsValid()) {
                ++invalid;
            }
        }
    }

    void Delete(const LogstoreFeedBackKey& key) {
        PTScopedLock dataLock(mLock);
        auto iter = mLogstoreQueueMap.find(key);
        if (iter != mLogstoreQueueMap.end()) {
            mLogstoreQueueMap.erase(iter);
        }
    }

    void DeletePriority(const LogstoreFeedBackKey& key) {
        PTScopedLock dataLock(mLock);
        DeletePriorityNoLock(key);
    }

    void SetPriority(const LogstoreFeedBackKey& key, int32_t priority) {
        PTScopedLock dataLock(mLock);
        SetPriorityNoLock(key, priority);
    }

    void SetPriorityNoLock(const LogstoreFeedBackKey& key, int32_t priority) {
        if (priority < 1 || priority > MAX_CONFIG_PRIORITY_LEVEL) {
            return;
        }
        priority -= 1;
        // should delete this key first
        DeletePriorityNoLock(key);
        mPriorityQueueArray[priority].push_back(SingleLogStorePriorityQueue(priority, key, &mLogstoreQueueMap[key]));
    }

    void DeletePriorityNoLock(const LogstoreFeedBackKey& key) {
        for (size_t i = 0; i < MAX_CONFIG_PRIORITY_LEVEL; ++i) {
            for (LogstoreFeedBackQueueVectorIterator iter = mPriorityQueueArray[i].begin();
                 iter != mPriorityQueueArray[i].end();
                 ++iter) {
                if (iter->mKey == key) {
                    mPriorityQueueArray[i].erase(iter);
                    return;
                }
            }
        }
    }

    void ConvertToExactlyOnceQueue(const LogstoreFeedBackKey& key) {
        PTScopedLock dataLock(mLock);
        mLogstoreQueueMap[key].SetType(QueueType::ExactlyOnce);
    }

protected:
    PTMutex mLock;
    TriggerEvent mTrigger;
    LogstoreFeedBackInterface* mFeedBackObj;
    LogstoreFeedBackQueueMap mLogstoreQueueMap;
    LogstoreFeedBackQueueVector mPriorityQueueArray[MAX_CONFIG_PRIORITY_LEVEL];

private:
    bool CanPopItem(int32_t threadNo,
                    int32_t threadNum,
                    LogstoreFeedBackInterface* checkObj,
                    const LogstoreFeedBackKey& key,
                    SingleLogStoreQueue& queue) const {
        // For each exactly once queue, only one thread can process it.
        if (queue.GetQueueType() == QueueType::ExactlyOnce && (key % threadNum != threadNo)) {
            return false;
        }
        return checkObj->IsValidToPush(key);
    }

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ExactlyOnceReaderUnittest;
    friend class SenderUnittest;
    friend class QueueManagerUnittest;

public:
    // do not clear real data
    void RemoveAll() {
        PTScopedLock dataLock(mLock);
        mLogstoreQueueMap.clear();
        for (size_t i = 0; i < MAX_CONFIG_PRIORITY_LEVEL; ++i) {
            mPriorityQueueArray[i].clear();
        }
    }

    void ClearEmptyQueue() {
        PTScopedLock dataLock(mLock);
        LogstoreFeedBackQueueMapIterator iter = mLogstoreQueueMap.begin();
        while (iter != mLogstoreQueueMap.end()) {
            if (iter->second.IsEmpty()) {
                // delete priority queue first
                DeletePriorityNoLock(iter->first);
                iter = mLogstoreQueueMap.erase(iter);
            } else {
                ++iter;
            }
        }
    }

    bool PopNextItem(LogstoreFeedBackKey& startKey, T& item) {
        int rst = 0;
        do {
            PTScopedLock dataLock(mLock);
            if (mLogstoreQueueMap.empty()) {
                return false;
            }
            for (size_t i = 0; i < MAX_CONFIG_PRIORITY_LEVEL; ++i) {
                for (LogstoreFeedBackQueueVectorIterator iter = mPriorityQueueArray[i].begin();
                     iter != mPriorityQueueArray[i].end();
                     ++iter) {
                    rst = iter->mQueue->PopItem(item);
                    if (rst == 0) {
                        continue;
                    }
                    // don't set start key, for fairness
                    //startKey = iter->mKey;
                    if (rst == 2 && mFeedBackObj != NULL) {
                        mFeedBackObj->FeedBack(iter->mKey);
                    }
                    return rst != 0;
                }
            }
            LogstoreFeedBackQueueMapIterator startKeyIter = mLogstoreQueueMap.find(startKey);
            if (startKeyIter == mLogstoreQueueMap.end()) {
                startKeyIter = mLogstoreQueueMap.begin();
            } else {
                ++startKeyIter;
            }

            for (LogstoreFeedBackQueueMapIterator iter = startKeyIter; iter != mLogstoreQueueMap.end(); ++iter) {
                rst = iter->second.PopItem(item);
                if (rst == 0) {
                    continue;
                }
                startKey = iter->first;
                break;
            }
            if (rst != 0) {
                break;
            }
            for (LogstoreFeedBackQueueMapIterator iter = mLogstoreQueueMap.begin(); iter != startKeyIter; ++iter) {
                rst = iter->second.PopItem(item);
                if (rst == 0) {
                    continue;
                }
                startKey = iter->first;
                break;
            }
        } while (false);

        if (rst == 2 && mFeedBackObj != NULL) {
            mFeedBackObj->FeedBack(startKey);
        }
        return rst != 0;
    }
#endif
};

} // namespace logtail
