// Copyright 2023 iLogtail Authors
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

#include <json/json.h>

#include <memory>
#include <string>

#include "common/JsonUtil.h"
#include "common/LogstoreFeedbackKey.h"
#include "common/LogtailCommonFlags.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif
#include "flusher/FlusherSLS.h"
#include "pipeline/PipelineContext.h"
#include "sender/SLSClientManager.h"
#include "sender/Sender.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_INT32(batch_send_interval);
DECLARE_FLAG_INT32(merge_log_count_limit);
DECLARE_FLAG_INT32(batch_send_metric_size);

using namespace std;

namespace logtail {

class FlusherSLSUnittest : public testing::Test {
public:
    void OnSuccessfulInit();
    void OnFailedInit();
    void OnPipelineUpdate();

protected:
    void SetUp() override { ctx.SetConfigName("test_config"); }

private:
    PipelineContext ctx;
};

void FlusherSLSUnittest::OnSuccessfulInit() {
    unique_ptr<FlusherSLS> flusher;
    Json::Value configJson, optionalGoPipelineJson, optionalGoPipeline;
    string configStr, optionalGoPipelineStr, errorMsg;

    // only mandatory param
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(optionalGoPipeline.isNull());
    APSARA_TEST_EQUAL("test_project", flusher->mProject);
    APSARA_TEST_EQUAL("test_logstore", flusher->mLogstore);
    APSARA_TEST_EQUAL(GenerateLogstoreFeedBackKey("test_project", "test_logstore"), flusher->GetLogstoreKey());
    APSARA_TEST_EQUAL(STRING_FLAG(default_region_name), flusher->mRegion);
    APSARA_TEST_EQUAL("cn-hangzhou.log.aliyuncs.com", flusher->mEndpoint);
    APSARA_TEST_EQUAL("", flusher->mAliuid);
    APSARA_TEST_EQUAL(FlusherSLS::TelemetryType::LOG, flusher->mTelemetryType);
    APSARA_TEST_EQUAL(0U, flusher->mFlowControlExpireTime);
    APSARA_TEST_EQUAL(-1, flusher->mMaxSendRate);
    APSARA_TEST_TRUE(flusher->mShardHashKeys.empty());
    APSARA_TEST_EQUAL(CompressType::LZ4, flusher->mCompressor->GetCompressType());
    APSARA_TEST_TRUE(flusher->mGroupSerializer);
    APSARA_TEST_TRUE(flusher->mGroupListSerializer);
    APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(merge_log_count_limit)),
                      flusher->mBatcher.mEventFlushStrategy.GetMaxCnt());
    APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(batch_send_metric_size)),
                      flusher->mBatcher.mEventFlushStrategy.GetMaxSizeBytes());
    uint32_t timeout = static_cast<uint32_t>(INT32_FLAG(batch_send_interval)) / 2;
    APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(batch_send_interval)) - timeout,
                      flusher->mBatcher.mEventFlushStrategy.GetTimeoutSecs());
    APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(batch_send_metric_size)),
                      flusher->mBatcher.mGroupFlushStrategy->GetMaxSizeBytes());
    APSARA_TEST_EQUAL(timeout, flusher->mBatcher.mGroupFlushStrategy->GetTimeoutSecs());

    // valid optional param
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com",
            "Aliuid": "123456789",
            "CompressType": "zstd",
            "TelemetryType": "metrics",
            "FlowControlExpireTime": 123456789,
            "MaxSendRate": 5,
            "ShardHashKeys": [
                "__source__"
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL("cn-hangzhou", flusher->mRegion);
#ifdef __ENTERPRISE__
    APSARA_TEST_EQUAL("123456789", flusher->mAliuid);
#else
    APSARA_TEST_EQUAL("", flusher->mAliuid);
#endif
    APSARA_TEST_EQUAL(CompressType::ZSTD, flusher->mCompressor->GetCompressType());
    APSARA_TEST_EQUAL(FlusherSLS::TelemetryType::METRIC, flusher->mTelemetryType);
    APSARA_TEST_EQUAL(123456789U, flusher->mFlowControlExpireTime);
    APSARA_TEST_EQUAL(5, flusher->mMaxSendRate);
    APSARA_TEST_EQUAL(1U, flusher->mShardHashKeys.size());

    // invalid optional param
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": true,
            "Endpoint": "cn-hangzhou.log.aliyuncs.com",
            "Aliuid": true,
            "CompressType": true,
            "TelemetryType": true,
            "FlowControlExpireTime": true,
            "MaxSendRate": true,
            "ShardHashKeys": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(STRING_FLAG(default_region_name), flusher->mRegion);
    APSARA_TEST_EQUAL("", flusher->mAliuid);
    APSARA_TEST_EQUAL(CompressType::LZ4, flusher->mCompressor->GetCompressType());
    APSARA_TEST_EQUAL(FlusherSLS::TelemetryType::LOG, flusher->mTelemetryType);
    APSARA_TEST_EQUAL(0U, flusher->mFlowControlExpireTime);
    APSARA_TEST_EQUAL(-1, flusher->mMaxSendRate);
    APSARA_TEST_TRUE(flusher->mShardHashKeys.empty());

#ifdef __ENTERPRISE__
    // region
    EnterpriseConfigProvider::GetInstance()->mIsPrivateCloud = true;
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(STRING_FLAG(default_region_name), flusher->mRegion);
    EnterpriseConfigProvider::GetInstance()->mIsPrivateCloud = false;
#endif

    // endpoint
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "cn-hangzhou",
            "Endpoint": "  cn-hangzhou.log.aliyuncs.com   "
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL("cn-hangzhou.log.aliyuncs.com", flusher->mEndpoint);
    auto iter = SLSClientManager::GetInstance()->mRegionEndpointEntryMap.find("cn-hangzhou");
    APSARA_TEST_NOT_EQUAL(SLSClientManager::GetInstance()->mRegionEndpointEntryMap.end(), iter);
    APSARA_TEST_NOT_EQUAL(iter->second.mEndpointInfoMap.end(),
                          iter->second.mEndpointInfoMap.find("http://cn-hangzhou.log.aliyuncs.com"));

    // TelemetryType
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com",
            "TelemetryType": "logs"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(FlusherSLS::TelemetryType::LOG, flusher->mTelemetryType);

    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com",
            "TelemetryType": "unknown"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(FlusherSLS::TelemetryType::LOG, flusher->mTelemetryType);

    // additional param
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com",
            "EnableShardHash": false
        }
    )";
    optionalGoPipelineStr = R"(
        {
            "flushers": [
                {
                    "type": "flusher_sls",
                    "detail": {
                        "EnableShardHash": false
                    }
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(optionalGoPipelineStr, optionalGoPipelineJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(optionalGoPipelineJson == optionalGoPipeline);
}

void FlusherSLSUnittest::OnFailedInit() {
    unique_ptr<FlusherSLS> flusher;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // invalid Project
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Logstore": "test_logstore",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_FALSE(flusher->Init(configJson, optionalGoPipeline));

    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": true,
            "Logstore": "test_logstore",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_FALSE(flusher->Init(configJson, optionalGoPipeline));

    // invalid Logstore
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_FALSE(flusher->Init(configJson, optionalGoPipeline));

    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": true,
            "Endpoint": "cn-hangzhou.log.aliyuncs.com"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_FALSE(flusher->Init(configJson, optionalGoPipeline));

#ifndef __ENTERPRISE__
    // invalid Endpoint
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_FALSE(flusher->Init(configJson, optionalGoPipeline));

    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Endpoint": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_FALSE(flusher->Init(configJson, optionalGoPipeline));
#endif
}

void FlusherSLSUnittest::OnPipelineUpdate() {
    PipelineContext ctx1, ctx2;
    ctx1.SetConfigName("test_config_1");
    ctx2.SetConfigName("test_config_2");

    Json::Value configJson, optionalGoPipeline;
    FlusherSLS flusher1, flusher2;
    flusher1.SetContext(ctx1);
    flusher2.SetContext(ctx2);
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    APSARA_TEST_TRUE(flusher1.Init(configJson, optionalGoPipeline));

    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project_2",
            "Logstore": "test_logstore_2",
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com",
            "Aliuid": "123456789"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    APSARA_TEST_TRUE(flusher2.Init(configJson, optionalGoPipeline));

    APSARA_TEST_TRUE(flusher1.Register());
    APSARA_TEST_EQUAL(1U, Sender::Instance()->mProjectRefCntMap.size());
    APSARA_TEST_TRUE(Sender::Instance()->IsRegionContainingConfig("cn-hangzhou"));
    APSARA_TEST_EQUAL(1U, SLSClientManager::GetInstance()->GetRegionAliuids("cn-hangzhou").size());

    APSARA_TEST_TRUE(flusher2.Register());
    APSARA_TEST_EQUAL(2U, Sender::Instance()->mProjectRefCntMap.size());
    APSARA_TEST_TRUE(Sender::Instance()->IsRegionContainingConfig("cn-hangzhou"));
#ifdef __ENTERPRISE__
    APSARA_TEST_EQUAL(2U, SLSClientManager::GetInstance()->GetRegionAliuids("cn-hangzhou").size());
#else
    APSARA_TEST_EQUAL(1U, SLSClientManager::GetInstance()->GetRegionAliuids("cn-hangzhou").size());
#endif

    APSARA_TEST_TRUE(flusher2.Unregister(true));
    APSARA_TEST_EQUAL(1U, Sender::Instance()->mProjectRefCntMap.size());
    APSARA_TEST_TRUE(Sender::Instance()->IsRegionContainingConfig("cn-hangzhou"));
    APSARA_TEST_EQUAL(1U, SLSClientManager::GetInstance()->GetRegionAliuids("cn-hangzhou").size());

    APSARA_TEST_TRUE(flusher1.Unregister(true));
    APSARA_TEST_TRUE(Sender::Instance()->mProjectRefCntMap.empty());
    APSARA_TEST_FALSE(Sender::Instance()->IsRegionContainingConfig("cn-hangzhou"));
    APSARA_TEST_TRUE(SLSClientManager::GetInstance()->GetRegionAliuids("cn-hangzhou").empty());
}

UNIT_TEST_CASE(FlusherSLSUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(FlusherSLSUnittest, OnFailedInit)
UNIT_TEST_CASE(FlusherSLSUnittest, OnPipelineUpdate)

} // namespace logtail

UNIT_TEST_MAIN
