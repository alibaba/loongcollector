#include "flusher/FlusherSLS.h"

#include "app_config/AppConfig.h"
#include "config_manager/ConfigManager.h"
#include "common/ParamExtractor.h"
#include "pipeline/Pipeline.h"
#include "sender/Sender.h"

using namespace std;

DECLARE_FLAG_INT32(batch_send_interval);
DEFINE_FLAG_BOOL(sls_client_send_compress, "whether compresses the data or not when put data", true);

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
                                                        "Batch"};

FlusherSLS::FlusherSLS() : mRegion(AppConfig::GetInstance()->GetDefaultRegion()) {
}

FlusherSLS::Batch::Batch() : mSendIntervalSecs(INT32_FLAG(batch_send_interval)) {
}

bool FlusherSLS::Init(const Json::Value& config) {
    string errorMsg;

    // Project
    if (!GetMandatoryStringParam(config, "Project", mProject, errorMsg)) {
        PARAM_ERROR(mContext->GetLogger(), errorMsg, sName, mContext->GetConfigName());
    }

    // Logstore
    if (!GetMandatoryStringParam(config, "Logstore", mLogstore, errorMsg)) {
        PARAM_ERROR(mContext->GetLogger(), errorMsg, sName, mContext->GetConfigName());
    }
    mLogstoreKey = GenerateLogstoreFeedBackKey(mProject, mLogstore);

#ifdef __ENTERPRISE__
    if (AppConfig::GetInstance()->IsDataServerPrivateCloud()) {
        mRegion = STRING_FLAG(default_region_name);
    } else {
#endif
        // Region
        if (!GetOptionalStringParam(config, "Region", mRegion, errorMsg)) {
            PARAM_WARNING_DEFAULT(
                mContext->GetLogger(), errorMsg, AppConfig::GetInstance()->GetDefaultRegion(), sName, mContext->GetConfigName());
        }

        // Endpoint
        if (!GetMandatoryStringParam(config, "Endpoint", mEndpoint, errorMsg)) {
            PARAM_ERROR(mContext->GetLogger(), errorMsg, sName, mContext->GetConfigName());
        }
        mEndpoint = TrimString(mEndpoint);
        if (!mEndpoint.empty()) {
            Sender::Instance()->AddEndpointEntry(mRegion, CheckAddress(mEndpoint, mEndpoint));
        }
#ifdef __ENTERPRISE__
    }

    // Aliuid
    if (!GetOptionalStringParam(config, "Aliuid", mAliuid, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(), errorMsg, sName, mContext->GetConfigName());
    }
#endif

    // CompressType
    if (BOOL_FLAG(sls_client_send_compress)) {
        string compressType;
        if (!GetOptionalStringParam(config, "CompressType", compressType, errorMsg)) {
            PARAM_WARNING_DEFAULT(mContext->GetLogger(), errorMsg, "lz4", sName, mContext->GetConfigName());
        } else if (compressType == "zstd") {
            mCompressType = CompressType::ZSTD;
        } else if (compressType == "none") {
            mCompressType = CompressType::NONE;
        } else if (compressType != "lz4") {
            PARAM_WARNING_DEFAULT(mContext->GetLogger(), "param CompressType is not valid", "lz4", sName, mContext->GetConfigName());
        }
    } else {
        mCompressType = CompressType::NONE;
    }

    // TelemetryType
    string telemetryType;
    if (!GetOptionalStringParam(config, "TelemetryType", telemetryType, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(), errorMsg, "logs", sName, mContext->GetConfigName());
    } else if (telemetryType == "metrics") {
        mTelemetryType = TelemetryType::METRIC;
    } else if (telemetryType != "logs") {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(), "param TelemetryType is not valid", "logs", sName, mContext->GetConfigName());
    }

    // FlowControlExpireTime
    if (!GetOptionalUIntParam(config, "FlowControlExpireTime", mFlowControlExpireTime, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(), errorMsg, 0, sName, mContext->GetConfigName());
    }

    // MaxSendRate
    if (!GetOptionalIntParam(config, "MaxSendRate", mMaxSendRate, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(), errorMsg, -1, sName, mContext->GetConfigName());
    }
    Sender::Instance()->SetLogstoreFlowControl(mLogstoreKey, mMaxSendRate, mFlowControlExpireTime);

    // Batch
    const char* key = "Batch";
    const Json::Value* itr = config.find(key, key + strlen(key));
    if (itr) {
        if (!itr->isObject()) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(), "param Batch is not of type object", sName, mContext->GetConfigName());
        } else {
            // MergeType
            string mergeType;
            if (!GetOptionalStringParam(*itr, "Batch.MergeType", mergeType, errorMsg)) {
                PARAM_WARNING_DEFAULT(mContext->GetLogger(), errorMsg, "topic", sName, mContext->GetConfigName());
            } else if (mergeType == "logstore") {
                mBatch.mMergeType = Batch::MergeType::LOGSTORE;
            } else if (mergeType != "topic") {
                PARAM_WARNING_DEFAULT(
                    mContext->GetLogger(), "param Batch.MergeType is not valid", "topic", sName, mContext->GetConfigName());
            }

            // SendIntervalSecs
            if (!GetOptionalUIntParam(*itr, "Batch.SendIntervalSecs", mBatch.mSendIntervalSecs, errorMsg)) {
                PARAM_WARNING_DEFAULT(
                    mContext->GetLogger(), errorMsg, INT32_FLAG(batch_send_interval), sName, mContext->GetConfigName());
            }

            // ShardHashKeys
            if (!GetOptionalListParam<string>(*itr, "Batch.ShardHashKeys", mBatch.mShardHashKeys, errorMsg)) {
                PARAM_WARNING_IGNORE(mContext->GetLogger(), errorMsg, sName, mContext->GetConfigName());
            }
        }
    }

    // generate Go plugin if necessary
    if (mContext->IsFlushingThroughGoPipeline()) {
        AddPluginToGoPipeline(config);
    }

    // 过渡使用
    ConfigManager::GetInstance()->InsertRegionAliuidMap(mRegion, mAliuid);
    ConfigManager::GetInstance()->InsertProject(mProject);
    ConfigManager::GetInstance()->InsertRegion(mRegion);

    return true;
}

