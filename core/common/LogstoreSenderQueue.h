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
#include <deque>
#include <stdio.h>
#include "logger/Logger.h"
#include "LogGroupContext.h"
#include "Lock.h"
#include "LogstoreFeedbackQueue.h"

namespace logtail {

enum LoggroupSendStatus { LoggroupSendStatus_Idle, LoggroupSendStatus_Sending, LoggroupSendStatus_Ok };

enum SEND_DATA_TYPE { LOG_PACKAGE_LIST, LOGGROUP_LZ4_COMPRESSED };

struct LogstoreSenderStatistics {
    LogstoreSenderStatistics();
    void Reset();

    int32_t mMaxUnsendTime;
    int32_t mMinUnsendTime;
    int32_t mMaxSendSuccessTime;
    uint32_t mSendQueueSize;
    uint32_t mSendNetWorkErrorCount;
    uint32_t mSendQuotaErrorCount;
    uint32_t mSendDiscardErrorCount;
    uint32_t mSendSuccessCount;
    bool mSendBlockFlag;
    bool mValidToSendFlag;
};

struct LoggroupTimeValue {
    int32_t mLastUpdateTime;
    SEND_DATA_TYPE mDataType;
    std::string mLogData;
    int32_t mRawSize;
    int32_t mLogLines;
    bool mBufferOrNot;
    std::string mProjectName;
    std::string mLogstore;
    std::string mConfigName;
    std::string mFilename;

    // truncate info
    std::string mTruncateInfo;

    int32_t mSendRetryTimes;
    std::string mAliuid;
    std::string mRegion;
    std::string mShardHashKey;
    std::string mCurrentEndpoint;
    LoggroupSendStatus mStatus;
    LogstoreFeedBackKey mLogstoreKey;
    bool mRealIpFlag;

    // each succeeded sending log group only contains logs in the same minute
    int32_t mLogTimeInMinute;
    LogGroupContext mLogGroupContext;

    LoggroupTimeValue(const std::string& projectName,
                      const std::string& logstore,
                      const std::string& configName,
                      const std::string& filename,
                      bool bufferOrNot,
                      const std::string& aliuid,
                      const std::string& region,
                      SEND_DATA_TYPE dataType,
                      int32_t lines,
                      int32_t rawSize,
                      int32_t lastUpdateTime,
                      const std::string& shardHashKey,
                      const LogstoreFeedBackKey& logstoreKey,
                      const LogGroupContext& context = LogGroupContext()) {
        mProjectName = projectName;
        mLogstore = logstore;
        mConfigName = configName;
        mFilename = filename;
        mBufferOrNot = bufferOrNot;
        mAliuid = aliuid;
        mRegion = region;
        mDataType = dataType;
        mLogLines = lines;
        mRawSize = rawSize;
        mLastUpdateTime = lastUpdateTime;
        mSendRetryTimes = 0;
        mLogData.clear();
        mShardHashKey = shardHashKey;
        mStatus = LoggroupSendStatus_Idle;
        mLogstoreKey = logstoreKey;
        mRealIpFlag = false;
        mLogTimeInMinute = -1;
        mLogGroupContext = context;
    }

#ifdef APSARA_UNIT_TEST_MAIN
    LoggroupTimeValue() {}
#endif
};

struct LogstoreSenderInfo {
    enum SendResult {
        SendResult_OK,
        SendResult_Buffered, // move to file buffer
        SendResult_NetworkFail,
        SendResult_QuotaFail,
        SendResult_DiscardFail,
        SendResult_OtherFail
    };

    std::string mRegion;
    volatile int32_t mLastNetworkErrorCount;
    volatile int32_t mLastQuotaExceedCount;
    volatile int32_t mLastNetworkErrorTime;
    volatile int32_t mLastQuotaExceedTime;
    volatile double mNetworkRetryInterval;
    volatile double mQuotaRetryInterval;
    volatile bool mNetworkValidFlag;
    volatile bool mQuotaValidFlag;
    volatile int32_t mSendConcurrency;
    volatile int32_t mSendConcurrencyUpdateTime;

