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

#include "flusher/sls/FlusherSLS.h"

#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif
#include "app_config/AppConfig.h"
#include "batch/FlushStrategy.h"
#include "common/EndpointUtil.h"
#include "common/HashUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/ParamExtractor.h"
#include "common/TimeUtil.h"
#include "compression/CompressorFactory.h"
#include "flusher/sls/PackIdManager.h"
#include "flusher/sls/SLSClientManager.h"
#include "flusher/sls/SLSResponse.h"
#include "flusher/sls/SendResult.h"
#include "pipeline/Pipeline.h"
#include "profile_sender/ProfileSender.h"
#include "queue/QueueKeyManager.h"
#include "queue/SLSSenderQueueItem.h"
#include "queue/SenderQueueManager.h"
#include "sdk/Common.h"
#include "sender/FlusherRunner.h"
#include "sls_control/SLSControl.h"
// TODO: temporarily used here
#include "flusher/sls/DiskBufferWriter.h"
#include "pipeline/PipelineManager.h"

using namespace std;

DEFINE_FLAG_INT32(batch_send_interval, "batch sender interval (second)(default 3)", 3);
DEFINE_FLAG_INT32(merge_log_count_limit, "log count in one logGroup at most", 4000);
DEFINE_FLAG_INT32(batch_send_metric_size, "batch send metric size limit(bytes)(default 256KB)", 256 * 1024);
DEFINE_FLAG_INT32(send_check_real_ip_interval, "seconds", 2);

DEFINE_FLAG_INT32(unauthorized_send_retrytimes,
                  "how many times should retry if PostLogStoreLogs operation return UnAuthorized",
                  5);
DEFINE_FLAG_INT32(unauthorized_allowed_delay_after_reset, "allowed delay to retry for unauthorized error, 30s", 30);
DEFINE_FLAG_INT32(discard_send_fail_interval, "discard data when send fail after 6 * 3600 seconds", 6 * 3600);
DEFINE_FLAG_INT32(profile_data_send_retrytimes, "how many times should retry if profile data send fail", 5);
DEFINE_FLAG_INT32(unknow_error_try_max, "discard data when try times > this value", 5);
DEFINE_FLAG_BOOL(global_network_success, "global network success flag, default false", false);

DECLARE_FLAG_BOOL(send_prefer_real_ip);

