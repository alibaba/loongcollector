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
#include <string>
#include "log_pb/sls_logs.pb.h"
#include <unordered_map>
#include <vector>
#include "common/Lock.h"
#include "common/LogGroupContext.h"
#include "common/Flags.h"
#include "flusher/FlusherSLS.h"

DECLARE_FLAG_INT32(batch_send_interval);

namespace logtail {

struct MergeItem {
    int32_t mLastUpdateTime;
    int32_t mRawBytes;
    int32_t mLines;
    std::string mProjectName;
    std::string mConfigName;
    std::string mFilename;
    sls_logs::LogGroup mLogGroup;
    std::string mShardHashKey;
    bool mBufferOrNot;
    std::string mAliuid;
    std::string mRegion;
    int64_t mKey; // for batchmap
    FlusherSLS::Batch::MergeType mMergeType;
    LogstoreFeedBackKey mLogstoreKey;
    int32_t mLogTimeInMinute;
    int32_t mBatchSendInterval;

    LogGroupContext mLogGroupContext;

    bool IsReady();
    MergeItem(const std::string& projectName,
              const std::string& configName,
              const std::string& filename,
              const bool bufferOrNot,
              const std::string& aliuid,
              const std::string& region,
              int64_t key,
              FlusherSLS::Batch::MergeType mergeType,
              const std::string& shardHashKey,
              const LogstoreFeedBackKey& logstoreKey,
              int32_t batchSendInterval = INT32_FLAG(batch_send_interval),
              const LogGroupContext& context = LogGroupContext()) {
        mProjectName = projectName;
        mConfigName = configName;
        mFilename = filename;
        mBufferOrNot = bufferOrNot;
        mAliuid = aliuid;
        mRegion = region;
        mKey = key;
        mMergeType = mergeType;
        mLastUpdateTime = -1;
        mRawBytes = 0;
        mLines = 0;
        mShardHashKey = shardHashKey;
        mLogstoreKey = logstoreKey;
        mLogTimeInMinute = -1;
        mBatchSendInterval = batchSendInterval;
        mLogGroupContext = context;
    }
};

struct PackageListMergeBuffer {
    int32_t mTotalRawBytes;
    int32_t mFirstItemTime;
    int32_t mItemCount;
    std::vector<MergeItem*> mMergeItems;

    PackageListMergeBuffer() {
        mTotalRawBytes = 0;
        mItemCount = 0;
        mFirstItemTime = 0;
    }

    void AddMergeItem(MergeItem* item) {
        mMergeItems.push_back(item);
        mTotalRawBytes += item->mRawBytes;
        mItemCount++;
        if (item->mLastUpdateTime < mFirstItemTime || mFirstItemTime == 0)
            mFirstItemTime = item->mLastUpdateTime;
    }

    bool IsReady(int32_t curTime);

    MergeItem* GetFirstItem() {
        if (!mMergeItems.empty()) {
            return mMergeItems[0];
        }
        return NULL;
    }
};

class Aggregator {
public:
    static Aggregator* GetInstance() {
        static Aggregator* instance = new Aggregator();
        return instance;
    }

    bool FlushReadyBuffer();
    bool IsMergeMapEmpty();

    std::string
    CalPostRequestShardHashKey(const std::string& source, const std::string& topic, const FlusherSLS* config);

    bool Add(const std::string& projectName,
             const std::string& sourceId,
             sls_logs::LogGroup& logGroup,
             int64_t logGroupKey,
             const FlusherSLS* config,
             FlusherSLS::Batch::MergeType mergeType,
             uint32_t logGroupSize,
             const std::string& defaultRegion = "",
             const std::string& filename = "",
             const LogGroupContext& context = LogGroupContext());

    void CleanLogPackSeqMap();
    void CleanTimeoutLogPackSeq();

private:
    struct LogPackSeqInfo {
    public:
        int64_t mSeq;
        int32_t mLastUpdateTime;

        LogPackSeqInfo(int64_t seq) {
            mSeq = seq;
            mLastUpdateTime = time(NULL);
        }

        void IncPackSeq() {
            ++mSeq;
            mLastUpdateTime = time(NULL);
        }
    };

    void MergeTruncateInfo(const sls_logs::LogGroup& logGroup, MergeItem* mergeItem);

    int64_t GetAndIncLogPackSeq(int64_t key);

    void AddPackIDForLogGroup(const std::string& packIDPrefix, int64_t logGroupKey, sls_logs::LogGroup& logGroup);

private:
    Aggregator() = default;
    ~Aggregator() = default;

private:
    std::unordered_map<int64_t, LogPackSeqInfo*> mLogPackSeqMap;
    PTMutex mLogPackSeqMapLock;

    std::unordered_map<int64_t, MergeItem*> mMergeMap;
    std::unordered_map<int64_t, PackageListMergeBuffer*> mPackageListMergeMap;
    PTMutex mMergeLock;

#ifdef APSARA_UNIT_TEST_MAIN
    int mSendVectorSize = 0;
    friend class SenderUnittest;
    friend class AggregatorUnittest;
#endif
};

} // namespace logtail