    LogstoreSenderInfo();

    void SetRegion(const std::string& region);

    bool ConcurrencyValid();
    void ConcurrencyDec() { --mSendConcurrency; }

    bool CanSend(int32_t curTime);
    // RecordSendResult
    // @return true if need to trigger.
    bool RecordSendResult(SendResult rst, LogstoreSenderStatistics& statisticsItem);
    // return value, recover sucess flag(if logstore is invalid before, return true, else return false)
    bool OnRegionRecover(const std::string& region);
};

template <class PARAM>
class SingleLogstoreSenderManager : public SingleLogstoreFeedbackQueue<LoggroupTimeValue*, PARAM> {
public:
    SingleLogstoreSenderManager()
        : mLastSendTimeSecond(0), mLastSecondTotalBytes(0), mMaxSendBytesPerSecond(-1), mFlowControlExpireTime(0) {}

    void SetMaxSendBytesPerSecond(int32_t maxBytes, int32_t expireTime) {
        mMaxSendBytesPerSecond = maxBytes;
        mFlowControlExpireTime = expireTime;
    }

    void GetAllIdleLoggroup(std::vector<LoggroupTimeValue*>& logGroupVec) {
        if (this->mSize == 0) {
            return;
        }

        uint64_t index = QueueType::ExactlyOnce == this->mType ? 0 : this->mRead;
        const uint64_t endIndex = QueueType::ExactlyOnce == this->mType ? this->SIZE : this->mWrite;
        for (; index < endIndex; ++index) {
            LoggroupTimeValue* item = this->mArray[index % this->SIZE];
            if (item == NULL) {
                continue;
            }
            if (item->mStatus == LoggroupSendStatus_Idle) {
                item->mStatus = LoggroupSendStatus_Sending;
                logGroupVec.push_back(item);
            }
        }
    }

    void GetAllIdleLoggroupWithLimit(std::vector<LoggroupTimeValue*>& logGroupVec,
                                     int32_t nowTime,
                                     std::unordered_map<std::string, int>& regionConcurrencyLimits) {
        bool expireFlag = (mFlowControlExpireTime > 0 && nowTime > mFlowControlExpireTime);
        if (this->mSize == 0 || (!expireFlag && mMaxSendBytesPerSecond == 0)) {
            return;
        }
        if (!expireFlag && mMaxSendBytesPerSecond > 0 && nowTime != mLastSendTimeSecond) {
            mLastSecondTotalBytes = 0;
            mLastSendTimeSecond = nowTime;
        }

        int regionConcurrency = -1;
        auto iter = regionConcurrencyLimits.find(mSenderInfo.mRegion);
        if (iter != regionConcurrencyLimits.end())
            regionConcurrency = iter->second;
        if (0 == regionConcurrency) {
            return;
        }

        uint64_t index = QueueType::ExactlyOnce == this->mType ? 0 : this->mRead;
        const uint64_t endIndex = QueueType::ExactlyOnce == this->mType ? this->SIZE : this->mWrite;
        for (; index < endIndex; ++index) {
            LoggroupTimeValue* item = this->mArray[index % this->SIZE];
            if (item == NULL) {
                continue;
            }
            if (item->mStatus == LoggroupSendStatus_Idle) {
                // check consurrency
                // check first, when mMaxSendBytesPerSecond is 1000, and the packet size is 10K, we should send this packet. if not, this logstore will block
                if ((!mSenderInfo.ConcurrencyValid())
                    || (!expireFlag && mMaxSendBytesPerSecond > 0 && mLastSecondTotalBytes > mMaxSendBytesPerSecond)) {
                    return;
                }
                mSenderInfo.ConcurrencyDec();
                mLastSecondTotalBytes += item->mRawSize;
                item->mStatus = LoggroupSendStatus_Sending;
                logGroupVec.push_back(item);
                if (-1 != regionConcurrency) {
                    if (0 == --regionConcurrency) {
                        break;
                    }
                }
            }
        }
        if (iter != regionConcurrencyLimits.end()) {
            iter->second = regionConcurrency;
        }
    }

