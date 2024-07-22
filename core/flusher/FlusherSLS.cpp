// Copyright 2023 iLogtail Authors
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

#include "flusher/FlusherSLS.h"

#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif
#include "batch/FlushStrategy.h"
#include "common/EndpointUtil.h"
#include "common/HashUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/ParamExtractor.h"
#include "compression/CompressorFactory.h"
#include "pipeline/Pipeline.h"
#include "queue/QueueKeyManager.h"
#include "queue/SLSSenderQueueItem.h"
#include "queue/SenderQueueManager.h"
#include "sdk/Common.h"
#include "sender/PackIdManager.h"
#include "sender/SLSClientManager.h"
#include "sender/Sender.h"
// TODO: temporarily used here
#include "pipeline/PipelineManager.h"

using namespace std;

DEFINE_FLAG_INT32(batch_send_interval, "batch sender interval (second)(default 3)", 3);
DEFINE_FLAG_INT32(merge_log_count_limit, "log count in one logGroup at most", 4000);
DEFINE_FLAG_INT32(batch_send_metric_size, "batch send metric size limit(bytes)(default 256KB)", 256 * 1024);
DEFINE_FLAG_INT32(send_check_real_ip_interval, "seconds", 2);

DECLARE_FLAG_BOOL(send_prefer_real_ip);

namespace logtail {

mutex FlusherSLS::sMux;
unordered_map<string, weak_ptr<ConcurrencyLimiter>> FlusherSLS::sProjectConcurrencyLimiterMap;
unordered_map<string, weak_ptr<ConcurrencyLimiter>> FlusherSLS::sRegionConcurrencyLimiterMap;

shared_ptr<ConcurrencyLimiter> FlusherSLS::GetProjectConcurrencyLimiter(const string& project) {
    lock_guard<mutex> lock(sMux);
    auto iter = sProjectConcurrencyLimiterMap.find(project);
    if (iter == sProjectConcurrencyLimiterMap.end()) {
        auto limiter = make_shared<ConcurrencyLimiter>();
        sProjectConcurrencyLimiterMap.try_emplace(project, limiter);
        return limiter;
    }
    if (iter->second.expired()) {
        auto limiter = make_shared<ConcurrencyLimiter>();
        iter->second = limiter;
        return limiter;
    }
    return iter->second.lock();
}

shared_ptr<ConcurrencyLimiter> FlusherSLS::GetRegionConcurrencyLimiter(const string& region) {
    lock_guard<mutex> lock(sMux);
    auto iter = sRegionConcurrencyLimiterMap.find(region);
    if (iter == sRegionConcurrencyLimiterMap.end()) {
        auto limiter = make_shared<ConcurrencyLimiter>();
        sRegionConcurrencyLimiterMap.try_emplace(region, limiter);
        return limiter;
    }
    if (iter->second.expired()) {
        auto limiter = make_shared<ConcurrencyLimiter>();
        iter->second = limiter;
        return limiter;
    }
    return iter->second.lock();
}

void FlusherSLS::ClearInvalidConcurrencyLimiters() {
    lock_guard<mutex> lock(sMux);
    for (auto iter = sProjectConcurrencyLimiterMap.begin(); iter != sProjectConcurrencyLimiterMap.end();) {
        if (iter->second.expired()) {
            iter = sProjectConcurrencyLimiterMap.erase(iter);
        } else {
            ++iter;
        }
    }
    for (auto iter = sRegionConcurrencyLimiterMap.begin(); iter != sRegionConcurrencyLimiterMap.end();) {
        if (iter->second.expired()) {
            iter = sRegionConcurrencyLimiterMap.erase(iter);
        } else {
            ++iter;
        }
    }
}

const string FlusherSLS::sName = "flusher_sls";

const unordered_set<string> FlusherSLS::sNativeParam = {"Project",
                                                        "Logstore",
                                                        "Region",
                                                        "Endpoint",
                                                        "Aliuid",
                                                        "CompressType",
                                                        "TelemetryType",
                                                        "FlowControlExpireTime",
                                                        "MaxSendRate",
                                                        "ShardHashKeys",
                                                        "Batch"};

FlusherSLS::FlusherSLS() : mRegion(Sender::Instance()->GetDefaultRegion()) {
}

bool FlusherSLS::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    string errorMsg;