namespace logtail {

enum class OperationOnFail { RETRY_IMMEDIATELY, RETRY_LATER, DISCARD };

static const int ON_FAIL_LOG_WARNING_INTERVAL_SECOND = 10;

static const char* GetOperationString(OperationOnFail op) {
    switch (op) {
        case OperationOnFail::RETRY_IMMEDIATELY:
            return "retry now";
        case OperationOnFail::RETRY_LATER:
            return "retry later";
        case OperationOnFail::DISCARD:
        default:
            return "discard data";
    }
}

static OperationOnFail DefaultOperation(uint32_t retryTimes) {
    if (retryTimes == 1) {
        return OperationOnFail::RETRY_IMMEDIATELY;
    } else if (retryTimes > static_cast<uint32_t>(INT32_FLAG(unknow_error_try_max))) {
        return OperationOnFail::DISCARD;
    } else {
        return OperationOnFail::RETRY_LATER;
    }
}

void FlusherSLS::InitResource() {
#ifndef APSARA_UNIT_TEST_MAIN
    if (!sIsResourceInited) {
        SLSControl::GetInstance()->Init();
        SLSClientManager::GetInstance()->Init();
        DiskBufferWriter::GetInstance()->Init();
        sIsResourceInited = true;
    }
#endif
}

void FlusherSLS::RecycleResourceIfNotUsed() {
#ifndef APSARA_UNIT_TEST_MAIN
    if (sIsResourceInited) {
        SLSClientManager::GetInstance()->Stop();
        DiskBufferWriter::GetInstance()->Stop();
    }
#endif
}

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

mutex FlusherSLS::sDefaultRegionLock;
string FlusherSLS::sDefaultRegion;

string FlusherSLS::GetDefaultRegion() {
    lock_guard<mutex> lock(sDefaultRegionLock);
    if (sDefaultRegion.empty()) {
        sDefaultRegion = STRING_FLAG(default_region_name);
    }
    return sDefaultRegion;
}

void FlusherSLS::SetDefaultRegion(const string& region) {
    lock_guard<mutex> lock(sDefaultRegionLock);
    sDefaultRegion = region;
}

mutex FlusherSLS::sProjectRefCntMapLock;
unordered_map<string, int32_t> FlusherSLS::sProjectRefCntMap;
mutex FlusherSLS::sRegionRefCntMapLock;
unordered_map<string, int32_t> FlusherSLS::sRegionRefCntMap;

string FlusherSLS::GetAllProjects() {
    string result;
    lock_guard<mutex> lock(sProjectRefCntMapLock);
    for (auto iter = sProjectRefCntMap.cbegin(); iter != sProjectRefCntMap.cend(); ++iter) {
        result.append(iter->first).append(" ");
    }
    return result;
}

void FlusherSLS::IncreaseProjectReferenceCnt(const string& project) {
    lock_guard<mutex> lock(sProjectRefCntMapLock);
    ++sProjectRefCntMap[project];
}

void FlusherSLS::DecreaseProjectReferenceCnt(const string& project) {
    lock_guard<mutex> lock(sProjectRefCntMapLock);
    auto iter = sProjectRefCntMap.find(project);
    if (iter == sProjectRefCntMap.end()) {
        // should not happen
        return;
    }
    if (--iter->second == 0) {
        sProjectRefCntMap.erase(iter);
    }
}

bool FlusherSLS::IsRegionContainingConfig(const string& region) {
    lock_guard<mutex> lock(sRegionRefCntMapLock);
    return sRegionRefCntMap.find(region) != sRegionRefCntMap.end();
}

void FlusherSLS::IncreaseRegionReferenceCnt(const string& region) {
    lock_guard<mutex> lock(sRegionRefCntMapLock);
    ++sRegionRefCntMap[region];
}

void FlusherSLS::DecreaseRegionReferenceCnt(const string& region) {
    lock_guard<mutex> lock(sRegionRefCntMapLock);
    auto iter = sRegionRefCntMap.find(region);
    if (iter == sRegionRefCntMap.end()) {
        // should not happen
        return;
    }
    if (--iter->second == 0) {
        sRegionRefCntMap.erase(iter);
    }
}

mutex FlusherSLS::sRegionStatusLock;
unordered_map<string, bool> FlusherSLS::sAllRegionStatus;

void FlusherSLS::UpdateRegionStatus(const string& region, bool status) {
    LOG_DEBUG(sLogger, ("update region status, region", region)("is network in good condition", ToString(status)));
    lock_guard<mutex> lock(sRegionStatusLock);
    sAllRegionStatus[region] = status;
}

bool FlusherSLS::GetRegionStatus(const string& region) {
    lock_guard<mutex> lock(sRegionStatusLock);
    auto rst = sAllRegionStatus.find(region);
    if (rst == sAllRegionStatus.end()) {
        return true;
    } else {
        return rst->second;
    }
}

bool FlusherSLS::sIsResourceInited = false;

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

FlusherSLS::FlusherSLS() : mRegion(GetDefaultRegion()) {
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
    InitResource();

    IncreaseProjectReferenceCnt(mProject);
    IncreaseRegionReferenceCnt(mRegion);
    SLSClientManager::GetInstance()->IncreaseAliuidReferenceCntForRegion(mRegion, mAliuid);
    return true;
}

bool FlusherSLS::Stop(bool isPipelineRemoving) {
    Flusher::Stop(isPipelineRemoving);

    DecreaseProjectReferenceCnt(mProject);
    DecreaseRegionReferenceCnt(mRegion);
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

bool FlusherSLS::FlushAll() {
    vector<BatchedEventsList> res;
    mBatcher.FlushAll(res);
    return SerializeAndPush(std::move(res));
}

unique_ptr<HttpSinkRequest> FlusherSLS::BuildRequest(SenderQueueItem* item) const {
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

    if (data->mType == RawDataType::EVENT_GROUP) {
        if (data->mShardHashKey.empty()) {
            return sendClient->CreatePostLogStoreLogsRequest(
                mProject, data->mLogstore, ConvertCompressType(GetCompressType()), data->mData, data->mRawSize, item);
        } else {
            auto& exactlyOnceCpt = data->mExactlyOnceCheckpoint;
            int64_t hashKeySeqID = exactlyOnceCpt ? exactlyOnceCpt->data.sequence_id() : sdk::kInvalidHashKeySeqID;
            return sendClient->CreatePostLogStoreLogsRequest(mProject,
                                                             data->mLogstore,
                                                             ConvertCompressType(GetCompressType()),
                                                             data->mData,
                                                             data->mRawSize,
                                                             item,
                                                             data->mShardHashKey,
                                                             hashKeySeqID);
        }
    } else {
        if (data->mShardHashKey.empty())
            return sendClient->CreatePostLogStoreLogPackageListRequest(
                mProject, data->mLogstore, ConvertCompressType(GetCompressType()), data->mData, item);
        else
            return sendClient->CreatePostLogStoreLogPackageListRequest(mProject,
                                                                       data->mLogstore,
                                                                       ConvertCompressType(GetCompressType()),
                                                                       data->mData,
                                                                       item,
                                                                       data->mShardHashKey);
    }
}

void FlusherSLS::OnSendDone(const HttpResponse& response, SenderQueueItem* item) {
    SLSResponse slsResponse;
    if (AppConfig::GetInstance()->IsResponseVerificationEnabled() && !IsSLSResponse(response)) {
        slsResponse.mStatusCode = 0;
        slsResponse.mErrorCode = sdk::LOGE_REQUEST_ERROR;
        slsResponse.mErrorMsg = "invalid response body";
    } else {
        slsResponse.Parse(response);

        if (AppConfig::GetInstance()->EnableLogTimeAutoAdjust()) {
            static uint32_t sCount = 0;
            if (sCount++ % 10000 == 0 || slsResponse.mErrorCode == sdk::LOGE_REQUEST_TIME_EXPIRED) {
                time_t serverTime = GetServerTime(response);
                if (serverTime > 0) {
                    UpdateTimeDelta(serverTime);
                }
            }
        }
    }

    auto data = static_cast<SLSSenderQueueItem*>(item);
    string configName = HasContext() ? GetContext().GetConfigName() : "";
    bool isProfileData = ProfileSender::GetInstance()->IsProfileData(mRegion, mProject, data->mLogstore);
    int32_t curTime = time(NULL);
    if (slsResponse.mStatusCode == 200) {
        auto& cpt = data->mExactlyOnceCheckpoint;
        if (cpt) {
            cpt->Commit();
            cpt->IncreaseSequenceID();
        }

        GetRegionConcurrencyLimiter(mRegion)->OnSuccess();
        DealSenderQueueItemAfterSend(item, false);
        LOG_DEBUG(sLogger,
                  ("send data to sls succeeded, item address", item)("request id", slsResponse.mRequestId)(
                      "config", configName)("region", mRegion)("project", mProject)("logstore", data->mLogstore)(
                      "response time", curTime - data->mLastSendTime)("total send time", curTime - data->mEnqueTime)(
                      "try cnt", data->mTryCnt)("endpoint", data->mCurrentEndpoint)("is profile data", isProfileData));
    } else {
        OperationOnFail operation;
        SendResult sendResult = ConvertErrorCode(slsResponse.mErrorCode);
        ostringstream failDetail, suggestion;
        string failEndpoint = data->mCurrentEndpoint;
        if (sendResult == SEND_NETWORK_ERROR || sendResult == SEND_SERVER_ERROR) {
            if (sendResult == SEND_NETWORK_ERROR) {
                failDetail << "network error";
            } else {
                failDetail << "server error";
            }
            suggestion << "check network connection to endpoint";
            if (BOOL_FLAG(send_prefer_real_ip) && data->mRealIpFlag) {
                // connect refused, use vip directly
                failDetail << ", real ip may be stale, force update";
                // just set force update flag
                SLSClientManager::GetInstance()->ForceUpdateRealIp(mRegion);
            }
            if (sendResult == SEND_NETWORK_ERROR) {
                // only set network stat when no real ip
                if (!BOOL_FLAG(send_prefer_real_ip) || !data->mRealIpFlag) {
                    SLSClientManager::GetInstance()->UpdateEndpointStatus(mRegion, data->mCurrentEndpoint, false);
                    if (SLSClientManager::GetInstance()->GetServerSwitchPolicy()
                        == SLSClientManager::EndpointSwitchPolicy::DESIGNATED_FIRST) {
                        SLSClientManager::GetInstance()->ResetClientEndpoint(mAliuid, mRegion, curTime);
                    }
                }
            }
            operation = data->mBufferOrNot ? OperationOnFail::RETRY_LATER : OperationOnFail::DISCARD;
        } else if (sendResult == SEND_QUOTA_EXCEED) {
            BOOL_FLAG(global_network_success) = true;
            if (slsResponse.mErrorCode == sdk::LOGE_SHARD_WRITE_QUOTA_EXCEED) {
                failDetail << "shard write quota exceed";
                suggestion << "Split logstore shards. https://help.aliyun.com/zh/sls/user-guide/expansion-of-resources";
            } else {
                failDetail << "project write quota exceed";
                suggestion << "Submit quota modification request. "
                              "https://help.aliyun.com/zh/sls/user-guide/expansion-of-resources";
            }
            LogtailAlarm::GetInstance()->SendAlarm(SEND_QUOTA_EXCEED_ALARM,
                                                   "error_code: " + slsResponse.mErrorCode
                                                       + ", error_message: " + slsResponse.mErrorMsg
                                                       + ", request_id:" + slsResponse.mRequestId,
                                                   mProject,
                                                   data->mLogstore,
                                                   mRegion);
            operation = OperationOnFail::RETRY_LATER;
        } else if (sendResult == SEND_UNAUTHORIZED) {
            failDetail << "write unauthorized";
            suggestion << "check https connection to endpoint or access keys provided";
            if (data->mTryCnt > static_cast<uint32_t>(INT32_FLAG(unauthorized_send_retrytimes))) {
                operation = OperationOnFail::DISCARD;
            } else {
                BOOL_FLAG(global_network_success) = true;
#ifdef __ENTERPRISE__
                if (mAliuid.empty() && !EnterpriseConfigProvider::GetInstance()->IsPubRegion()) {
                    operation = OperationOnFail::RETRY_IMMEDIATELY;
                } else {
#endif
                    int32_t lastUpdateTime;
                    sdk::Client* sendClient = SLSClientManager::GetInstance()->GetClient(mRegion, mAliuid);
                    if (SLSControl::GetInstance()->SetSlsSendClientAuth(mAliuid, false, sendClient, lastUpdateTime))
                        operation = OperationOnFail::RETRY_IMMEDIATELY;
                    else if (curTime - lastUpdateTime < INT32_FLAG(unauthorized_allowed_delay_after_reset))
                        operation = OperationOnFail::RETRY_LATER;
                    else
                        operation = OperationOnFail::DISCARD;
#ifdef __ENTERPRISE__
                }
#endif
            }
        } else if (sendResult == SEND_PARAMETER_INVALID) {
            failDetail << "invalid paramters";
            suggestion << "check input parameters";
            operation = DefaultOperation(item->mTryCnt);
        } else if (sendResult == SEND_INVALID_SEQUENCE_ID) {
            failDetail << "invalid exactly-once sequence id";
            do {
                auto& cpt = data->mExactlyOnceCheckpoint;
                if (!cpt) {
                    failDetail << ", unexpected result when exactly once checkpoint is not found";
                    suggestion << "report bug";
                    LogtailAlarm::GetInstance()->SendAlarm(
                        EXACTLY_ONCE_ALARM,
                        "drop exactly once log group because of invalid sequence ID, request id:"
                            + slsResponse.mRequestId,
                        mProject,
                        data->mLogstore,
                        mRegion);
                    operation = OperationOnFail::DISCARD;
                    break;
                }

                // Because hash key is generated by UUID library, we consider that
                //  the possibility of hash key conflict is very low, so data is
                //  dropped here.
                cpt->Commit();
                failDetail << ", drop exactly once log group and commit checkpoint" << " checkpointKey:" << cpt->key
                           << " checkpoint:" << cpt->data.DebugString();
                suggestion << "no suggestion";
                LogtailAlarm::GetInstance()->SendAlarm(
                    EXACTLY_ONCE_ALARM,
                    "drop exactly once log group because of invalid sequence ID, cpt:" + cpt->key
                        + ", data:" + cpt->data.DebugString() + "request id:" + slsResponse.mRequestId,
                    mProject,
                    data->mLogstore,
                    mRegion);
                operation = OperationOnFail::DISCARD;
                cpt->IncreaseSequenceID();
            } while (0);
        } else if (AppConfig::GetInstance()->EnableLogTimeAutoAdjust()
                   && sdk::LOGE_REQUEST_TIME_EXPIRED == slsResponse.mErrorCode) {
            failDetail << "write request expired, will retry";
            suggestion << "check local system time";
            operation = OperationOnFail::RETRY_IMMEDIATELY;
        } else {
            failDetail << "other error";
            suggestion << "no suggestion";
            // when unknown error such as SignatureNotMatch happens, we should retry several times
            // first time, we will retry immediately
            // then we record error and retry latter
            // when retry times > unknow_error_try_max, we will drop this data
            operation = DefaultOperation(item->mTryCnt);
        }
        if (curTime - data->mEnqueTime > INT32_FLAG(discard_send_fail_interval)) {
            operation = OperationOnFail::DISCARD;
        }
        if (isProfileData && data->mTryCnt >= static_cast<uint32_t>(INT32_FLAG(profile_data_send_retrytimes))) {
            operation = OperationOnFail::DISCARD;
        }

#define LOG_PATTERN \
    ("failed to send request", failDetail.str())("operation", GetOperationString(operation))( \
        "suggestion", suggestion.str())("item address", item)("request id", slsResponse.mRequestId)( \
        "status code", slsResponse.mStatusCode)("error code", slsResponse.mErrorCode)( \
        "errMsg", slsResponse.mErrorMsg)("config", configName)("region", mRegion)("project", mProject)( \
        "logstore", data->mLogstore)("try cnt", data->mTryCnt)("response time", curTime - data->mLastSendTime)( \
        "total send time", curTime - data->mEnqueTime)("endpoint", data->mCurrentEndpoint)("is profile data", \
                                                                                           isProfileData)

        switch (operation) {
            case OperationOnFail::RETRY_IMMEDIATELY:
                ++item->mTryCnt;
                FlusherRunner::GetInstance()->PushToHttpSink(item, false);
                break;
            case OperationOnFail::RETRY_LATER:
                if (slsResponse.mErrorCode == sdk::LOGE_REQUEST_TIMEOUT
                    || curTime - data->mLastLogWarningTime > ON_FAIL_LOG_WARNING_INTERVAL_SECOND) {
                    LOG_WARNING(sLogger, LOG_PATTERN);
                    data->mLastLogWarningTime = curTime;
                }
                DealSenderQueueItemAfterSend(item, true);
                break;
            case OperationOnFail::DISCARD:
            default:
                LOG_WARNING(sLogger, LOG_PATTERN);
                if (!isProfileData) {
                    LogtailAlarm::GetInstance()->SendAlarm(
                        SEND_DATA_FAIL_ALARM,
                        "failed to send request: " + failDetail.str() + "\toperation: " + GetOperationString(operation)
                            + "\trequestId: " + slsResponse.mRequestId
                            + "\tstatusCode: " + ToString(slsResponse.mStatusCode)
                            + "\terrorCode: " + slsResponse.mErrorCode + "\terrorMessage: " + slsResponse.mErrorMsg
                            + "\tconfig: " + configName + "\tendpoint: " + data->mCurrentEndpoint,
                        mProject,
                        data->mLogstore,
                        mRegion);
                }
                DealSenderQueueItemAfterSend(item, false);
                break;
        }
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
    return Flusher::PushToQueue(make_unique<SLSSenderQueueItem>(std::move(compressedData),
                                                                data.size(),
                                                                this,
                                                                mQueueKey,
                                                                logstore.empty() ? mLogstore : logstore,
                                                                RawDataType::EVENT_GROUP,
                                                                shardHashKey));
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
            = PipelineManager::GetInstance()->FindConfigByName(item->mFlusher->GetContext().GetConfigName());
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