    bool insertExactlyOnceItem(LoggroupTimeValue* item) {
        auto& eo = item->mLogGroupContext.mExactlyOnceCheckpoint;
        if (eo->IsComplete()) {
            // Checkpoint is complete, which means it is a replayed checkpoint,
            //  use it directly.
            if (this->mArray[eo->index] != NULL) {
                APSARA_LOG_ERROR(sLogger,
                                 ("replay checkpoint has been used",
                                  "unexpected")("key", eo->key)("checkpoint", eo->data.DebugString()));
                return false;
            }
            this->mArray[eo->index] = item;
        } else {
            // Incomplete checkpoint, select an available checkpoint for it.
            // Use mWrite to record last selected index, iterate from it for balance.
            this->mWrite++;
            for (size_t idx = 0; idx < this->SIZE; ++idx, this->mWrite++) {
                auto index = this->mWrite % this->SIZE;
                if (this->mArray[index] != NULL) {
                    continue;
                }
                this->mArray[index] = item;
                auto& newCpt = mRangeCheckpoints[index];
                newCpt->data.set_read_offset(eo->data.read_offset());
                newCpt->data.set_read_length(eo->data.read_length());
                eo = newCpt;
                break;
            }
            if (!eo->IsComplete()) {
                this->mValid = false;
                mExtraBuffers.push_back(item);
                APSARA_LOG_DEBUG(sLogger,
                                 ("can not find any checkpoint for data",
                                  "add to extra buffer")("fb key", eo->fbKey)("checkpoint", eo->data.DebugString())(
                                     "queue size", this->mSize)("buffer size", mExtraBuffers.size()));
                return true;
            }
        }

        this->mSize++;
        if (this->IsFull()) {
            this->mValid = false;
        }
        eo->Prepare();
        APSARA_LOG_DEBUG(
            sLogger,
            ("bind data with checkpoint", eo->index)("checkpoint", eo->data.DebugString())("queue size", this->mSize));
        return true;
    }

    // with empty item
    bool InsertItem(LoggroupTimeValue* item) {
        mSenderInfo.SetRegion(item->mRegion);
        if (QueueType::ExactlyOnce == this->mType) {
            return insertExactlyOnceItem(item);
        }

        if (this->IsFull()) {
            return false;
        }
        auto index = this->mRead;
        for (; index < this->mWrite; ++index) {
            if (this->mArray[index % this->SIZE] == NULL) {
                break;
            }
        }
#ifdef LOGTAIL_DEBUG_FLAG
        APSARA_LOG_DEBUG(sLogger,
                         ("Insert send item",
                          item)("size", this->mSize)("read", this->mRead)("write", this->mWrite)("index", index));
#endif
        this->mArray[index % this->SIZE] = item;
        if (index == this->mWrite) {
            ++this->mWrite;
        }
        ++this->mSize;
        if (this->mSize == this->HIGH_SIZE) {
            this->mValid = false;
        }
        return true;
    }

    int32_t removeExactlyOnceItem(LoggroupTimeValue* item) {
        auto& checkpoint = item->mLogGroupContext.mExactlyOnceCheckpoint;
        this->mArray[checkpoint->index] = NULL;
        this->mSize--;
        delete item;
        if (!mExtraBuffers.empty()) {
            auto extraItem = mExtraBuffers.front();
            mExtraBuffers.pop_front();
            auto& eo = extraItem->mLogGroupContext.mExactlyOnceCheckpoint;
            APSARA_LOG_DEBUG(sLogger,
                             ("process extra item at first",
                              "after item removed")("fb key", eo->fbKey)("checkpoint", eo->data.DebugString()));
            insertExactlyOnceItem(extraItem);
            return 1;
        }
        this->mValid = true;
        return 2;
    }