    // Project
    if (!GetMandatoryStringParam(config, "Project", mProject, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    // Logstore
    if (!GetMandatoryStringParam(config, "Logstore", mLogstore, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

#ifdef __ENTERPRISE__
    if (EnterpriseConfigProvider::GetInstance()->IsDataServerPrivateCloud()) {
        mRegion = STRING_FLAG(default_region_name);
    } else {
#endif
        // Region
        if (!GetOptionalStringParam(config, "Region", mRegion, errorMsg)) {
            PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                                  mContext->GetAlarm(),
                                  errorMsg,
                                  mRegion,
                                  sName,
                                  mContext->GetConfigName(),
                                  mContext->GetProjectName(),
                                  mContext->GetLogstoreName(),
                                  mContext->GetRegion());
        }

        // Endpoint
#ifdef __ENTERPRISE__
        if (!GetOptionalStringParam(config, "Endpoint", mEndpoint, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        } else {
#else
    if (!GetMandatoryStringParam(config, "Endpoint", mEndpoint, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    } else {
#endif
            mEndpoint = TrimString(mEndpoint);
            if (!mEndpoint.empty()) {
                SLSClientManager::GetInstance()->AddEndpointEntry(mRegion, StandardizeEndpoint(mEndpoint, mEndpoint));
            }
        }
#ifdef __ENTERPRISE__
    }

    // Aliuid
    if (!GetOptionalStringParam(config, "Aliuid", mAliuid, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }
#endif

    // TelemetryType
    string telemetryType;
    if (!GetOptionalStringParam(config, "TelemetryType", telemetryType, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              "logs",
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    } else if (telemetryType == "metrics") {
        mTelemetryType = TelemetryType::METRIC;
    } else if (!telemetryType.empty() && telemetryType != "logs") {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              "string param TelemetryType is not valid",
                              "logs",
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    // Batch
    const char* key = "Batch";
    const Json::Value* itr = config.find(key, key + strlen(key));
    if (itr) {
        if (!itr->isObject()) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 "param Batch is not of type object",
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }

        // Deprecated, use ShardHashKeys instead
        if (!GetOptionalListParam<string>(*itr, "Batch.ShardHashKeys", mShardHashKeys, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }
    }

    // ShardHashKeys
    if (!GetOptionalListParam<string>(config, "ShardHashKeys", mShardHashKeys, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    } else if (!mShardHashKeys.empty() && mContext->IsExactlyOnceEnabled()) {
        mShardHashKeys.clear();
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             "exactly once enabled when ShardHashKeys is not empty",
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }

    DefaultFlushStrategyOptions strategy{static_cast<uint32_t>(INT32_FLAG(batch_send_metric_size)),
                                         static_cast<uint32_t>(INT32_FLAG(merge_log_count_limit)),
                                         static_cast<uint32_t>(INT32_FLAG(batch_send_interval))};
    if (!mBatcher.Init(
            itr ? *itr : Json::Value(), this, strategy, !mContext->IsExactlyOnceEnabled() && mShardHashKeys.empty())) {
        // when either exactly once is enabled or ShardHashKeys is not empty, we don't enable group batch
        return false;
    }

    // CompressType
    if (BOOL_FLAG(sls_client_send_compress)) {
        mCompressor = CompressorFactory::GetInstance()->Create(config, *mContext, sName, CompressType::LZ4);
    }

    mGroupSerializer = make_unique<SLSEventGroupSerializer>(this);
    mGroupListSerializer = make_unique<SLSEventGroupListSerializer>(this);

    // MaxSendRate
    // For legacy reason, MaxSendRate should be int, where negative number means unlimited. However, this can be
    // compatable with the following logic.
    if (!GetOptionalUIntParam(config, "MaxSendRate", mMaxSendRate, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mMaxSendRate,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    if (!mContext->IsExactlyOnceEnabled()) {
        GenerateQueueKey(mProject + "#" + mLogstore);
        SenderQueueManager::GetInstance()->CreateQueue(
            mQueueKey,
            vector<shared_ptr<ConcurrencyLimiter>>{GetRegionConcurrencyLimiter(mRegion),
                                                   GetProjectConcurrencyLimiter(mProject)},
            mMaxSendRate);
    }

    // (Deprecated) FlowControlExpireTime
    if (!GetOptionalUIntParam(config, "FlowControlExpireTime", mFlowControlExpireTime, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              mFlowControlExpireTime,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }

    GenerateGoPlugin(config, optionalGoPipeline);

    return true;
}

bool FlusherSLS::Start() {
    Sender::Instance()->IncreaseProjectReferenceCnt(mProject);
    Sender::Instance()->IncreaseRegionReferenceCnt(mRegion);
    SLSClientManager::GetInstance()->IncreaseAliuidReferenceCntForRegion(mRegion, mAliuid);
    return true;
}

bool FlusherSLS::Stop(bool isPipelineRemoving) {
    Flusher::Stop(isPipelineRemoving);

    Sender::Instance()->DecreaseProjectReferenceCnt(mProject);
    Sender::Instance()->DecreaseRegionReferenceCnt(mRegion);
    SLSClientManager::GetInstance()->DecreaseAliuidReferenceCntForRegion(mRegion, mAliuid);
    return true;
}

bool FlusherSLS::Send(PipelineEventGroup&& g) {
    if (g.IsReplay()) {
        return SerializeAndPush(std::move(g));
    } else {
        vector<BatchedEventsList> res;
        mBatcher.Add(std::move(g), res);
        return SerializeAndPush(std::move(res));
    }
}

bool FlusherSLS::Flush(size_t key) {
    BatchedEventsList res;
    mBatcher.FlushQueue(key, res);
    return SerializeAndPush(std::move(res));
}

// TODO: currently, if sender queue is blocked, data will be lost during pipeline update
// this should be fixed during sender queue refactorization, where all batch should be put into sender queue
bool FlusherSLS::FlushAll() {
    vector<BatchedEventsList> res;
    mBatcher.FlushAll(res);
    return SerializeAndPush(std::move(res));
}

sdk::AsynRequest* FlusherSLS::BuildRequest(SenderQueueItem* item) const {
    auto data = static_cast<SLSSenderQueueItem*>(item);
    static int32_t lastResetEndpointTime = 0;
    sdk::Client* sendClient = SLSClientManager::GetInstance()->GetClient(mRegion, mAliuid);
    int32_t curTime = time(NULL);

    data->mCurrentEndpoint = sendClient->GetRawSlsHost();
    if (data->mCurrentEndpoint.empty()) {
        if (curTime - lastResetEndpointTime >= 30) {
            SLSClientManager::GetInstance()->ResetClientEndpoint(mAliuid, mRegion, curTime);
            data->mCurrentEndpoint = sendClient->GetRawSlsHost();
            lastResetEndpointTime = curTime;
        }
    }
    if (BOOL_FLAG(send_prefer_real_ip)) {
        if (curTime - sendClient->GetSlsRealIpUpdateTime() >= INT32_FLAG(send_check_real_ip_interval)) {
            SLSClientManager::GetInstance()->UpdateSendClientRealIp(sendClient, mRegion);
        }
        data->mRealIpFlag = sendClient->GetRawSlsHostFlag();
    }

    SendClosure* sendClosure = new SendClosure;
    sendClosure->mDataPtr = item;
    if (data->mType == RawDataType::EVENT_GROUP) {
        if (data->mShardHashKey.empty()) {
            return sendClient->CreatePostLogStoreLogsRequest(mProject,
                                                             data->mLogstore,
                                                             ConvertCompressType(GetCompressType()),
                                                             data->mData,
                                                             data->mRawSize,
                                                             sendClosure);
        } else {
            auto& exactlyOnceCpt = data->mExactlyOnceCheckpoint;
            int64_t hashKeySeqID = exactlyOnceCpt ? exactlyOnceCpt->data.sequence_id() : sdk::kInvalidHashKeySeqID;
            return sendClient->CreatePostLogStoreLogsRequest(mProject,
                                                             data->mLogstore,
                                                             ConvertCompressType(GetCompressType()),
                                                             data->mData,
                                                             data->mRawSize,
                                                             sendClosure,
                                                             data->mShardHashKey,
                                                             hashKeySeqID);
        }
    } else {
        if (data->mShardHashKey.empty())
            return sendClient->CreatePostLogStoreLogPackageListRequest(
                mProject, data->mLogstore, ConvertCompressType(GetCompressType()), data->mData, sendClosure);
        else
            return sendClient->CreatePostLogStoreLogPackageListRequest(mProject,
                                                                       data->mLogstore,
                                                                       ConvertCompressType(GetCompressType()),
                                                                       data->mData,
                                                                       sendClosure,
                                                                       data->mShardHashKey);
    }
}

bool FlusherSLS::Send(string&& data, const string& shardHashKey, const string& logstore) {
    string compressedData;
    if (mCompressor) {
        string errorMsg;
        if (!mCompressor->Compress(data, compressedData, errorMsg)) {
            LOG_WARNING(mContext->GetLogger(),
                        ("failed to compress data",
                         errorMsg)("action", "discard data")("plugin", sName)("config", mContext->GetConfigName()));
            mContext->GetAlarm().SendAlarm(COMPRESS_FAIL_ALARM,
                                           "failed to compress data: " + errorMsg + "\taction: discard data\tplugin: "
                                               + sName + "\tconfig: " + mContext->GetConfigName(),
                                           mContext->GetProjectName(),
                                           mContext->GetLogstoreName(),
                                           mContext->GetRegion());
            return false;
        }
    } else {
        compressedData = data;
    }
    return Flusher::PushToQueue(make_unique<SLSSenderQueueItem>(
        std::move(compressedData), data.size(), this, mQueueKey, logstore, RawDataType::EVENT_GROUP, shardHashKey));
}

void FlusherSLS::GenerateGoPlugin(const Json::Value& config, Json::Value& res) const {
    Json::Value detail(Json::objectValue);
    for (auto itr = config.begin(); itr != config.end(); ++itr) {
        if (sNativeParam.find(itr.name()) == sNativeParam.end() && itr.name() != "Type") {
            detail[itr.name()] = *itr;
        }
    }
    if (!detail.empty()) {
        Json::Value plugin(Json::objectValue);
        plugin["type"] = "flusher_sls";
        plugin["detail"] = detail;
        res["flushers"].append(plugin);
    }
}

bool FlusherSLS::SerializeAndPush(PipelineEventGroup&& group) {
    string serializedData, compressedData;
    BatchedEvents g(std::move(group.MutableEvents()),
                    std::move(group.GetSizedTags()),
                    std::move(group.GetSourceBuffer()),
                    group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                    std::move(group.GetExactlyOnceCheckpoint()));
    AddPackId(g);
    string errorMsg;
    if (!mGroupSerializer->Serialize(std::move(g), serializedData, errorMsg)) {
        LOG_WARNING(mContext->GetLogger(),
                    ("failed to serialize event group",
                     errorMsg)("action", "discard data")("plugin", sName)("config", mContext->GetConfigName()));
        mContext->GetAlarm().SendAlarm(SERIALIZE_FAIL_ALARM,
                                       "failed to serialize event group: " + errorMsg
                                           + "\taction: discard data\tplugin: " + sName
                                           + "\tconfig: " + mContext->GetConfigName(),
                                       mContext->GetProjectName(),
                                       mContext->GetLogstoreName(),
                                       mContext->GetRegion());
        return false;
    }
    if (mCompressor) {
        if (!mCompressor->Compress(serializedData, compressedData, errorMsg)) {
            LOG_WARNING(mContext->GetLogger(),
                        ("failed to compress event group",
                         errorMsg)("action", "discard data")("plugin", sName)("config", mContext->GetConfigName()));
            mContext->GetAlarm().SendAlarm(COMPRESS_FAIL_ALARM,
                                           "failed to compress event group: " + errorMsg
                                               + "\taction: discard data\tplugin: " + sName
                                               + "\tconfig: " + mContext->GetConfigName(),
                                           mContext->GetProjectName(),
                                           mContext->GetLogstoreName(),
                                           mContext->GetRegion());
            return false;
        }
    } else {
        compressedData = serializedData;
    }
    // must create a tmp, because eoo checkpoint is moved in second param
    auto fbKey = g.mExactlyOnceCheckpoint->fbKey;
    return PushToQueue(fbKey,
                       make_unique<SLSSenderQueueItem>(std::move(compressedData),
                                                       serializedData.size(),
                                                       this,
                                                       fbKey,
                                                       mLogstore,
                                                       RawDataType::EVENT_GROUP,
                                                       g.mExactlyOnceCheckpoint->data.hash_key(),
                                                       std::move(g.mExactlyOnceCheckpoint),
                                                       false));
}

bool FlusherSLS::SerializeAndPush(BatchedEventsList&& groupList) {
    if (groupList.empty()) {
        return true;
    }
    vector<CompressedLogGroup> compressedLogGroups;
    string shardHashKey, serializedData, compressedData;
    size_t packageSize = 0;
    bool enablePackageList = groupList.size() > 1;

    bool allSucceeded = true;
    for (auto& group : groupList) {
        if (!mShardHashKeys.empty()) {
            shardHashKey = GetShardHashKey(group);
        }
        AddPackId(group);
        string errorMsg;
        if (!mGroupSerializer->Serialize(std::move(group), serializedData, errorMsg)) {
            LOG_WARNING(mContext->GetLogger(),
                        ("failed to serialize event group",
                         errorMsg)("action", "discard data")("plugin", sName)("config", mContext->GetConfigName()));
            mContext->GetAlarm().SendAlarm(SERIALIZE_FAIL_ALARM,
                                           "failed to serialize event group: " + errorMsg
                                               + "\taction: discard data\tplugin: " + sName
                                               + "\tconfig: " + mContext->GetConfigName(),
                                           mContext->GetProjectName(),
                                           mContext->GetLogstoreName(),
                                           mContext->GetRegion());
            allSucceeded = false;
            continue;
        }
        if (mCompressor) {
            if (!mCompressor->Compress(serializedData, compressedData, errorMsg)) {
                LOG_WARNING(mContext->GetLogger(),
                            ("failed to compress event group",
                             errorMsg)("action", "discard data")("plugin", sName)("config", mContext->GetConfigName()));
                mContext->GetAlarm().SendAlarm(COMPRESS_FAIL_ALARM,
                                               "failed to compress event group: " + errorMsg
                                                   + "\taction: discard data\tplugin: " + sName
                                                   + "\tconfig: " + mContext->GetConfigName(),
                                               mContext->GetProjectName(),
                                               mContext->GetLogstoreName(),
                                               mContext->GetRegion());
                allSucceeded = false;
                continue;
            }
        } else {
            compressedData = serializedData;
        }
        if (enablePackageList) {
            packageSize += serializedData.size();
            compressedLogGroups.emplace_back(std::move(compressedData), serializedData.size());
        } else {
            if (group.mExactlyOnceCheckpoint) {
                // must create a tmp, because eoo checkpoint is moved in second param
                auto fbKey = group.mExactlyOnceCheckpoint->fbKey;
                allSucceeded
                    = PushToQueue(fbKey,
                                  make_unique<SLSSenderQueueItem>(std::move(compressedData),
                                                                  serializedData.size(),
                                                                  this,
                                                                  fbKey,
                                                                  mLogstore,
                                                                  RawDataType::EVENT_GROUP,
                                                                  group.mExactlyOnceCheckpoint->data.hash_key(),
                                                                  std::move(group.mExactlyOnceCheckpoint),
                                                                  false))
                    && allSucceeded;
            } else {
                allSucceeded = Flusher::PushToQueue(make_unique<SLSSenderQueueItem>(std::move(compressedData),
                                                                                    serializedData.size(),
                                                                                    this,
                                                                                    mQueueKey,
                                                                                    mLogstore,
                                                                                    RawDataType::EVENT_GROUP,
                                                                                    shardHashKey))
                    && allSucceeded;
            }
        }
    }
    if (enablePackageList) {
        string errorMsg;
        mGroupListSerializer->Serialize(std::move(compressedLogGroups), serializedData, errorMsg);
        allSucceeded
            = Flusher::PushToQueue(make_unique<SLSSenderQueueItem>(
                  std::move(serializedData), packageSize, this, mQueueKey, mLogstore, RawDataType::EVENT_GROUP_LIST))
            && allSucceeded;
    }
    return allSucceeded;
}

bool FlusherSLS::SerializeAndPush(vector<BatchedEventsList>&& groupLists) {
    bool allSucceeded = true;
    for (auto& groupList : groupLists) {
        allSucceeded = SerializeAndPush(std::move(groupList)) && allSucceeded;
    }
    return allSucceeded;
}

bool FlusherSLS::PushToQueue(QueueKey key, unique_ptr<SenderQueueItem>&& item, uint32_t retryTimes) {
#ifndef APSARA_UNIT_TEST_MAIN
    // TODO: temporarily set here, should be removed after independent config update refactor
    if (item->mFlusher->HasContext()) {
        item->mPipeline
            = PipelineManager::GetInstance()->FindPipelineByName(item->mFlusher->GetContext().GetConfigName());
        if (!item->mPipeline) {
            // should not happen
            return false;
        }
    }
#endif

    const string& str = QueueKeyManager::GetInstance()->GetName(key);
    for (size_t i = 0; i < retryTimes; ++i) {
        int rst = SenderQueueManager::GetInstance()->PushQueue(key, std::move(item));
        if (rst == 0) {
            return true;
        }
        if (rst == 2) {
            // should not happen
            LOG_ERROR(sLogger,
                      ("failed to push data to sender queue",
                       "queue not found")("action", "discard data")("config-flusher-dst", str));
            LogtailAlarm::GetInstance()->SendAlarm(
                DISCARD_DATA_ALARM,
                "failed to push data to sender queue: queue not found\taction: discard data\tconfig-flusher-dst" + str);
            return false;
        }
        if (i % 100 == 0) {
            LOG_WARNING(sLogger,
                        ("push attempts to sender queue continuously failed for the past second",
                         "retry again")("config-flusher-dst", str));
        }
        this_thread::sleep_for(chrono::milliseconds(10));
    }
    LOG_WARNING(
        sLogger,
        ("failed to push data to sender queue", "queue full")("action", "discard data")("config-flusher-dst", str));
    LogtailAlarm::GetInstance()->SendAlarm(
        DISCARD_DATA_ALARM,
        "failed to push data to sender queue: queue full\taction: discard data\tconfig-flusher-dst" + str);
    return false;
}

string FlusherSLS::GetShardHashKey(const BatchedEvents& g) const {
    // TODO: improve performance
    string key;
    for (size_t i = 0; i < mShardHashKeys.size(); ++i) {
        for (auto& item : g.mTags.mInner) {
            if (item.first == mShardHashKeys[i]) {
                key += item.second.to_string();
                break;
            }
        }
        if (i != mShardHashKeys.size() - 1) {
            key += "_";
        }
    }
    return sdk::CalcMD5(key);
}

void FlusherSLS::AddPackId(BatchedEvents& g) const {
    string packIdPrefixStr = g.mPackIdPrefix.to_string();
    int64_t packidPrefix = HashString(packIdPrefixStr);
    int64_t packSeq = PackIdManager::GetInstance()->GetAndIncPackSeq(
        HashString(packIdPrefixStr + "_" + mProject + "_" + mLogstore));
    auto packId = g.mSourceBuffers[0]->CopyString(ToHexString(packidPrefix) + "-" + ToHexString(packSeq));
    g.mTags.Insert(LOG_RESERVED_KEY_PACKAGE_ID, StringView(packId.data, packId.size));
}

sls_logs::SlsCompressType ConvertCompressType(CompressType type) {
    sls_logs::SlsCompressType compressType = sls_logs::SLS_CMP_NONE;
    switch (type) {
        case CompressType::LZ4:
            compressType = sls_logs::SLS_CMP_LZ4;
            break;
        case CompressType::ZSTD:
            compressType = sls_logs::SLS_CMP_ZSTD;
            break;
        default:
            break;
    }
    return compressType;
}

} // namespace logtail