bool FlusherSLS::Start() {
    // Sender::Instance()->IncreaseProjectReferenceCnt(mProject);
    // Sender::Instance()->IncreaseRegionReferenceCnt(mRegion);
    // Sender::Instance()->IncreaseAliuidReferenceCntForRegion(mRegion, mAliuid);
    return true;
}

bool FlusherSLS::Stop(bool isPipelineRemoving) {
    // Sender::Instance()->DecreaseProjectReferenceCnt(mProject);
    // Sender::Instance()->DecreaseRegionReferenceCnt(mRegion);
    // Sender::Instance()->DecreaseAliuidReferenceCntForRegion(mRegion, mAliuid);
    return true;
}

void FlusherSLS::AddPluginToGoPipeline(const Json::Value& config) const {
    Json::Value detail(Json::objectValue);
    for (auto itr = config.begin(); itr != config.end(); ++itr) {
        if (sNativeParam.find(itr.name()) != sNativeParam.end()) {
            detail[itr.name()] = *itr;
        }
    }
    if (!detail.empty()) {
        Json::Value flusherSLS(Json::objectValue);
        flusherSLS["type"] = "flusher_sls";
        flusherSLS["detail"] = detail;

        Json::Value *flushers;
        if (mContext->GetPipeline().ShouldAddFlusherToGoPipelineWithInput()) {
            flushers = &mContext->GetPipeline().GetGoPipelineWithInput()["flushers"];
        } else {
            flushers = &mContext->GetPipeline().GetGoPipelineWithoutInput()["flushers"];
        }
        flushers->append(flusherSLS);
    }
}
} // namespace logtail