    // with empty item
    int32_t RemoveItem(LoggroupTimeValue* item, bool deleteFlag) {
        if (this->IsEmpty()) {
            return 0;
        }

        if (QueueType::ExactlyOnce == this->mType) {
            return removeExactlyOnceItem(item);
        }

        auto index = this->mRead;
        for (; index < this->mWrite; ++index) {
            auto dataItem = this->mArray[index % this->SIZE];
            if (dataItem == item) {
                if (deleteFlag) {
                    delete dataItem;
                }
#ifdef LOGTAIL_DEBUG_FLAG
                APSARA_LOG_DEBUG(sLogger,
                                 ("Remove send item", dataItem)("size", this->mSize)("read", this->mRead)(
                                     "write", this->mWrite)("index", index));
#endif
                this->mArray[index % this->SIZE] = NULL;
                break;
            }
        }
        if (index == this->mWrite) {
            APSARA_LOG_ERROR(
                sLogger,
                ("find no sender item, project", item->mProjectName)("logstore", item->mLogstore)(
                    "lines", item->mLogLines)("read", this->mRead)("write", this->mWrite)("size", this->mSize));
            return 0;
        }
        // need to check mWrite to avoid dead while
        while (this->mRead < this->mWrite && this->mArray[this->mRead % this->SIZE] == NULL) {
            ++this->mRead;
        }
        int rst = 1;
        if (--this->mSize == this->LOW_SIZE && !this->mValid) {
            this->mValid = true;
            rst = 2;
        }
        return rst;
    }

    LogstoreSenderStatistics GetSenderStatistics() {
        LogstoreSenderStatistics statisticsItem = mSenderStatistics;
        mSenderStatistics.Reset();

        statisticsItem.mSendBlockFlag = !this->IsValid();
        statisticsItem.mValidToSendFlag = IsValidToSend(time(NULL));
        if (this->IsEmpty()) {
            return statisticsItem;
        }

        uint64_t index = QueueType::ExactlyOnce == this->mType ? 0 : this->mRead;
        int32_t minSendTime = INT32_MAX;
        int32_t maxSendTime = 0;
        for (size_t i = 0; i < this->mSize; ++i, ++index) {
            LoggroupTimeValue* item = this->mArray[index % this->SIZE];
            if (item == NULL) {
                continue;
            }

            if (item->mLastUpdateTime < minSendTime) {
                minSendTime = item->mLastUpdateTime;
            }
            if (item->mLastUpdateTime > maxSendTime) {
                maxSendTime = item->mLastUpdateTime;
            }
            ++statisticsItem.mSendQueueSize;
        }
        if (statisticsItem.mSendQueueSize > 0) {
            statisticsItem.mMinUnsendTime = minSendTime;
            statisticsItem.mMaxUnsendTime = maxSendTime;
        }
        return statisticsItem;
    }

    int32_t OnSendDone(LoggroupTimeValue* item, LogstoreSenderInfo::SendResult sendRst, bool& needTrigger) {
        needTrigger = mSenderInfo.RecordSendResult(sendRst, mSenderStatistics);
        // if send error, reset status to idle, and wait to send again
        // network fail or quota fail
        if (sendRst != LogstoreSenderInfo::SendResult_OK && sendRst != LogstoreSenderInfo::SendResult_Buffered
            && sendRst != LogstoreSenderInfo::SendResult_DiscardFail) {
            item->mStatus = LoggroupSendStatus_Idle;
            return 0;
        }
        if (mSenderStatistics.mMaxSendSuccessTime < item->mLastUpdateTime) {
            mSenderStatistics.mMaxSendSuccessTime = item->mLastUpdateTime;
        }
        // else remove item except buffered
        return RemoveItem(item, sendRst != LogstoreSenderInfo::SendResult_Buffered);
    }

