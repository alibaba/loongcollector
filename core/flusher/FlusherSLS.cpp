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
#include "sdk/Common.h"
#include "sender/PackIdManager.h"
#include "sender/SLSClientManager.h"
#include "sender/Sender.h"

using namespace std;

DEFINE_FLAG_INT32(batch_send_interval, "batch sender interval (second)(default 3)", 3);
DEFINE_FLAG_INT32(merge_log_count_limit, "log count in one logGroup at most", 4000);
DEFINE_FLAG_INT32(batch_send_metric_size, "batch send metric size limit(bytes)(default 256KB)", 256 * 1024);
DEFINE_FLAG_INT32(send_check_real_ip_interval, "seconds", 2);

DECLARE_FLAG_BOOL(send_prefer_real_ip);

namespace logtail {

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
    mLogstoreKey = GenerateLogstoreFeedBackKey(mProject, mLogstore);
    mSenderQueue = Sender::Instance()->GetSenderQueue(mLogstoreKey);
    mSenderQueue->mLimiters.emplace_back(&sRegionConcurrencyLimiter); // TODO: temporary solution

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

    // CompressType
    if (BOOL_FLAG(sls_client_send_compress)) {
        mCompressor = CompressorFactory::GetInstance()->Create(config, *mContext, sName, CompressType::LZ4);
    }

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

    // FlowControlExpireTime
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

    // MaxSendRate
    if (!GetOptionalIntParam(config, "MaxSendRate", mMaxSendRate, errorMsg)) {
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
    Sender::Instance()->SetLogstoreFlowControl(mLogstoreKey, mMaxSendRate, mFlowControlExpireTime);

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

    DefaultFlushStrategyOptions strategy{static_cast<uint32_t>(INT32_FLAG(batch_send_metric_size)),
                                         static_cast<uint32_t>(INT32_FLAG(merge_log_count_limit)),
                                         static_cast<uint32_t>(INT32_FLAG(batch_send_interval))};
    if (!mBatcher.Init(
            itr ? *itr : Json::Value(), this, strategy, !mContext->IsExactlyOnceEnabled() && mShardHashKeys.empty())) {
        // when either exactly once is enabled or ShardHashKeys is not empty, we don't enable group batch
        return false;
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

    mGroupSerializer = make_unique<SLSEventGroupSerializer>(this);
    mGroupListSerializer = make_unique<SLSEventGroupListSerializer>(this);

    GenerateGoPlugin(config, optionalGoPipeline);

    return true;
}

bool FlusherSLS::Register() {
    Sender::Instance()->IncreaseProjectReferenceCnt(mProject);
    Sender::Instance()->IncreaseRegionReferenceCnt(mRegion);
    SLSClientManager::GetInstance()->IncreaseAliuidReferenceCntForRegion(mRegion, mAliuid);
    return true;
}

bool FlusherSLS::Unregister(bool isPipelineRemoving) {
    Sender::Instance()->DecreaseProjectReferenceCnt(mProject);
    Sender::Instance()->DecreaseRegionReferenceCnt(mRegion);
    SLSClientManager::GetInstance()->DecreaseAliuidReferenceCntForRegion(mRegion, mAliuid);
    return true;
}

void FlusherSLS::Send(PipelineEventGroup&& g) {
    if (g.IsReplay()) {
        SerializeAndPush(std::move(g));
    } else {
        vector<BatchedEventsList> res;
        mBatcher.Add(std::move(g), res);
        SerializeAndPush(std::move(res));
    }
}

void FlusherSLS::Flush(size_t key) {
    BatchedEventsList res;
    mBatcher.FlushQueue(key, res);
    SerializeAndPush(std::move(res));
}

// TODO: currently, if sender queue is blocked, data will be lost during pipeline update
// this should be fixed during sender queue refactorization, where all batch should be put into sender queue
void FlusherSLS::FlushAll() {
    vector<BatchedEventsList> res;
    mBatcher.FlushAll(res);
    SerializeAndPush(std::move(res));
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
        auto& exactlyOnceCpt = data->mExactlyOnceCheckpoint;
        const auto& hashKey = exactlyOnceCpt ? exactlyOnceCpt->data.hash_key() : data->mShardHashKey;
        if (hashKey.empty()) {
            return sendClient->CreatePostLogStoreLogsRequest(mProject,
                                                             data->mLogstore,
                                                             ConvertCompressType(GetCompressType()),
                                                             data->mData,
                                                             data->mRawSize,
                                                             sendClosure);
        } else {
            int64_t hashKeySeqID = exactlyOnceCpt ? exactlyOnceCpt->data.sequence_id() : sdk::kInvalidHashKeySeqID;
            return sendClient->CreatePostLogStoreLogsRequest(mProject,
                                                             data->mLogstore,
                                                             ConvertCompressType(GetCompressType()),
                                                             data->mData,
                                                             data->mRawSize,
                                                             sendClosure,
                                                             hashKey,
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
    PushToQueue(std::move(compressedData), data.size(), RawDataType::EVENT_GROUP, logstore);
    return true;
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

void FlusherSLS::SerializeAndPush(PipelineEventGroup&& group) {
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
        return;
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
            return;
        }
    } else {
        compressedData = serializedData;
    }
    PushToQueue(std::move(compressedData),
                serializedData.size(),
                RawDataType::EVENT_GROUP,
                "",
                g.mExactlyOnceCheckpoint->data.hash_key(),
                std::move(g.mExactlyOnceCheckpoint));
}

void FlusherSLS::SerializeAndPush(BatchedEventsList&& groupList) {
    if (groupList.empty()) {
        return;
    }
    vector<CompressedLogGroup> compressedLogGroups;
    string shardHashKey, serializedData, compressedData;
    size_t packageSize = 0;
    bool enablePackageList = groupList.size() > 1;

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
            return;
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
                return;
            }
        } else {
            compressedData = serializedData;
        }
        if (enablePackageList) {
            packageSize += serializedData.size();
            compressedLogGroups.emplace_back(std::move(compressedData), serializedData.size());
        } else {
            if (group.mExactlyOnceCheckpoint) {
                PushToQueue(std::move(compressedData),
                            serializedData.size(),
                            RawDataType::EVENT_GROUP,
                            "",
                            group.mExactlyOnceCheckpoint->data.hash_key(),
                            std::move(group.mExactlyOnceCheckpoint));
            } else {
                PushToQueue(
                    std::move(compressedData), serializedData.size(), RawDataType::EVENT_GROUP, "", shardHashKey);
            }
        }
    }
    if (enablePackageList) {
        string errorMsg;
        mGroupListSerializer->Serialize(std::move(compressedLogGroups), serializedData, errorMsg);
        PushToQueue(std::move(compressedData), packageSize, RawDataType::EVENT_GROUP_LIST);
    }
}

