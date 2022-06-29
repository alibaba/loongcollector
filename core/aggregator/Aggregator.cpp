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

#include <processor/LogFilter.h>
#include <profiler/LogFileProfiler.h>
#include <common/Constants.h>
#include <config_manager/ConfigManager.h>
#include <common/StringTools.h>
#include <common/HashUtil.h>
#include "Aggregator.h"
#include "common/LogtailCommonFlags.h"
#include "sender/Sender.h"
#include "config/Config.h"
#include <app_config/AppConfig.h>

using namespace std;
using namespace sls_logs;

DECLARE_FLAG_INT32(merge_log_count_limit);
DECLARE_FLAG_INT32(same_topic_merge_send_count);
DECLARE_FLAG_INT32(max_send_log_group_size);

namespace logtail {


bool MergeItem::IsReady() {
    return (mRawBytes > INT32_FLAG(batch_send_metric_size) || ((time(NULL) - mLastUpdateTime) >= mBatchSendInterval));
}

bool PackageListMergeBuffer::IsReady(int32_t curTime) {
    // should use 2 * INT32_FLAG(batch_send_interval)), package list interval should > merge item interval
    return (mTotalRawBytes >= INT32_FLAG(batch_send_metric_size))
        || (mItemCount > 0 && (curTime - mFirstItemTime) >= 2 * INT32_FLAG(batch_send_interval));
}


bool Aggregator::FlushReadyBuffer() {
    static Sender* sender = Sender::Instance();
    vector<MergeItem*> sendDataVec;
    {
        PTScopedLock lock(mMergeLock);
        unordered_map<int64_t, MergeItem*>::iterator itr = mMergeMap.begin();
        for (; itr != mMergeMap.end();) {
            if (sender->IsFlush()
                || (itr->second->IsReady()
                    && sender->GetSenderFeedBackInterface()->IsValidToPush(itr->second->mLogstoreKey))) {
                if (itr->second->mMergeType == MERGE_BY_TOPIC)
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
            if (sender->IsFlush()
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
        sender->SendLZ4Compressed(sendDataVec);

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
                     const Config* config,
                     DATA_MERGE_TYPE mergeType,
                     const uint32_t logGroupSize,
                     const std::string& defaultRegion,
                     const std::string& filename,
                     const LogGroupContext& context) {
    if ((logGroupSize == 0 && logGroup.ByteSize() > INT32_FLAG(max_send_log_group_size))
        || (int32_t)logGroupSize > INT32_FLAG(max_send_log_group_size)) {
        LOG_ERROR(sLogger, ("invalid log group size", logGroupSize)("real size", logGroup.ByteSize()));
        return false;
    }

    int32_t logSize = (int32_t)logGroup.logs_size();
    if (logSize == 0)
        return true;
    static const vector<sls_logs::LogTag>& sEnvTags = AppConfig::GetInstance()->GetEnvTags();
    if (!sEnvTags.empty()) {
        for (size_t i = 0; i < sEnvTags.size(); ++i) {
            sls_logs::LogTag* logTagPtr = logGroup.add_logtags();
            logTagPtr->set_key(sEnvTags[i].key());
            logTagPtr->set_value(sEnvTags[i].value());
        }
    }
    vector<int32_t> neededLogs;
    int32_t neededLogSize = FilterNoneUtf8Metric(logGroup, config, neededLogs, context);
    if (neededLogSize == 0)
        return true;
    if (config != NULL && config->mSensitiveWordCastOptions.size() > (size_t)0) {
        LogFilter::CastSensitiveWords(logGroup, config);
    }

    static Sender* sender = Sender::Instance();
    const string& region = (config == NULL ? defaultRegion : config->mRegion);
    const string& aliuid = (config == NULL ? STRING_FLAG(logtail_profile_aliuid) : config->mAliuid);
    const string& configName = (config == NULL ? "" : config->mConfigName);
    const string& category = logGroup.category();
    const string& topic = logGroup.topic();
    const string& source = logGroup.has_source() ? logGroup.source() : LogFileProfiler::mIpAddr;
    string shardHashKey = CalPostRequestShardHashKey(source, topic, config);
    //now shardHashKey is compute using machine level fields, so logGroupKey will not contain shardHashKey
    int64_t logGroupKey
        = HashString(projectName + "_" + category + "_" + topic + "_" + source + "_"
                     + ((config != NULL && config->mLogType != STREAM_LOG && config->mLogType != PLUGIN_LOG)
                            ? (config->mBasePath + config->mFilePattern)
                            : "")
                     + "_" + sourceId);

    // Replay checkpoint had already been merged, resend directly.
    if (context.mExactlyOnceCheckpoint && context.mExactlyOnceCheckpoint->IsComplete()) {
        AddPackIDForLogGroup(sourceId, logGroupKey, logGroup);
        sender->SendLZ4Compressed(projectName, logGroup, neededLogs, configName, aliuid, region, filename, context);
        LOG_DEBUG(sLogger,
                  ("complete checkpoint", "resend directly")("filename", filename)(
                      "key", context.mExactlyOnceCheckpoint->key)("checkpoint",
                                                                  context.mExactlyOnceCheckpoint->data.DebugString()));
        return true;
    }

    LogstoreFeedBackKey feedBackKey
        = config == NULL ? GenerateLogstoreFeedBackKey(projectName, category) : config->mLogstoreKey;
    int64_t key, logstoreKey;
    if (mergeType == MERGE_BY_LOGSTORE) {
        logstoreKey = HashString(projectName + "_" + category);
        key = logstoreKey;
    } else {
        key = logGroupKey;
    }

    LogGroup discardLogGroup;
    vector<MergeItem*> sendDataVec;
    int32_t logByteSize = (logGroupSize == 0 ? logGroup.ByteSize() : logGroupSize) / logSize;
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
        unordered_map<int64_t, PackageListMergeBuffer*>::iterator pIter;
        if (mergeType == MERGE_BY_LOGSTORE) {
            pIter = mPackageListMergeMap.find(logstoreKey);
            if (pIter == mPackageListMergeMap.end()) {
                PackageListMergeBuffer* tmpPtr = new PackageListMergeBuffer();
                pIter = mPackageListMergeMap.insert(std::make_pair(logstoreKey, tmpPtr)).first;
            }
        }
        unordered_map<int64_t, MergeItem*>::iterator itr = mMergeMap.find(logGroupKey);
        MergeItem* value = NULL;
        if (itr != mMergeMap.end())
            value = itr->second;
        else
            itr = mMergeMap.insert(std::make_pair(logGroupKey, value)).first;

        bool mergeFinishedFlag = false, initFlag = false;
        for (int32_t logIdx = 0; logIdx < logSize; logIdx++) {
            if (neededIdx < neededLogSize && logIdx == neededLogs[neededIdx]) {
                if (value == NULL || value->mLines > logCountMin || value->mRawBytes > logGroupByteMin
                    || (curTime - value->mLastUpdateTime) >= INT32_FLAG(batch_send_interval)
                    || ((value->mLogGroup).logs(0).time() / 60 != (*(mutableLogPtr + logIdx))->time() / 60)) {
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

                        if (mergeType == MERGE_BY_LOGSTORE)
                            pIter->second->AddMergeItem(value);
                        else
                            sendDataVec.push_back(value);
                    }

                    bool bufferOrNot = config == NULL ? BOOL_FLAG(default_secondary_storage) : config->mLocalStorage;
                    int32_t batchSendInterval
                        = config == NULL ? INT32_FLAG(batch_send_interval) : config->mAdvancedConfig.mBatchSendInterval;
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
                    itr->second = value;
                    initFlag = true;

                    value->mLastUpdateTime = curTime;
                    (value->mLogGroup).mutable_logs()->Reserve(INT32_FLAG(merge_log_count_limit));
                    (value->mLogGroup).set_category(category);
                    (value->mLogGroup).set_topic(topic);
                    (value->mLogGroup).set_machineuuid(ConfigManager::GetInstance()->GetUUID());
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
                    auto& logPosition = context.mExactlyOnceCheckpoint->positions[logIdx];
                    auto& cpt = value->mLogGroupContext.mExactlyOnceCheckpoint->data;

                    // First log, upodate read_offset.
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
            } else
                discardLogGroup.mutable_logs()->AddAllocated(*(mutableLogPtr + logIdx));
        }

        // handle truncate info, the first truncate info may be inserted while merge item 'value' initialized
        if (context.mFuseMode) {
            MergeTruncateInfo(logGroup, value);
        }
        if (context.mMarkOffsetFlag && !mergeFinishedFlag && !initFlag) {
            value->mLogGroupContext.mFileInfoPtr = context.mFileInfoPtr;
        }

        for (int32_t logIdx = 0; logIdx < logSize; logIdx++)
            logGroup.mutable_logs()->ReleaseLast();
        if (value != NULL && (value->IsReady() || sender->IsFlush())) {
            if (mergeType == MERGE_BY_LOGSTORE)
                (pIter->second)->AddMergeItem(value);
            else
                sendDataVec.push_back(value);

            mMergeMap.erase(itr);
        }
        if (mergeType == MERGE_BY_LOGSTORE) {
            if (pIter->second->IsReady(curTime) || sender->IsFlush()) {
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
        }
    }

    if (sendDataVec.size() > 0) {
        // if send data package count greater than INT32_FLAG(same_topic_merge_send_count), there will be many little package in send queue, so we merge data package to send.
        // because the send loggroup's max size is 512k, so this scenario will only happen in log time mess
        if (context.mExactlyOnceCheckpoint) {
            sender->SendLZ4Compressed(sendDataVec);
        } else if (mergeType == MERGE_BY_TOPIC && sendDataVec.size() < (size_t)INT32_FLAG(same_topic_merge_send_count))
            sender->SendLZ4Compressed(sendDataVec);
        else
            sender->SendLogPackageList(sendDataVec);
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
Aggregator::CalPostRequestShardHashKey(const std::string& source, const std::string& topic, const Config* config) {
    if (config == NULL)
        return "";
    uint32_t keySize = config->mShardHashKey.size();
    if (keySize == 0)
        return "";

    string input;
    for (uint32_t idx = 0; idx < keySize; ++idx) {
        const string& key = config->mShardHashKey[idx];
        if (key == LOG_RESERVED_KEY_SOURCE)
            input.append(source);
        else if (key == LOG_RESERVED_KEY_TOPIC)
            input.append(topic);
        else if (key == LOG_RESERVED_KEY_USER_DEFINED_ID)
            input.append(ConfigManager::GetInstance()->GetUserDefinedIdSet());
        else if (key == LOG_RESERVED_KEY_MACHINE_UUID)
            input.append(ConfigManager::GetInstance()->GetUUID());
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

int32_t Aggregator::FilterNoneUtf8Metric(sls_logs::LogGroup& logGroup,
                                         const Config* config,
                                         std::vector<int32_t>& neededLogs,
                                         const LogGroupContext& context) {
    static LogFilter* filterPtr = LogFilter::Instance();
    if (config != NULL && config->mAdvancedConfig.mFilterExpressionRoot.get() != NULL) {
        neededLogs = filterPtr->Filter(logGroup, config->mAdvancedConfig.mFilterExpressionRoot, context);
    } else if (config != NULL && config->mFilterRule) {
        neededLogs = filterPtr->Filter(logGroup, config->mFilterRule.get(), context);
    } else {
        neededLogs = filterPtr->Filter(context.mProjectName, context.mRegion, logGroup);
    }
    int32_t neededLogSize = (int32_t)neededLogs.size();
    if (neededLogSize > 0 && config != NULL && (config->mLogType != STREAM_LOG && config->mLogType != PLUGIN_LOG)
        && config->mDiscardNoneUtf8) {
        for (int32_t i = 0; i < neededLogSize; ++i) {
            const Log& log = logGroup.logs(neededLogs[i]);
            for (int j = 0; j < log.contents_size(); ++j) {
                FilterNoneUtf8(log.contents(j).key());
                FilterNoneUtf8(log.contents(j).value());
            }
        }
    }
    return neededLogSize;
}

bool Aggregator::IsMergeMapEmpty() {
    PTScopedLock lock(mMergeLock);
    if (mMergeMap.size() == 0 && mPackageListMergeMap.size() == 0)
        return true;
    return false;
}

} // namespace logtail