    bool IsValidToSend(int32_t curTime) { return mSenderInfo.CanSend(curTime); }

    LogstoreSenderInfo mSenderInfo;
    LogstoreSenderStatistics mSenderStatistics;

    //add for flow control
    volatile int32_t mLastSendTimeSecond;
    volatile int32_t mLastSecondTotalBytes;

    volatile int32_t mMaxSendBytesPerSecond; // <0, no flowControl, 0 pause sending,
    volatile int32_t mFlowControlExpireTime; // <=0 no expire

    std::vector<RangeCheckpointPtr> mRangeCheckpoints;
    std::deque<LoggroupTimeValue*> mExtraBuffers;
};

template <class PARAM>
class LogstoreSenderQueue : public LogstoreFeedBackInterface {
protected:
    typedef SingleLogstoreSenderManager<PARAM> SingleLogStoreManager;
    typedef std::unordered_map<LogstoreFeedBackKey, SingleLogStoreManager> LogstoreFeedBackQueueMap;
    typedef typename std::unordered_map<LogstoreFeedBackKey, SingleLogStoreManager>::iterator
        LogstoreFeedBackQueueMapIterator;

public:
    LogstoreSenderQueue() : mFeedBackObj(NULL), mUrgentFlag(false), mSenderQueueBeginIndex(0) {}

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

    void Signal() { mTrigger.Trigger(); }

    virtual void FeedBack(const LogstoreFeedBackKey& key) { mTrigger.Trigger(); }

    virtual bool IsValidToPush(const LogstoreFeedBackKey& key) {
        PTScopedLock dataLock(mLock);
        auto& singleQueue = mLogstoreSenderQueueMap[key];

        // For correctness, exactly once queue should ignore mUrgentFlag.
        if (singleQueue.GetQueueType() == QueueType::ExactlyOnce) {
            return singleQueue.IsValid();
        }

        if (mUrgentFlag) {
            return true;
        }
        return singleQueue.IsValid();
    }

    void SetLogstoreFlowControl(const LogstoreFeedBackKey& key, int32_t maxBytes, int32_t expireTime) {
        PTScopedLock dataLock(mLock);
        SingleLogStoreManager& singleQueue = mLogstoreSenderQueueMap[key];
        singleQueue.SetMaxSendBytesPerSecond(maxBytes, expireTime);
    }

    void ConvertToExactlyOnceQueue(const LogstoreFeedBackKey& key, const std::vector<RangeCheckpointPtr>& checkpoints) {
        PTScopedLock dataLock(mLock);
        auto& queue = mLogstoreSenderQueueMap[key];
        queue.mRangeCheckpoints = checkpoints;
        queue.ConvertToExactlyOnceQueue(0, checkpoints.size(), checkpoints.size());
    }

    bool Wait(int32_t waitMs) { return mTrigger.Wait(waitMs); }

    bool IsValid(const LogstoreFeedBackKey& key) {
        PTScopedLock dataLock(mLock);
        SingleLogStoreManager& singleQueue = mLogstoreSenderQueueMap[key];
        return singleQueue.IsValid();
    }

    bool PushItem(const LogstoreFeedBackKey& key, LoggroupTimeValue* const& item) {
        {
            PTScopedLock dataLock(mLock);
            SingleLogStoreManager& singleQueue = mLogstoreSenderQueueMap[key];
            if (!singleQueue.InsertItem(item)) {
                return false;
            }
        }
        Signal();
        return true;
    }

