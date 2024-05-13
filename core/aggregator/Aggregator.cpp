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

#include "Aggregator.h"

#include <app_config/AppConfig.h>
#include <common/Constants.h>
#include <common/HashUtil.h>
#include <common/StringTools.h>
#include <config_manager/ConfigManager.h>
#include <monitor/LogFileProfiler.h>

#include <numeric>
#include <vector>

#include "application/Application.h"
#include "common/LogtailCommonFlags.h"
#include "sender/Sender.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif

using namespace std;
using namespace sls_logs;

DEFINE_FLAG_BOOL(default_secondary_storage, "default strategy whether enable secondary storage", false);
DEFINE_FLAG_INT32(batch_send_metric_size, "batch send metric size limit(bytes)(default 256KB)", 256 * 1024);

DECLARE_FLAG_INT32(merge_log_count_limit);
DECLARE_FLAG_INT32(same_topic_merge_send_count);
DECLARE_FLAG_INT32(max_send_log_group_size);

namespace logtail {


bool MergeItem::IsReady() {
    return mRawBytes > INT32_FLAG(batch_send_metric_size) || ((time(NULL) - mLastUpdateTime) >= mBatchSendInterval);
}

bool PackageListMergeBuffer::IsReady(int32_t curTime) {
    // should use 2 * INT32_FLAG(batch_send_interval)), package list interval should > merge item interval
    return (mTotalRawBytes >= INT32_FLAG(batch_send_metric_size))
        || (mItemCount > 0 && (curTime - mFirstItemTime) >= INT32_FLAG(batch_send_interval));
}


bool Aggregator::FlushReadyBuffer() {
    static Sender* sender = Sender::Instance();
    vector<MergeItem*> sendDataVec;
    {
        PTScopedLock lock(mMergeLock);
        unordered_map<int64_t, MergeItem*>::iterator itr = mMergeMap.begin();
        for (; itr != mMergeMap.end();) {
            if (Application::GetInstance()->IsExiting()
                || (itr->second->IsReady()
                    && sender->GetSenderFeedBackInterface()->IsValidToPush(itr->second->mLogstoreKey))) {
                if (itr->second->mMergeType == FlusherSLS::Batch::MergeType::TOPIC)
                    sendDataVec.push_back(itr->second);
                else {
                    int64_t key = itr->second->mKey;
                    unordered_map<int64_t, PackageListMergeBuffer*>::iterator pIter = mPackageListMergeMap.find(key);
                    if (pIter == mPackageListMergeMap.end()) {
                        PackageListMergeBuffer* tmpPtr = new PackageListMergeBuffer;
                        pIter = mPackageListMergeMap.insert(std::make_pair(key, tmpPtr)).first;
                    }
                    pIter->second->AddMergeItem(itr->second);
                }
                itr = mMergeMap.erase(itr);
            } else
                itr++;
        }
    }

    vector<vector<MergeItem*> > packageListVec;
    int32_t curTime = time(NULL);
    {
        PTScopedLock lock(mMergeLock);
        unordered_map<int64_t, PackageListMergeBuffer*>::iterator pIter = mPackageListMergeMap.begin();
        for (; pIter != mPackageListMergeMap.end();) {
            if (Application::GetInstance()->IsExiting()
                || (pIter->second->IsReady(curTime) && pIter->second->mMergeItems.size() > 0
                    && sender->GetSenderFeedBackInterface()->IsValidToPush(
                        pIter->second->GetFirstItem()->mLogstoreKey))) {
#ifdef LOGTAIL_DEBUG_FLAG
                LOG_DEBUG(sLogger,
                          ("Flush logstore merged packet, size", pIter->second->mMergeItems.size())(
                              "first time", pIter->second->mFirstItemTime)("now", curTime));
#endif
                if (pIter->second->mItemCount > 1)
                    packageListVec.push_back(pIter->second->mMergeItems);
                else
                    sendDataVec.push_back(
                        pIter->second->mMergeItems[0]); // send LogGroup avoid more cost for LogPackageList
                delete pIter->second;
                pIter = mPackageListMergeMap.erase(pIter);
            } else
                pIter++;
        }
    }

    if (sendDataVec.size() > 0)
        sender->SendCompressed(sendDataVec);

    for (vector<vector<MergeItem*> >::iterator plIter = packageListVec.begin(); plIter != packageListVec.end();
         ++plIter)
        sender->SendLogPackageList(*plIter);
    return true;
}

void Aggregator::AddPackIDForLogGroup(const std::string& packIDPrefix,
                                      int64_t logGroupKey,
                                      sls_logs::LogGroup& logGroup) {
    // No logtags -> shennong metrics -> no need to add pack ID.
    if (packIDPrefix.empty() || logGroup.logtags_size() == 0) {
        return;
    }

    auto logTagPtr = logGroup.add_logtags();
    logTagPtr->set_key(LOG_RESERVED_KEY_PACKAGE_ID);
    logTagPtr->set_value(packIDPrefix + "-" + ToHexString(GetAndIncLogPackSeq(logGroupKey)));
}

bool Aggregator::Add(const std::string& projectName,
                     const std::string& sourceId,
                     sls_logs::LogGroup& logGroup,
                     int64_t logGroupKey,
                     const FlusherSLS* config,
                     FlusherSLS::Batch::MergeType mergeType,
                     uint32_t logGroupSize,
                     const std::string& defaultRegion,
                     const std::string& filename,
                     const LogGroupContext& context) {
    if (logGroupSize == 0) {
        logGroupSize = logGroup.ByteSize();
    }
    if ((int32_t)logGroupSize > INT32_FLAG(max_send_log_group_size)) {
        LOG_ERROR(sLogger,
                  ("log group size exceed limit. actual size", logGroupSize)("size limit",
                                                                             INT32_FLAG(max_send_log_group_size)));
        return false;
    }

    int32_t logSize = (int32_t)logGroup.logs_size();
    if (logSize == 0)
        return true;
    vector<int32_t> neededLogs;
    int32_t neededLogSize = logSize;
    neededLogs.resize(logSize);
    std::iota(std::begin(neededLogs), std::end(neededLogs), 0);

    static Sender* sender = Sender::Instance();
    const string& region = (config == NULL ? defaultRegion : config->mRegion);
    const string& aliuid = (config == NULL ? STRING_FLAG(logtail_profile_aliuid) : config->mAliuid);
    const string& configName = ((config == NULL || !config->HasContext()) ? "" : config->GetContext().GetConfigName());
    const string& category = logGroup.category();
    const string& topic = logGroup.topic();
    const string& source = logGroup.has_source() ? logGroup.source() : LogFileProfiler::mIpAddr;
    string shardHashKey = CalPostRequestShardHashKey(source, topic, config);

    // Replay checkpoint had already been merged, resend directly.
    if (context.mExactlyOnceCheckpoint && context.mExactlyOnceCheckpoint->IsComplete()) {
        AddPackIDForLogGroup(sourceId, logGroupKey, logGroup);
        sender->SendCompressed(projectName, logGroup, neededLogs, configName, aliuid, region, filename, context);
        LOG_DEBUG(sLogger,
                  ("complete checkpoint", "resend directly")("filename", filename)(
                      "key", context.mExactlyOnceCheckpoint->key)("checkpoint",
                                                                  context.mExactlyOnceCheckpoint->data.DebugString()));
        return true;
    }

    LogstoreFeedBackKey feedBackKey
        = config == NULL ? GenerateLogstoreFeedBackKey(projectName, category) : config->GetLogstoreKey();
    int64_t key, logstoreKey = 0;
    if (mergeType == FlusherSLS::Batch::MergeType::LOGSTORE) {
        logstoreKey = HashString(projectName + "_" + category);
        key = logstoreKey;
    } else {
        key = logGroupKey;
    }

    LogGroup discardLogGroup;
    vector<MergeItem*> sendDataVec;
    int32_t logByteSize = logGroupSize / logSize;
    decltype(logGroup.mutable_logs()->mutable_data()) mutableLogPtr = logGroup.mutable_logs()->mutable_data();
    int32_t neededIdx = 0;
    int32_t discardLogSize = logSize - neededLogSize;
    if (discardLogSize > 0) {
        discardLogGroup.mutable_logs()->Reserve(discardLogSize);
    }
    int32_t logCountMin = INT32_FLAG(merge_log_count_limit) > 1 ? (INT32_FLAG(merge_log_count_limit) - 1) : 0;
    int32_t logGroupByteMin = AppConfig::GetInstance()->GetMaxHoldedDataSize() > logByteSize
        ? (AppConfig::GetInstance()->GetMaxHoldedDataSize() - logByteSize)
        : 0;

    int32_t curTime = time(NULL);
    {
        PTScopedLock lock(mMergeLock);
        unordered_map<int64_t, PackageListMergeBuffer*>::iterator pIter = mPackageListMergeMap.find(logstoreKey);
        ;
        unordered_map<int64_t, MergeItem*>::iterator itr = mMergeMap.find(logGroupKey);
        MergeItem* value = NULL;
        // for mergeType: LOGSTORE, value is always new
        if (mergeType == FlusherSLS::Batch::MergeType::LOGSTORE) {
            if (pIter == mPackageListMergeMap.end()) {
                PackageListMergeBuffer* tmpPtr = new PackageListMergeBuffer();
                pIter = mPackageListMergeMap.insert(std::make_pair(logstoreKey, tmpPtr)).first;
            }
        } else {
            if (itr != mMergeMap.end()) {
                value = itr->second;
            } else {
                itr = mMergeMap.insert(std::make_pair(logGroupKey, value)).first;
            }
        }
        bool mergeFinishedFlag = false, initFlag = false;
        for (int32_t logIdx = 0; logIdx < logSize; logIdx++) {
            if (neededIdx < neededLogSize && logIdx == neededLogs[neededIdx]) {
                if (value == NULL
                    || (value->mLines > logCountMin || value->mRawBytes > logGroupByteMin
                        || (curTime - value->mLastUpdateTime) >= INT32_FLAG(batch_send_interval)
                        || ((value->mLogGroup).logs(0).time() / 60 != (*(mutableLogPtr + logIdx))->time() / 60))) {
                    // value is not NULL, log group merging finished
                    if (value != NULL) {
                        if (context.mMarkOffsetFlag) {
                            // log group is split here, merged into MergeItem
                            // set sparse info len to 0 because we don't know the real size of merged log group
                            FileInfo* fileInfo = new FileInfo(*context.mFileInfoPtr);
                            fileInfo->len = 0;
                            value->mLogGroupContext.mFileInfoPtr.reset(fileInfo);
                            mergeFinishedFlag = true;
                        }

                        // put finished value to sendDataVec, and set new value
                        if (mergeType != FlusherSLS::Batch::MergeType::LOGSTORE) {
                            sendDataVec.push_back(value);
                        } else {
                            pIter->second->AddMergeItem(value);
                        }
                    }

                    bool bufferOrNot = config == NULL ? BOOL_FLAG(default_secondary_storage) : true;
                    int32_t batchSendInterval
                        = config == NULL ? INT32_FLAG(batch_send_interval) : config->mBatch.mSendIntervalSecs;
                    // when old value is finish
                    // or new value
                    if (context.mExactlyOnceCheckpoint) {
                        // The log group might be splitted to multiple merge items, so we must
                        //  copy range checkpoint in context.
                        auto newContext = context;
                        newContext.mExactlyOnceCheckpoint.reset(
                            new RangeCheckpoint(*(context.mExactlyOnceCheckpoint.get())));
                        value = new MergeItem(projectName,
                                              configName,
                                              filename,
                                              bufferOrNot,
                                              aliuid,
                                              region,
                                              key,
                                              mergeType,
                                              shardHashKey,
                                              feedBackKey,
                                              batchSendInterval,
                                              newContext);
                    } else {
                        value = new MergeItem(projectName,
                                              configName,
                                              filename,
                                              bufferOrNot,
                                              aliuid,
                                              region,
                                              key,
                                              mergeType,
                                              shardHashKey,
                                              feedBackKey,
                                              batchSendInterval,
                                              context);
                    }
                    value->mLastUpdateTime = curTime; // set the last update time before enqueue

                    initFlag = true;
                    if (mergeType != FlusherSLS::Batch::MergeType::LOGSTORE) {
                        itr->second = value;
                    }

                    (value->mLogGroup).mutable_logs()->Reserve(INT32_FLAG(merge_log_count_limit));
                    (value->mLogGroup).set_category(category);
                    (value->mLogGroup).set_topic(topic);
                    (value->mLogGroup).set_machineuuid(Application::GetInstance()->GetUUID());
                    (value->mLogGroup).set_source(logGroup.has_source() ? logGroup.source() : LogFileProfiler::mIpAddr);

                    for (int32_t logTagIdx = 0; logTagIdx < logGroup.logtags_size(); ++logTagIdx) {
                        const sls_logs::LogTag& oriLogTag = logGroup.logtags(logTagIdx);
                        sls_logs::LogTag* logTagPtr = (value->mLogGroup).add_logtags();
                        logTagPtr->set_key(oriLogTag.key());
                        logTagPtr->set_value(oriLogTag.value());
                    }

                    AddPackIDForLogGroup(sourceId, logGroupKey, value->mLogGroup);
                }

                (value->mLogGroup).mutable_logs()->AddAllocated(*(mutableLogPtr + logIdx));
                if (context.mExactlyOnceCheckpoint) {
                    // TODO: for GBK, position is not correct
                    auto& logPosition = context.mExactlyOnceCheckpoint->positions[logIdx];
                    auto& cpt = value->mLogGroupContext.mExactlyOnceCheckpoint->data;

                    // First log, update read_offset.
                    if (1 == value->mLogGroup.logs_size()) {
                        cpt.set_read_offset(logPosition.first);
                    }
                    // Update read_length.
                    cpt.set_read_length(logPosition.first + logPosition.second - cpt.read_offset());
                }

                // get first log time as log time of this log group in merge item
                value->mLogTimeInMinute = (value->mLogGroup).logs(0).time() - (value->mLogGroup).logs(0).time() % 60;
                value->mRawBytes += logByteSize;
                value->mLines++;
                neededIdx++;
            } else {
                discardLogGroup.mutable_logs()->AddAllocated(*(mutableLogPtr + logIdx));
            }
        }

        // handle truncate info, the first truncate info may be inserted while merge item 'value' initialized
        if (context.mFuseMode) {
            MergeTruncateInfo(logGroup, value);
        }
        if (context.mMarkOffsetFlag && !mergeFinishedFlag && !initFlag) {
            value->mLogGroupContext.mFileInfoPtr = context.mFileInfoPtr;
        }
        // AddAllocated above
        for (int32_t logIdx = 0; logIdx < logSize; logIdx++) {
            logGroup.mutable_logs()->ReleaseLast();
        }

        if (mergeType == FlusherSLS::Batch::MergeType::LOGSTORE) {
            pIter->second->AddMergeItem(value);
            if (pIter->second->IsReady(curTime) || Application::GetInstance()->IsExiting()) {
#ifdef LOGTAIL_DEBUG_FLAG
                LOG_DEBUG(sLogger,
                          ("Send logstore merged packet, size", pIter->second->mMergeItems.size())(
                              "first time", pIter->second->mFirstItemTime)("now", curTime));
#endif

                sendDataVec.insert(
                    sendDataVec.end(), (pIter->second)->mMergeItems.begin(), (pIter->second)->mMergeItems.end());
                delete pIter->second;
                mPackageListMergeMap.erase(pIter);
            }
        } else {
            if (value != NULL
                && (value->IsReady() || Application::GetInstance()->IsExiting() || context.mExactlyOnceCheckpoint)) {
                sendDataVec.push_back(value);
                if (itr != mMergeMap.end()) {
                    mMergeMap.erase(itr);
                }
            }
        }
    }

#ifdef APSARA_UNIT_TEST_MAIN
    mSendVectorSize = sendDataVec.size();
#endif

    if (sendDataVec.size() > 0) {
        // if send data package count greater than INT32_FLAG(same_topic_merge_send_count), there will be many little
        // package in send queue, so we merge data package to send. because the send loggroup's max size is 512k, so
        // this scenario will only happen in log time mess
        if (context.mExactlyOnceCheckpoint) {
            sender->SendCompressed(sendDataVec);
        } else if (mergeType == FlusherSLS::Batch::MergeType::TOPIC
                   && sendDataVec.size() < (size_t)INT32_FLAG(same_topic_merge_send_count)) {
            sender->SendCompressed(sendDataVec);
        } else {
            sender->SendLogPackageList(sendDataVec);
        }
    }
    return true;
}

void Aggregator::MergeTruncateInfo(const sls_logs::LogGroup& logGroup, MergeItem* mergeItem) {
    string truncateTag;
    for (int32_t logTagIdx = 0; logTagIdx < logGroup.logtags_size(); ++logTagIdx) {
        const sls_logs::LogTag& logTag = logGroup.logtags(logTagIdx);
        if (logTag.key() == LOG_RESERVED_KEY_TRUNCATE_INFO) {
            truncateTag = logTag.value();
            break;
        }
    }

    if (truncateTag.empty())
        return;

    int32_t truncateTagIdx = -1;
    for (int32_t logTagIdx = 0; logTagIdx < mergeItem->mLogGroup.logtags_size(); ++logTagIdx) {
        const sls_logs::LogTag& logTag = mergeItem->mLogGroup.logtags(logTagIdx);
        if (logTag.key() == LOG_RESERVED_KEY_TRUNCATE_INFO)
            truncateTagIdx = logTagIdx;
    }
    if (truncateTagIdx == -1) {
        sls_logs::LogTag* logTag = mergeItem->mLogGroup.add_logtags();
        logTag->set_key(LOG_RESERVED_KEY_TRUNCATE_INFO);
        logTag->set_value(truncateTag);
    } else {
        sls_logs::LogTag* logTag = mergeItem->mLogGroup.mutable_logtags(truncateTagIdx);
        logTag->mutable_value()->append(",").append(string(truncateTag));
    }
}

std::string
Aggregator::CalPostRequestShardHashKey(const std::string& source, const std::string& topic, const FlusherSLS* config) {
    if (config == NULL)
        return "";
    uint32_t keySize = config->mBatch.mShardHashKeys.size();
    if (keySize == 0)
        return "";

    string input;
    for (uint32_t idx = 0; idx < keySize; ++idx) {
        const string& key = config->mBatch.mShardHashKeys[idx];
        if (key == LOG_RESERVED_KEY_SOURCE)
            input.append(source);
        else if (key == LOG_RESERVED_KEY_TOPIC)
            input.append(topic);
#ifdef __ENTERPRISE__
        else if (key == LOG_RESERVED_KEY_USER_DEFINED_ID)
            input.append(EnterpriseConfigProvider::GetInstance()->GetUserDefinedIdSet());
#endif
        else if (key == LOG_RESERVED_KEY_MACHINE_UUID)
            input.append(Application::GetInstance()->GetUUID());
        else if (key == LOG_RESERVED_KEY_HOSTNAME)
            input.append(LogFileProfiler::mHostname);

        if (idx != (keySize - 1))
            input.append("_");
    }
    return sdk::CalcMD5(input);
}


void Aggregator::CleanTimeoutLogPackSeq() {
    PTScopedLock lock(mLogPackSeqMapLock);
    int32_t curTime = time(NULL);
    int32_t timeoutInterval = mLogPackSeqMap.size() > 100000 ? 86400 : (86400 * 30);
    std::unordered_map<int64_t, LogPackSeqInfo*>::iterator iter = mLogPackSeqMap.begin();
    for (; iter != mLogPackSeqMap.end();) {
        if ((curTime - iter->second->mLastUpdateTime) > timeoutInterval) {
            delete iter->second;
            iter = mLogPackSeqMap.erase(iter);
        } else
            iter++;
    }
}

void Aggregator::CleanLogPackSeqMap() {
    PTScopedLock lock(mLogPackSeqMapLock);
    for (std::unordered_map<int64_t, LogPackSeqInfo*>::iterator iter = mLogPackSeqMap.begin();
         iter != mLogPackSeqMap.end();
         ++iter)
        delete iter->second;
    mLogPackSeqMap.clear();
}


int64_t Aggregator::GetAndIncLogPackSeq(int64_t key) {
    PTScopedLock lock(mLogPackSeqMapLock);
    std::unordered_map<int64_t, LogPackSeqInfo*>::iterator iter = mLogPackSeqMap.find(key);
    if (iter == mLogPackSeqMap.end()) {
        mLogPackSeqMap[key] = new LogPackSeqInfo(1);
        return 0;
    } else {
        int64_t seq = iter->second->mSeq;
        iter->second->IncPackSeq();
        return seq;
    }
}

bool Aggregator::IsMergeMapEmpty() {
    PTScopedLock lock(mMergeLock);
    if (mMergeMap.size() == 0 && mPackageListMergeMap.size() == 0)
        return true;
    return false;
}

} // namespace logtail