void FlusherSLS::SerializeAndPush(vector<BatchedEventsList>&& groupLists) {
    for (auto& groupList : groupLists) {
        SerializeAndPush(std::move(groupList));
    }
}

string FlusherSLS::GetShardHashKey(const BatchedEvents& g) const {
    // TODO: improve performance
    string key;
    for (size_t i = 0; i < mShardHashKeys.size(); ++i) {
        if (mShardHashKeys[i] == "__source__") {
            key += LogFileProfiler::mIpAddr;
        } else {
            for (auto& item : g.mTags.mInner) {
                if (item.first == mShardHashKeys[i]) {
                    key += item.second.to_string();
                    break;
                }
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

void FlusherSLS::PushToQueue(string&& data,
                             size_t rawSize,
                             RawDataType type,
                             const string& logstore,
                             const string& shardHashKey,
                             RangeCheckpointPtr&& eoo) {
    SLSSenderQueueItem* item = new SLSSenderQueueItem(std::move(data),
                                                      rawSize,
                                                      this,
                                                      eoo ? eoo->fbKey : mLogstoreKey,
                                                      shardHashKey,
                                                      std::move(eoo),
                                                      type,
                                                      eoo ? false : true,
                                                      logstore.empty() ? mLogstore : logstore);
    Sender::Instance()->PutIntoBatchMap(item, mRegion);
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

ConcurrencyLimiter FlusherSLS::sRegionConcurrencyLimiter;

} // namespace logtail