    void CheckAndPopAllItem(std::vector<LoggroupTimeValue*>& itemVec,
                            int32_t curTime,
                            bool& singleQueueFullFlag,
                            std::unordered_map<std::string, int>& regionConcurrencyLimits) {
        singleQueueFullFlag = false;
        PTScopedLock dataLock(mLock);
        if (mLogstoreSenderQueueMap.empty()) {
            return;
        }

        // must check index before moving iterator
        mSenderQueueBeginIndex = mSenderQueueBeginIndex % mLogstoreSenderQueueMap.size();

        // here we set sender queue begin index, let the sender order be different each time
        LogstoreFeedBackQueueMapIterator beginIter = mLogstoreSenderQueueMap.begin();
        std::advance(beginIter, mSenderQueueBeginIndex++);

        PopItem(
            beginIter, mLogstoreSenderQueueMap.end(), itemVec, curTime, regionConcurrencyLimits, singleQueueFullFlag);
        PopItem(
            mLogstoreSenderQueueMap.begin(), beginIter, itemVec, curTime, regionConcurrencyLimits, singleQueueFullFlag);
    }

    static void PopItem(LogstoreFeedBackQueueMapIterator beginIter,
                        LogstoreFeedBackQueueMapIterator endIter,
                        std::vector<LoggroupTimeValue*>& itemVec,
                        int32_t curTime,
                        std::unordered_map<std::string, int>& regionConcurrencyLimits,
                        bool& singleQueueFullFlag) {
        for (LogstoreFeedBackQueueMapIterator iter = beginIter; iter != endIter; ++iter) {
            SingleLogStoreManager& singleQueue = iter->second;
            if (!singleQueue.IsValidToSend(curTime)) {
                continue;
            }
            singleQueue.GetAllIdleLoggroupWithLimit(itemVec, curTime, regionConcurrencyLimits);
            singleQueueFullFlag |= !singleQueue.IsValid();
        }
    }

    void PopAllItem(std::vector<LoggroupTimeValue*>& itemVec, int32_t curTime, bool& singleQueueFullFlag) {
        singleQueueFullFlag = false;
        PTScopedLock dataLock(mLock);
        for (LogstoreFeedBackQueueMapIterator iter = mLogstoreSenderQueueMap.begin();
             iter != mLogstoreSenderQueueMap.end();
             ++iter) {
            iter->second.GetAllIdleLoggroup(itemVec);
            singleQueueFullFlag |= !iter->second.IsValid();
        }
    }

    void OnLoggroupSendDone(LoggroupTimeValue* item, LogstoreSenderInfo::SendResult sendRst) {
        if (item == NULL) {
            return;
        }

        LogstoreFeedBackKey key = item->mLogstoreKey;
        int rst = 0;
        bool needTrigger = false;
        {
            PTScopedLock dataLock(mLock);
            SingleLogStoreManager& singleQueue = mLogstoreSenderQueueMap[key];
            rst = singleQueue.OnSendDone(item, sendRst, needTrigger);
        }
        if (rst == 2 && mFeedBackObj != NULL) {
            APSARA_LOG_DEBUG(sLogger, ("OnLoggroupSendDone feedback", ""));
            mFeedBackObj->FeedBack(key);
        }
        if (needTrigger) {
            // trigger deamon sender
            Signal();
        }
    }

    void OnRegionRecover(const std::string& region) {
        APSARA_LOG_DEBUG(sLogger, ("Recover region", region));
        bool recoverFlag = false;
        PTScopedLock dataLock(mLock);
        for (LogstoreFeedBackQueueMapIterator iter = mLogstoreSenderQueueMap.begin();
             iter != mLogstoreSenderQueueMap.end();
             ++iter) {
            recoverFlag |= iter->second.mSenderInfo.OnRegionRecover(region);
        }
        if (recoverFlag) {
            Signal();
        }
    }

    bool IsEmpty() {
        PTScopedLock dataLock(mLock);
        for (LogstoreFeedBackQueueMapIterator iter = mLogstoreSenderQueueMap.begin();
             iter != mLogstoreSenderQueueMap.end();
             ++iter) {
            if (!iter->second.IsEmpty()) {
                return false;
            }
        }
        return true;
    }

    bool IsEmpty(const LogstoreFeedBackKey& key) {
        PTScopedLock dataLock(mLock);
        auto iter = mLogstoreSenderQueueMap.find(key);
        return iter == mLogstoreSenderQueueMap.end() || iter->second.IsEmpty();
    }

