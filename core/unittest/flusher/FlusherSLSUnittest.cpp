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

#include <memory>
#include <string>

#include <json/json.h>

#include "common/JsonUtil.h"
#include "common/LogstoreFeedbackKey.h"
#include "common/LogtailCommonFlags.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif
#include "flusher/FlusherSLS.h"
#include "pipeline/PipelineContext.h"
#include "sender/Sender.h"
#include "unittest/Unittest.h"

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
    APSARA_TEST_EQUAL(FlusherSLS::CompressType::LZ4, flusher->mCompressType);
    APSARA_TEST_EQUAL(FlusherSLS::TelemetryType::LOG, flusher->mTelemetryType);
    APSARA_TEST_EQUAL(0U, flusher->mFlowControlExpireTime);
    APSARA_TEST_EQUAL(-1, flusher->mMaxSendRate);
    APSARA_TEST_EQUAL(FlusherSLS::Batch::MergeType::TOPIC, flusher->mBatch.mMergeType);
    APSARA_TEST_EQUAL(INT32_FLAG(batch_send_interval), flusher->mBatch.mSendIntervalSecs);
    APSARA_TEST_TRUE(flusher->mBatch.mShardHashKeys.empty());

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
            "Batch": {
                "MergeType": "logstore",
                "SendIntervalSecs": 1,
                "ShardHashKeys": [
                    "__source__"
                ]
            }
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
    APSARA_TEST_EQUAL(FlusherSLS::CompressType::ZSTD, flusher->mCompressType);
    APSARA_TEST_EQUAL(FlusherSLS::TelemetryType::METRIC, flusher->mTelemetryType);
    APSARA_TEST_EQUAL(123456789U, flusher->mFlowControlExpireTime);
    APSARA_TEST_EQUAL(5, flusher->mMaxSendRate);
    APSARA_TEST_EQUAL(FlusherSLS::Batch::MergeType::LOGSTORE, flusher->mBatch.mMergeType);
    APSARA_TEST_EQUAL(1U, flusher->mBatch.mSendIntervalSecs);
    APSARA_TEST_EQUAL(1U, flusher->mBatch.mShardHashKeys.size());

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
            "Batch": {
                "MergeType": true,
                "SendIntervalSecs": true,
                "ShardHashKeys": true
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(STRING_FLAG(default_region_name), flusher->mRegion);
    APSARA_TEST_EQUAL("", flusher->mAliuid);
    APSARA_TEST_EQUAL(FlusherSLS::CompressType::LZ4, flusher->mCompressType);
    APSARA_TEST_EQUAL(FlusherSLS::TelemetryType::LOG, flusher->mTelemetryType);
    APSARA_TEST_EQUAL(0U, flusher->mFlowControlExpireTime);
    APSARA_TEST_EQUAL(-1, flusher->mMaxSendRate);
    APSARA_TEST_EQUAL(FlusherSLS::Batch::MergeType::TOPIC, flusher->mBatch.mMergeType);
    APSARA_TEST_EQUAL(INT32_FLAG(batch_send_interval), flusher->mBatch.mSendIntervalSecs);
    APSARA_TEST_TRUE(flusher->mBatch.mShardHashKeys.empty());

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
    auto iter = Sender::Instance()->mRegionEndpointEntryMap.find("cn-hangzhou");
    APSARA_TEST_NOT_EQUAL(Sender::Instance()->mRegionEndpointEntryMap.end(), iter);
    APSARA_TEST_NOT_EQUAL(iter->second->mEndpointDetailMap.end(),
                          iter->second->mEndpointDetailMap.find("http://cn-hangzhou.log.aliyuncs.com"));

    // CompressType
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com",
            "CompressType": "none"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(FlusherSLS::CompressType::NONE, flusher->mCompressType);

    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com",
            "CompressType": "lz4"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(FlusherSLS::CompressType::LZ4, flusher->mCompressType);

    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com",
            "CompressType": "unknown"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(FlusherSLS::CompressType::LZ4, flusher->mCompressType);

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

    // Batch.MergeType
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com",
            "Batch": {
                "MergeType": "topic"
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(FlusherSLS::Batch::MergeType::TOPIC, flusher->mBatch.mMergeType);

    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com",
            "Batch": {
                "MergeType": "unknown"
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(FlusherSLS::Batch::MergeType::TOPIC, flusher->mBatch.mMergeType);

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
}

void FlusherSLSUnittest::OnPipelineUpdate() {
    PipelineContext ctx1, ctx2;
    ctx1.SetConfigName("test_config_1");
    ctx2.SetConfigName("test_config_2");

    Json::Value configJson, optionalGoPipeline;
    FlusherSLS flusher1, flusher2;
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
    flusher1.SetContext(ctx1);

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
    flusher2.SetContext(ctx2);

    APSARA_TEST_TRUE(flusher1.Start());
    APSARA_TEST_EQUAL(1U, Sender::Instance()->mProjectRefCntMap.size());
    APSARA_TEST_TRUE(Sender::Instance()->IsRegionContainingConfig("cn-hangzhou"));
    APSARA_TEST_EQUAL(1U, Sender::Instance()->GetRegionAliuids("cn-hangzhou").size());

    APSARA_TEST_TRUE(flusher2.Start());
    APSARA_TEST_EQUAL(2U, Sender::Instance()->mProjectRefCntMap.size());
    APSARA_TEST_TRUE(Sender::Instance()->IsRegionContainingConfig("cn-hangzhou"));
#ifdef __ENTERPRISE__
    APSARA_TEST_EQUAL(2U, Sender::Instance()->GetRegionAliuids("cn-hangzhou").size());
#else
    APSARA_TEST_EQUAL(1U, Sender::Instance()->GetRegionAliuids("cn-hangzhou").size());
#endif

    APSARA_TEST_TRUE(flusher2.Stop(true));
    APSARA_TEST_EQUAL(1U, Sender::Instance()->mProjectRefCntMap.size());
    APSARA_TEST_TRUE(Sender::Instance()->IsRegionContainingConfig("cn-hangzhou"));
    APSARA_TEST_EQUAL(1U, Sender::Instance()->GetRegionAliuids("cn-hangzhou").size());

    APSARA_TEST_TRUE(flusher1.Stop(true));
    APSARA_TEST_TRUE(Sender::Instance()->mProjectRefCntMap.empty());
    APSARA_TEST_FALSE(Sender::Instance()->IsRegionContainingConfig("cn-hangzhou"));
    APSARA_TEST_TRUE(Sender::Instance()->GetRegionAliuids("cn-hangzhou").empty());
}

UNIT_TEST_CASE(FlusherSLSUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(FlusherSLSUnittest, OnFailedInit)
UNIT_TEST_CASE(FlusherSLSUnittest, OnPipelineUpdate)

} // namespace logtail

UNIT_TEST_MAIN