    void Delete(const LogstoreFeedBackKey& key) {
        PTScopedLock dataLock(mLock);
        auto iter = mLogstoreSenderQueueMap.find(key);
        if (iter != mLogstoreSenderQueueMap.end()) {
            mLogstoreSenderQueueMap.erase(iter);
        }
    }

    // do not clear real data, just for unit test
    void RemoveAll() {
        PTScopedLock dataLock(mLock);
        mLogstoreSenderQueueMap.clear();
        mSenderQueueBeginIndex = 0;
    }

    void Lock() { mLock.lock(); }

    void Unlock() { mLock.unlock(); }

    void SetUrgent() { mUrgentFlag = true; }

    void ResetUrgent() { mUrgentFlag = false; }

    LogstoreSenderStatistics GetSenderStatistics(const LogstoreFeedBackKey& key) {
        PTScopedLock dataLock(mLock);
        LogstoreFeedBackQueueMapIterator iter = mLogstoreSenderQueueMap.find(key);
        if (iter == mLogstoreSenderQueueMap.end()) {
            return LogstoreSenderStatistics();
        }
        return iter->second.GetSenderStatistics();
    }

    void GetStatus(int32_t& normalInvalidCount,
                   int32_t& normalInvalidSenderCount,
                   int32_t& normalTotalCount,
                   int32_t& eoInvalidCount,
                   int32_t& eoInvalidSenderCount,
                   int32_t& eoTotalCount) {
        int32_t curTime = time(NULL);
        PTScopedLock dataLock(mLock);
        for (LogstoreFeedBackQueueMapIterator iter = mLogstoreSenderQueueMap.begin();
             iter != mLogstoreSenderQueueMap.end();
             ++iter) {
            bool isExactlyOnceQueue = iter->second.GetQueueType() == QueueType::ExactlyOnce;
            auto& invalidCount = isExactlyOnceQueue ? eoInvalidCount : normalInvalidCount;
            auto& invalidSenderCount = isExactlyOnceQueue ? eoInvalidSenderCount : normalInvalidSenderCount;
            auto& totalCount = isExactlyOnceQueue ? eoTotalCount : normalTotalCount;

            ++totalCount;
            if (!iter->second.IsValid()) {
                ++invalidCount;
            }
            if (!iter->second.IsValidToSend(curTime)) {
                ++invalidSenderCount;
            }
        }
    }

protected:
    LogstoreFeedBackQueueMap mLogstoreSenderQueueMap;
    PTMutex mLock;
    TriggerEvent mTrigger;
    LogstoreFeedBackInterface* mFeedBackObj;
    bool mUrgentFlag;
    size_t mSenderQueueBeginIndex;

private:
#ifdef APSARA_UNIT_TEST_MAIN
    friend class SenderUnittest;
    friend class ExactlyOnceReaderUnittest;
    friend class QueueManagerUnittest;

    void PrintStatus() {
        printf("================================\n");
        PTScopedLock dataLock(mLock);
        for (LogstoreFeedBackQueueMapIterator iter = mLogstoreSenderQueueMap.begin();
             iter != mLogstoreSenderQueueMap.end();
             ++iter) {
            SingleLogStoreManager& logstoreManager = iter->second;

            printf(" %d   %d   %s \n ",
                   (int32_t)iter->first,
                   (int32_t)logstoreManager.GetSize(),
                   logstoreManager.mSenderInfo.mRegion.c_str());
        }
        printf("================================\n");
    }

    LogstoreSenderInfo* GetSenderInfo(LogstoreFeedBackKey key) {
        PTScopedLock dataLock(mLock);
        LogstoreFeedBackQueueMapIterator iter = mLogstoreSenderQueueMap.find(key);
        if (iter == mLogstoreSenderQueueMap.end()) {
            return NULL;
        }
        return &(iter->second.mSenderInfo);
    }
#endif
};

} // namespace logtail
