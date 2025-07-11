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
#include <random>
#include <string>

#include "json/json.h"

#include "TagConstants.h"
#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/queue/ExactlyOnceQueueManager.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "collection_pipeline/queue/QueueKeyManager.h"
#include "collection_pipeline/queue/SLSSenderQueueItem.h"
#include "collection_pipeline/queue/SenderQueueManager.h"
#include "common/JsonUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/compression/CompressorFactory.h"
#include "common/http/Constant.h"
#include "plugin/flusher/sls/FlusherSLS.h"
#include "plugin/flusher/sls/PackIdManager.h"
#include "plugin/flusher/sls/SLSClientManager.h"
#include "plugin/flusher/sls/SLSConstant.h"
#include "unittest/Unittest.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#include "unittest/flusher/SLSNetworkRequestMock.h"
#endif

DECLARE_FLAG_INT32(batch_send_interval);
DECLARE_FLAG_INT32(merge_log_count_limit);
DECLARE_FLAG_INT32(batch_send_metric_size);
DECLARE_FLAG_INT32(max_send_log_group_size);
DECLARE_FLAG_DOUBLE(sls_serialize_size_expansion_ratio);
DECLARE_FLAG_BOOL(send_prefer_real_ip);
DECLARE_FLAG_STRING(default_access_key_id);
DECLARE_FLAG_STRING(default_access_key);

using namespace std;

namespace logtail {

class FlusherSLSUnittest : public testing::Test {
public:
    void OnSuccessfulInit();
    void OnFailedInit();
    void OnPipelineUpdate();
    void TestBuildRequest();
    void TestSend();
    void TestFlush();
    void TestFlushAll();
    void TestAddPackId();
    void OnGoPipelineSend();

protected:
    static void SetUpTestCase() {
#ifdef __ENTERPRISE__
        EnterpriseSLSClientManager::GetInstance()->mDoProbeNetwork = ProbeNetworkMock::DoProbeNetwork;
        EnterpriseSLSClientManager::GetInstance()->mGetEndpointRealIp = GetRealIpMock::GetEndpointRealIp;
        EnterpriseSLSClientManager::GetInstance()->mGetAccessKeyFromSLS = GetAccessKeyMock::DoGetAccessKey;
#endif
    }

    void SetUp() override {
        ctx.SetConfigName("test_config");
        ctx.SetPipeline(pipeline);
    }

    void TearDown() override {
        PackIdManager::GetInstance()->mPackIdSeq.clear();
        QueueKeyManager::GetInstance()->Clear();
        SenderQueueManager::GetInstance()->Clear();
        ExactlyOnceQueueManager::GetInstance()->Clear();
#ifdef __ENTERPRISE__
        EnterpriseSLSClientManager::GetInstance()->Clear();
#endif
    }

private:
    CollectionPipeline pipeline;
    CollectionPipelineContext ctx;
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
            "Logstore": "test_logstore"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
#ifndef __ENTERPRISE__
    configJson["Endpoint"] = "test_region.log.aliyuncs.com";
#endif
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
    APSARA_TEST_TRUE(optionalGoPipeline.isNull());
    APSARA_TEST_EQUAL("test_project", flusher->mProject);
    APSARA_TEST_EQUAL("test_logstore", flusher->mLogstore);
    APSARA_TEST_EQUAL(STRING_FLAG(default_region_name), flusher->mRegion);
#ifndef __ENTERPRISE__
    APSARA_TEST_EQUAL("test_region.log.aliyuncs.com", flusher->mEndpoint);
#endif
    APSARA_TEST_EQUAL("", flusher->mAliuid);
    APSARA_TEST_EQUAL(sls_logs::SlsTelemetryType::SLS_TELEMETRY_TYPE_LOGS, flusher->mTelemetryType);
    APSARA_TEST_TRUE(flusher->mShardHashKeys.empty());
    APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(merge_log_count_limit)),
                      flusher->mBatcher.GetEventFlushStrategy().GetMinCnt());
    APSARA_TEST_EQUAL(
        static_cast<uint32_t>(INT32_FLAG(max_send_log_group_size) / DOUBLE_FLAG(sls_serialize_size_expansion_ratio)),
        flusher->mBatcher.GetEventFlushStrategy().GetMaxSizeBytes());
    APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(batch_send_metric_size)),
                      flusher->mBatcher.GetEventFlushStrategy().GetMinSizeBytes());
    uint32_t timeout = static_cast<uint32_t>(INT32_FLAG(batch_send_interval)) / 2;
    APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(batch_send_interval)) - timeout,
                      flusher->mBatcher.GetEventFlushStrategy().GetTimeoutSecs());
    APSARA_TEST_TRUE(flusher->mBatcher.GetGroupFlushStrategy().has_value());
    APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(batch_send_metric_size)),
                      flusher->mBatcher.GetGroupFlushStrategy()->GetMinSizeBytes());
    APSARA_TEST_EQUAL(timeout, flusher->mBatcher.GetGroupFlushStrategy()->GetTimeoutSecs());
    APSARA_TEST_TRUE(flusher->mGroupSerializer);
    APSARA_TEST_TRUE(flusher->mGroupListSerializer);
    APSARA_TEST_EQUAL(CompressType::LZ4, flusher->mCompressor->GetCompressType());
    APSARA_TEST_EQUAL(QueueKeyManager::GetInstance()->GetKey("test_config-flusher_sls-test_project#test_logstore"),
                      flusher->GetQueueKey());
    auto que = SenderQueueManager::GetInstance()->GetQueue(flusher->GetQueueKey());
    APSARA_TEST_NOT_EQUAL(nullptr, que);
    APSARA_TEST_FALSE(que->GetRateLimiter().has_value());
    APSARA_TEST_EQUAL(3U, que->GetConcurrencyLimiters().size());

    // valid optional param
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region",
            "Endpoint": "test_region.log.aliyuncs.com",
            "Aliuid": "123456789",
            "TelemetryType": "logs",
            "ShardHashKeys": [
                "__source__"
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
#ifndef __ENTERPRISE__
    configJson["EndpointMode"] = "default";
#endif
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL("test_region", flusher->mRegion);
#ifdef __ENTERPRISE__
    APSARA_TEST_EQUAL("123456789", flusher->mAliuid);
    APSARA_TEST_EQUAL(EndpointMode::DEFAULT, flusher->mEndpointMode);
#else
    APSARA_TEST_EQUAL("", flusher->mAliuid);
#endif
    APSARA_TEST_EQUAL("test_region.log.aliyuncs.com", flusher->mEndpoint);
    APSARA_TEST_EQUAL(sls_logs::SlsTelemetryType::SLS_TELEMETRY_TYPE_LOGS, flusher->mTelemetryType);
    APSARA_TEST_EQUAL(1U, flusher->mShardHashKeys.size());
    APSARA_TEST_EQUAL("__source__", flusher->mShardHashKeys[0]);
    SenderQueueManager::GetInstance()->Clear();

    // invalid optional param
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": true,
            "Aliuid": true,
            "TelemetryType": true,
            "ShardHashKeys": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
#ifdef __ENTERPRISE__
    configJson["EndpointMode"] = true;
    configJson["Endpoint"] = true;
#else
    configJson["Endpoint"] = "test_region.log.aliyuncs.com";
#endif
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(STRING_FLAG(default_region_name), flusher->mRegion);
#ifdef __ENTERPRISE__
    APSARA_TEST_EQUAL(EndpointMode::DEFAULT, flusher->mEndpointMode);
    APSARA_TEST_EQUAL("", flusher->mEndpoint);
#endif
    APSARA_TEST_EQUAL("", flusher->mAliuid);
    APSARA_TEST_EQUAL(sls_logs::SlsTelemetryType::SLS_TELEMETRY_TYPE_LOGS, flusher->mTelemetryType);
    APSARA_TEST_TRUE(flusher->mShardHashKeys.empty());
    SenderQueueManager::GetInstance()->Clear();

#ifdef __ENTERPRISE__
    // region
    EnterpriseConfigProvider::GetInstance()->mIsPrivateCloud = true;
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region",
            "Endpoint": "test_region.log.aliyuncs.com"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(STRING_FLAG(default_region_name), flusher->mRegion);
    EnterpriseConfigProvider::GetInstance()->mIsPrivateCloud = false;
    SenderQueueManager::GetInstance()->Clear();
#endif

#ifdef __ENTERPRISE__
    // EndpointMode && Endpoint
    EnterpriseSLSClientManager::GetInstance()->Clear();
    // Endpoint ignored in accelerate mode
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region",
            "EndpointMode": "accelerate",
            "Endpoint": "  test_region.log.aliyuncs.com   "
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(EndpointMode::ACCELERATE, flusher->mEndpointMode);
    APSARA_TEST_EQUAL(EnterpriseSLSClientManager::GetInstance()->mRegionCandidateEndpointsMap.end(),
                      EnterpriseSLSClientManager::GetInstance()->mRegionCandidateEndpointsMap.find("test_region"));
    APSARA_TEST_EQUAL(flusher->mProject, flusher->mCandidateHostsInfo->GetProject());
    APSARA_TEST_EQUAL(flusher->mRegion, flusher->mCandidateHostsInfo->GetRegion());
    APSARA_TEST_EQUAL(EndpointMode::ACCELERATE, flusher->mCandidateHostsInfo->GetMode());
    SenderQueueManager::GetInstance()->Clear();

    // region remote endpoints not existed
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region",
            "EndpointMode": "unknown",
            "Endpoint": "  test_region.log.aliyuncs.com   "
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(EndpointMode::DEFAULT, flusher->mEndpointMode);
    auto& endpoints
        = EnterpriseSLSClientManager::GetInstance()->mRegionCandidateEndpointsMap["test_region"].mRemoteEndpoints;
    APSARA_TEST_EQUAL(1U, endpoints.size());
    APSARA_TEST_EQUAL("test_region.log.aliyuncs.com", endpoints[0]);
    APSARA_TEST_EQUAL(flusher->mProject, flusher->mCandidateHostsInfo->GetProject());
    APSARA_TEST_EQUAL(flusher->mRegion, flusher->mCandidateHostsInfo->GetRegion());
    APSARA_TEST_EQUAL(EndpointMode::DEFAULT, flusher->mCandidateHostsInfo->GetMode());
    SenderQueueManager::GetInstance()->Clear();

    // region remote endpoints existed
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region",
            "EndpointMode": "default",
            "Endpoint": "  test_region-intranet.log.aliyuncs.com   "
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(EndpointMode::DEFAULT, flusher->mEndpointMode);
    endpoints = EnterpriseSLSClientManager::GetInstance()->mRegionCandidateEndpointsMap["test_region"].mRemoteEndpoints;
    APSARA_TEST_EQUAL(2U, endpoints.size());
    APSARA_TEST_EQUAL("test_region.log.aliyuncs.com", endpoints[0]);
    APSARA_TEST_EQUAL("test_region-intranet.log.aliyuncs.com", endpoints[1]);
    APSARA_TEST_EQUAL(flusher->mProject, flusher->mCandidateHostsInfo->GetProject());
    APSARA_TEST_EQUAL(flusher->mRegion, flusher->mCandidateHostsInfo->GetRegion());
    APSARA_TEST_EQUAL(EndpointMode::DEFAULT, flusher->mCandidateHostsInfo->GetMode());
    SenderQueueManager::GetInstance()->Clear();
#endif

#ifndef __ENTERPRISE__
    // Endpoint
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region",
            "Endpoint": "  test_region.log.aliyuncs.com   "
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL("test_region.log.aliyuncs.com", flusher->mEndpoint);
    SenderQueueManager::GetInstance()->Clear();
#endif

    // TelemetryType
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region",
            "Endpoint": "test_region.log.aliyuncs.com",
            "TelemetryType": "metrics"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(sls_logs::SlsTelemetryType::SLS_TELEMETRY_TYPE_METRICS, flusher->mTelemetryType);
    SenderQueueManager::GetInstance()->Clear();

    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region",
            "Endpoint": "test_region.log.aliyuncs.com",
            "TelemetryType": "unknown"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(sls_logs::SlsTelemetryType::SLS_TELEMETRY_TYPE_LOGS, flusher->mTelemetryType);
    SenderQueueManager::GetInstance()->Clear();

    // ShardHashKeys
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region",
            "Endpoint": "test_region.log.aliyuncs.com",
            "ShardHashKeys": [
                "__source__"
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    ctx.SetExactlyOnceFlag(true);
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
    APSARA_TEST_TRUE(flusher->mShardHashKeys.empty());
    ctx.SetExactlyOnceFlag(false);
    SenderQueueManager::GetInstance()->Clear();

    // group batch && sender queue
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region",
            "Endpoint": "test_region.log.aliyuncs.com",
            "ShardHashKeys": [
                "__source__"
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
    APSARA_TEST_FALSE(flusher->mBatcher.GetGroupFlushStrategy().has_value());
    SenderQueueManager::GetInstance()->Clear();

    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region",
            "Endpoint": "test_region.log.aliyuncs.com"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    ctx.SetExactlyOnceFlag(true);
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
    APSARA_TEST_FALSE(flusher->mBatcher.GetGroupFlushStrategy().has_value());
    APSARA_TEST_EQUAL(nullptr, SenderQueueManager::GetInstance()->GetQueue(flusher->GetQueueKey()));
    ctx.SetExactlyOnceFlag(false);
    SenderQueueManager::GetInstance()->Clear();

    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region",
            "Endpoint": "test_region.log.aliyuncs.com",
            "TelemetryType": "metrics"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
    APSARA_TEST_FALSE(flusher->mBatcher.GetGroupFlushStrategy().has_value());
    SenderQueueManager::GetInstance()->Clear();

    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region",
            "Endpoint": "test_region.log.aliyuncs.com",
            "TelemetryType": "metrics_multivalue"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
    APSARA_TEST_FALSE(flusher->mBatcher.GetGroupFlushStrategy().has_value());
    SenderQueueManager::GetInstance()->Clear();

    // apm
    std::vector<std::string> apmConfigStr = {R"(
        {
            "Type": "flusher_sls",
            "TelemetryType": "arms_traces",
            "Project": "test_project",
            "Region": "test_region",
            "Endpoint": "test_endpoint",
            "Match": {
                "Type": "tag",
                "Key": "data_type",
                "Value": "trace"
            }
        }
    )",
                                             R"(
        {
            "Type": "flusher_sls",
            "TelemetryType": "arms_metrics",
            "Project": "test_project",
            "Region": "test_region",
            "Endpoint": "test_endpoint",
            "Match": {
                "Type": "tag",
                "Key": "data_type",
                "Value": "metric"
            }
        }
    )",
                                             R"(
        {
            "Type": "flusher_sls",
            "TelemetryType": "arms_agentinfo",
            "Project": "test_project",
            "Region": "test_region",
            "Endpoint": "test_endpoint",
            "Match": {
                "Type": "tag",
                "Key": "data_type",
                "Value": "agent_info"
            }
        }
    )"};
    std::vector<std::string> apmSubpath = {APM_TRACES_URL, APM_METRICS_URL, APM_AGENTINFOS_URL};
    std::vector<sls_logs::SlsTelemetryType> apmTelemetryTypes = {
        sls_logs::SlsTelemetryType::SLS_TELEMETRY_TYPE_APM_TRACES,
        sls_logs::SlsTelemetryType::SLS_TELEMETRY_TYPE_APM_METRICS,
        sls_logs::SlsTelemetryType::SLS_TELEMETRY_TYPE_APM_AGENTINFOS,
    };
    for (size_t ii = 0; ii < apmConfigStr.size(); ii++) {
        auto& cfg = apmConfigStr[ii];
        APSARA_TEST_TRUE(ParseJsonTable(cfg, configJson, errorMsg));
        flusher.reset(new FlusherSLS());
        flusher->SetContext(ctx);
        flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
        APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
        flusher->CommitMetricsRecordRef();
        APSARA_TEST_EQUAL(flusher->mSubpath, apmSubpath[ii]);
        APSARA_TEST_EQUAL(flusher->mTelemetryType, apmTelemetryTypes[ii]);
        SenderQueueManager::GetInstance()->Clear();
    }

    // go param
    ctx.SetIsFlushingThroughGoPipelineFlag(true);
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com"
        }
    )";
    optionalGoPipelineStr = R"(
        {
            "flushers": [
                {
                    "type": "flusher_sls/4",
                    "detail": {}
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(optionalGoPipelineStr, optionalGoPipelineJson, errorMsg));
    pipeline.mPluginID.store(4);
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
    APSARA_TEST_TRUE(optionalGoPipelineJson == optionalGoPipeline);
    optionalGoPipeline.clear();

    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region",
            "Endpoint": "test_region.log.aliyuncs.com",
            "EnableShardHash": false
        }
    )";
    optionalGoPipelineStr = R"(
        {
            "flushers": [
                {
                    "type": "flusher_sls/4",
                    "detail": {
                        "EnableShardHash": false
                    }
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    APSARA_TEST_TRUE(ParseJsonTable(optionalGoPipelineStr, optionalGoPipelineJson, errorMsg));
    pipeline.mPluginID.store(4);
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(optionalGoPipelineJson.toStyledString(), optionalGoPipeline.toStyledString());
    ctx.SetIsFlushingThroughGoPipelineFlag(false);
    SenderQueueManager::GetInstance()->Clear();
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
            "Endpoint": "test_region.log.aliyuncs.com"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_FALSE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();

    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": true,
            "Logstore": "test_logstore",
            "Endpoint": "test_region.log.aliyuncs.com"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_FALSE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();

    // invalid Logstore
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Endpoint": "test_region.log.aliyuncs.com"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_FALSE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();

    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": true,
            "Endpoint": "test_region.log.aliyuncs.com"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_FALSE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();

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
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_FALSE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();

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
    flusher->CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_FALSE(flusher->Init(configJson, optionalGoPipeline));
    flusher->CommitMetricsRecordRef();
#endif
}

void FlusherSLSUnittest::OnPipelineUpdate() {
    CollectionPipelineContext ctx1;
    ctx1.SetConfigName("test_config_1");

    Json::Value configJson, optionalGoPipeline;
    FlusherSLS flusher1;
    flusher1.SetContext(ctx1);
    flusher1.CreateMetricsRecordRef(FlusherSLS::sName, "1");
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region",
            "Endpoint": "test_region.log.aliyuncs.com"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    APSARA_TEST_TRUE(flusher1.Init(configJson, optionalGoPipeline));
    flusher1.CommitMetricsRecordRef();
    APSARA_TEST_TRUE(flusher1.Start());
    APSARA_TEST_EQUAL(1U, FlusherSLS::sProjectRefCntMap.size());

    {
        CollectionPipelineContext ctx2;
        ctx2.SetConfigName("test_config_2");
        FlusherSLS flusher2;
        flusher2.SetContext(ctx2);
        flusher2.CreateMetricsRecordRef(FlusherSLS::sName, "1");
        configStr = R"(
            {
                "Type": "flusher_sls",
                "Project": "test_project_2",
                "Logstore": "test_logstore_2",
                "Region": "test_region",
                "Endpoint": "test_region.log.aliyuncs.com",
                "Aliuid": "123456789"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        APSARA_TEST_TRUE(flusher2.Init(configJson, optionalGoPipeline));
        flusher2.CommitMetricsRecordRef();

        APSARA_TEST_TRUE(flusher1.Stop(false));
        APSARA_TEST_TRUE(FlusherSLS::sProjectRefCntMap.empty());
        APSARA_TEST_TRUE(SenderQueueManager::GetInstance()->IsQueueMarkedDeleted(flusher1.GetQueueKey()));

        APSARA_TEST_TRUE(flusher2.Start());
        APSARA_TEST_EQUAL(1U, FlusherSLS::sProjectRefCntMap.size());
        APSARA_TEST_TRUE(SenderQueueManager::GetInstance()->IsQueueMarkedDeleted(flusher1.GetQueueKey()));
        APSARA_TEST_FALSE(SenderQueueManager::GetInstance()->IsQueueMarkedDeleted(flusher2.GetQueueKey()));
        flusher2.Stop(true);
        flusher1.Start();
    }
    {
        CollectionPipelineContext ctx2;
        ctx2.SetConfigName("test_config_1");
        FlusherSLS flusher2;
        flusher2.SetContext(ctx2);
        flusher2.CreateMetricsRecordRef(FlusherSLS::sName, "1");
        configStr = R"(
            {
                "Type": "flusher_sls",
                "Project": "test_project",
                "Logstore": "test_logstore",
                "Region": "test_region",
                "Endpoint": "test_region.log.aliyuncs.com",
                "Aliuid": "123456789"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        APSARA_TEST_TRUE(flusher2.Init(configJson, optionalGoPipeline));
        flusher2.CommitMetricsRecordRef();

        APSARA_TEST_TRUE(flusher1.Stop(false));
        APSARA_TEST_TRUE(SenderQueueManager::GetInstance()->IsQueueMarkedDeleted(flusher1.GetQueueKey()));

        APSARA_TEST_TRUE(flusher2.Start());
        APSARA_TEST_FALSE(SenderQueueManager::GetInstance()->IsQueueMarkedDeleted(flusher1.GetQueueKey()));
        APSARA_TEST_FALSE(SenderQueueManager::GetInstance()->IsQueueMarkedDeleted(flusher2.GetQueueKey()));
        flusher2.Stop(true);
    }
}

void FlusherSLSUnittest::TestBuildRequest() {
#ifdef __ENTERPRISE__
    EnterpriseSLSClientManager::GetInstance()->UpdateLocalRegionEndpointsAndHttpsInfo("test_region",
                                                                                      {kAccelerationDataEndpoint});
    EnterpriseSLSClientManager::GetInstance()->UpdateRemoteRegionEndpoints(
        "test_region", {"test_region-intranet.log.aliyuncs.com", "test_region.log.aliyuncs.com"});
    EnterpriseSLSClientManager::GetInstance()->UpdateRemoteRegionEndpoints(
        "test_region-b", {"test_region-b-intranet.log.aliyuncs.com", "test_region-b.log.aliyuncs.com"});
#endif
    Json::Value configJson, optionalGoPipeline;
    string errorMsg;
    string configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region-b",
            "Aliuid": "1234567890",
            "Endpoint": "test_endpoint"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    FlusherSLS flusher;
    flusher.SetContext(ctx);
    flusher.CreateMetricsRecordRef(FlusherSLS::sName, "1");
    APSARA_TEST_TRUE(flusher.Init(configJson, optionalGoPipeline));
    flusher.CommitMetricsRecordRef();

    string body = "hello, world!";
    string bodyLenStr = to_string(body.size());
    uint32_t rawSize = 100;
    string rawSizeStr = "100";

    SLSSenderQueueItem item("hello, world!", rawSize, &flusher, flusher.GetQueueKey(), flusher.mLogstore);
    unique_ptr<HttpSinkRequest> req;
    bool keepItem = false;
    string errMsg;
#ifdef __ENTERPRISE__
    {
        // empty ak, first try
        APSARA_TEST_FALSE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        // empty ak, second try
        APSARA_TEST_FALSE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        APSARA_TEST_EQUAL(nullptr, req);
        APSARA_TEST_TRUE(keepItem);
    }
    EnterpriseSLSClientManager::GetInstance()->SetAccessKey(
        "1234567890", SLSClientManager::AuthType::ANONYMOUS, "test_ak", "test_sk");
    {
        // no available host, uninitialized
        APSARA_TEST_FALSE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        APSARA_TEST_EQUAL(nullptr, req);
        APSARA_TEST_TRUE(keepItem);
        APSARA_TEST_EQUAL(static_cast<uint32_t>(AppConfig::GetInstance()->GetSendRequestConcurrency()),
                          FlusherSLS::GetRegionConcurrencyLimiter(flusher.mRegion)->GetCurrentLimit());
    }
    {
        // no available host, initialized
        flusher.mCandidateHostsInfo->SetInitialized();
        APSARA_TEST_FALSE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        APSARA_TEST_EQUAL(nullptr, req);
        APSARA_TEST_TRUE(keepItem);
        APSARA_TEST_EQUAL(static_cast<uint32_t>(AppConfig::GetInstance()->GetSendRequestConcurrency()),
                          FlusherSLS::GetRegionConcurrencyLimiter(flusher.mRegion)->GetCurrentLimit());
    }
    EnterpriseSLSClientManager::GetInstance()->UpdateHostLatency("test_project",
                                                                 EndpointMode::DEFAULT,
                                                                 "test_project.test_region-b.log.aliyuncs.com",
                                                                 chrono::milliseconds(100));
    flusher.mCandidateHostsInfo->SelectBestHost();
#endif
    // log telemetry type
    {
        // normal
        SLSSenderQueueItem item("hello, world!", rawSize, &flusher, flusher.GetQueueKey(), flusher.mLogstore);
        APSARA_TEST_TRUE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        APSARA_TEST_EQUAL(HTTP_POST, req->mMethod);
#ifdef __ENTERPRISE__
        APSARA_TEST_FALSE(req->mHTTPSFlag);
#else
        APSARA_TEST_TRUE(req->mHTTPSFlag);
#endif
        APSARA_TEST_EQUAL("/logstores/test_logstore/shards/lb", req->mUrl);
        APSARA_TEST_EQUAL("", req->mQueryString);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(12U, req->mHeader.size());
#else
        APSARA_TEST_EQUAL(11U, req->mHeader.size());
#endif
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", req->mHeader[HOST]);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", req->mHeader[HOST]);
#endif
        APSARA_TEST_EQUAL(SLSClientManager::GetInstance()->GetUserAgent(), req->mHeader[USER_AGENT]);
        APSARA_TEST_FALSE(req->mHeader[DATE].empty());
        APSARA_TEST_EQUAL(TYPE_LOG_PROTOBUF, req->mHeader[CONTENT_TYPE]);
        APSARA_TEST_EQUAL(bodyLenStr, req->mHeader[CONTENT_LENGTH]);
        APSARA_TEST_EQUAL(CalcMD5(req->mBody), req->mHeader[CONTENT_MD5]);
        APSARA_TEST_EQUAL(LOG_API_VERSION, req->mHeader[X_LOG_APIVERSION]);
        APSARA_TEST_EQUAL(HMAC_SHA1, req->mHeader[X_LOG_SIGNATUREMETHOD]);
        APSARA_TEST_EQUAL("lz4", req->mHeader[X_LOG_COMPRESSTYPE]);
        APSARA_TEST_EQUAL(rawSizeStr, req->mHeader[X_LOG_BODYRAWSIZE]);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(MD5_SHA1_SALT_KEYPROVIDER, req->mHeader[X_LOG_KEYPROVIDER]);
#endif
        APSARA_TEST_FALSE(req->mHeader[AUTHORIZATION].empty());
        APSARA_TEST_EQUAL(body, req->mBody);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", req->mHost);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", req->mHost);
#endif
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(80, req->mPort);
#else
        APSARA_TEST_EQUAL(443, req->mPort);
#endif
        APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(default_http_request_timeout_sec)), req->mTimeout);
        APSARA_TEST_EQUAL(1U, req->mMaxTryCnt);
        APSARA_TEST_FALSE(req->mFollowRedirects);
        APSARA_TEST_EQUAL(&item, req->mItem);
        APSARA_TEST_FALSE(item.mRealIpFlag);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", item.mCurrentHost);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", item.mCurrentHost);
#endif
    }
    // arms_traces telemetry type
    {
        flusher.mTelemetryType = sls_logs::SLS_TELEMETRY_TYPE_APM_TRACES;
        flusher.mSubpath = APM_TRACES_URL;
        // normal
        SLSSenderQueueItem item("hello, world!", rawSize, &flusher, flusher.GetQueueKey(), flusher.mLogstore);

        APSARA_TEST_TRUE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        APSARA_TEST_EQUAL(HTTP_POST, req->mMethod);
#ifdef __ENTERPRISE__
        APSARA_TEST_FALSE(req->mHTTPSFlag);
#else
        APSARA_TEST_TRUE(req->mHTTPSFlag);
#endif
        APSARA_TEST_EQUAL(APM_TRACES_URL, req->mUrl);
        APSARA_TEST_EQUAL("", req->mQueryString);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(12U, req->mHeader.size());
#else
        APSARA_TEST_EQUAL(11U, req->mHeader.size());
#endif
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", req->mHeader[HOST]);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", req->mHeader[HOST]);
#endif
        APSARA_TEST_EQUAL(SLSClientManager::GetInstance()->GetUserAgent(), req->mHeader[USER_AGENT]);
        APSARA_TEST_FALSE(req->mHeader[DATE].empty());
        APSARA_TEST_EQUAL(TYPE_LOG_PROTOBUF, req->mHeader[CONTENT_TYPE]);
        APSARA_TEST_EQUAL(bodyLenStr, req->mHeader[CONTENT_LENGTH]);
        APSARA_TEST_EQUAL(CalcMD5(req->mBody), req->mHeader[CONTENT_MD5]);
        APSARA_TEST_EQUAL(LOG_API_VERSION, req->mHeader[X_LOG_APIVERSION]);
        APSARA_TEST_EQUAL(HMAC_SHA1, req->mHeader[X_LOG_SIGNATUREMETHOD]);
        APSARA_TEST_EQUAL("lz4", req->mHeader[X_LOG_COMPRESSTYPE]);
        APSARA_TEST_EQUAL(rawSizeStr, req->mHeader[X_LOG_BODYRAWSIZE]);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(MD5_SHA1_SALT_KEYPROVIDER, req->mHeader[X_LOG_KEYPROVIDER]);
#endif
        APSARA_TEST_FALSE(req->mHeader[AUTHORIZATION].empty());
        APSARA_TEST_EQUAL(body, req->mBody);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", req->mHost);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", req->mHost);
#endif
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(80, req->mPort);
#else
        APSARA_TEST_EQUAL(443, req->mPort);
#endif
        APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(default_http_request_timeout_sec)), req->mTimeout);
        APSARA_TEST_EQUAL(1U, req->mMaxTryCnt);
        APSARA_TEST_FALSE(req->mFollowRedirects);
        APSARA_TEST_EQUAL(&item, req->mItem);
        APSARA_TEST_FALSE(item.mRealIpFlag);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", item.mCurrentHost);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", item.mCurrentHost);
#endif
    }
    // arms_metrics telemetry type
    {
        flusher.mTelemetryType = sls_logs::SLS_TELEMETRY_TYPE_APM_METRICS;
        flusher.mSubpath = APM_METRICS_URL;
        // normal
        SLSSenderQueueItem item("hello, world!", rawSize, &flusher, flusher.GetQueueKey(), flusher.mLogstore);

        APSARA_TEST_TRUE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        APSARA_TEST_EQUAL(HTTP_POST, req->mMethod);
#ifdef __ENTERPRISE__
        APSARA_TEST_FALSE(req->mHTTPSFlag);
#else
        APSARA_TEST_TRUE(req->mHTTPSFlag);
#endif
        APSARA_TEST_EQUAL(APM_METRICS_URL, req->mUrl);
        APSARA_TEST_EQUAL("", req->mQueryString);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(12U, req->mHeader.size());
#else
        APSARA_TEST_EQUAL(11U, req->mHeader.size());
#endif
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", req->mHeader[HOST]);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", req->mHeader[HOST]);
#endif
        APSARA_TEST_EQUAL(SLSClientManager::GetInstance()->GetUserAgent(), req->mHeader[USER_AGENT]);
        APSARA_TEST_FALSE(req->mHeader[DATE].empty());
        APSARA_TEST_EQUAL(TYPE_LOG_PROTOBUF, req->mHeader[CONTENT_TYPE]);
        APSARA_TEST_EQUAL(bodyLenStr, req->mHeader[CONTENT_LENGTH]);
        APSARA_TEST_EQUAL(CalcMD5(req->mBody), req->mHeader[CONTENT_MD5]);
        APSARA_TEST_EQUAL(LOG_API_VERSION, req->mHeader[X_LOG_APIVERSION]);
        APSARA_TEST_EQUAL(HMAC_SHA1, req->mHeader[X_LOG_SIGNATUREMETHOD]);
        APSARA_TEST_EQUAL("lz4", req->mHeader[X_LOG_COMPRESSTYPE]);
        APSARA_TEST_EQUAL(rawSizeStr, req->mHeader[X_LOG_BODYRAWSIZE]);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(MD5_SHA1_SALT_KEYPROVIDER, req->mHeader[X_LOG_KEYPROVIDER]);
#endif
        APSARA_TEST_FALSE(req->mHeader[AUTHORIZATION].empty());
        APSARA_TEST_EQUAL(body, req->mBody);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", req->mHost);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", req->mHost);
#endif
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(80, req->mPort);
#else
        APSARA_TEST_EQUAL(443, req->mPort);
#endif
        APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(default_http_request_timeout_sec)), req->mTimeout);
        APSARA_TEST_EQUAL(1U, req->mMaxTryCnt);
        APSARA_TEST_FALSE(req->mFollowRedirects);
        APSARA_TEST_EQUAL(&item, req->mItem);
        APSARA_TEST_FALSE(item.mRealIpFlag);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", item.mCurrentHost);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", item.mCurrentHost);
#endif
    }
    // arms_agentinfo telemetry type
    {
        flusher.mTelemetryType = sls_logs::SLS_TELEMETRY_TYPE_APM_AGENTINFOS;
        flusher.mSubpath = APM_AGENTINFOS_URL;
        // normal
        SLSSenderQueueItem item("hello, world!", rawSize, &flusher, flusher.GetQueueKey(), flusher.mLogstore);

        APSARA_TEST_TRUE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        APSARA_TEST_EQUAL(HTTP_POST, req->mMethod);
#ifdef __ENTERPRISE__
        APSARA_TEST_FALSE(req->mHTTPSFlag);
#else
        APSARA_TEST_TRUE(req->mHTTPSFlag);
#endif
        APSARA_TEST_EQUAL(APM_AGENTINFOS_URL, req->mUrl);
        APSARA_TEST_EQUAL("", req->mQueryString);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(12U, req->mHeader.size());
#else
        APSARA_TEST_EQUAL(11U, req->mHeader.size());
#endif
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", req->mHeader[HOST]);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", req->mHeader[HOST]);
#endif
        APSARA_TEST_EQUAL(SLSClientManager::GetInstance()->GetUserAgent(), req->mHeader[USER_AGENT]);
        APSARA_TEST_FALSE(req->mHeader[DATE].empty());
        APSARA_TEST_EQUAL(TYPE_LOG_PROTOBUF, req->mHeader[CONTENT_TYPE]);
        APSARA_TEST_EQUAL(bodyLenStr, req->mHeader[CONTENT_LENGTH]);
        APSARA_TEST_EQUAL(CalcMD5(req->mBody), req->mHeader[CONTENT_MD5]);
        APSARA_TEST_EQUAL(LOG_API_VERSION, req->mHeader[X_LOG_APIVERSION]);
        APSARA_TEST_EQUAL(HMAC_SHA1, req->mHeader[X_LOG_SIGNATUREMETHOD]);
        APSARA_TEST_EQUAL("lz4", req->mHeader[X_LOG_COMPRESSTYPE]);
        APSARA_TEST_EQUAL(rawSizeStr, req->mHeader[X_LOG_BODYRAWSIZE]);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(MD5_SHA1_SALT_KEYPROVIDER, req->mHeader[X_LOG_KEYPROVIDER]);
#endif
        APSARA_TEST_FALSE(req->mHeader[AUTHORIZATION].empty());
        APSARA_TEST_EQUAL(body, req->mBody);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", req->mHost);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", req->mHost);
#endif
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(80, req->mPort);
#else
        APSARA_TEST_EQUAL(443, req->mPort);
#endif
        APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(default_http_request_timeout_sec)), req->mTimeout);
        APSARA_TEST_EQUAL(1U, req->mMaxTryCnt);
        APSARA_TEST_FALSE(req->mFollowRedirects);
        APSARA_TEST_EQUAL(&item, req->mItem);
        APSARA_TEST_FALSE(item.mRealIpFlag);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", item.mCurrentHost);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", item.mCurrentHost);
#endif
    }
    flusher.mTelemetryType = sls_logs::SLS_TELEMETRY_TYPE_LOGS;

    {
        // event group list
        SLSSenderQueueItem item("hello, world!",
                                rawSize,
                                &flusher,
                                flusher.GetQueueKey(),
                                flusher.mLogstore,
                                RawDataType::EVENT_GROUP_LIST);
        APSARA_TEST_TRUE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        APSARA_TEST_EQUAL(HTTP_POST, req->mMethod);
#ifdef __ENTERPRISE__
        APSARA_TEST_FALSE(req->mHTTPSFlag);
#else
        APSARA_TEST_TRUE(req->mHTTPSFlag);
#endif
        APSARA_TEST_EQUAL("/logstores/test_logstore/shards/lb", req->mUrl);
        APSARA_TEST_EQUAL("", req->mQueryString);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(13U, req->mHeader.size());
#else
        APSARA_TEST_EQUAL(12U, req->mHeader.size());
#endif
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", req->mHeader[HOST]);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", req->mHeader[HOST]);
#endif
        APSARA_TEST_EQUAL(SLSClientManager::GetInstance()->GetUserAgent(), req->mHeader[USER_AGENT]);
        APSARA_TEST_FALSE(req->mHeader[DATE].empty());
        APSARA_TEST_EQUAL(TYPE_LOG_PROTOBUF, req->mHeader[CONTENT_TYPE]);
        APSARA_TEST_EQUAL(bodyLenStr, req->mHeader[CONTENT_LENGTH]);
        APSARA_TEST_EQUAL(CalcMD5(req->mBody), req->mHeader[CONTENT_MD5]);
        APSARA_TEST_EQUAL(LOG_API_VERSION, req->mHeader[X_LOG_APIVERSION]);
        APSARA_TEST_EQUAL(HMAC_SHA1, req->mHeader[X_LOG_SIGNATUREMETHOD]);
        APSARA_TEST_EQUAL("lz4", req->mHeader[X_LOG_COMPRESSTYPE]);
        APSARA_TEST_EQUAL(bodyLenStr, req->mHeader[X_LOG_BODYRAWSIZE]);
        APSARA_TEST_EQUAL(LOG_MODE_BATCH_GROUP, req->mHeader[X_LOG_MODE]);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(MD5_SHA1_SALT_KEYPROVIDER, req->mHeader[X_LOG_KEYPROVIDER]);
#endif
        APSARA_TEST_FALSE(req->mHeader[AUTHORIZATION].empty());
        APSARA_TEST_EQUAL(body, req->mBody);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", req->mHost);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", req->mHost);
#endif
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(80, req->mPort);
#else
        APSARA_TEST_EQUAL(443, req->mPort);
#endif
        APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(default_http_request_timeout_sec)), req->mTimeout);
        APSARA_TEST_EQUAL(1U, req->mMaxTryCnt);
        APSARA_TEST_FALSE(req->mFollowRedirects);
        APSARA_TEST_EQUAL(&item, req->mItem);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", item.mCurrentHost);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", item.mCurrentHost);
#endif
    }
    {
        // shard hash
        SLSSenderQueueItem item("hello, world!",
                                rawSize,
                                &flusher,
                                flusher.GetQueueKey(),
                                flusher.mLogstore,
                                RawDataType::EVENT_GROUP,
                                "hash_key");
        APSARA_TEST_TRUE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        APSARA_TEST_EQUAL(HTTP_POST, req->mMethod);
#ifdef __ENTERPRISE__
        APSARA_TEST_FALSE(req->mHTTPSFlag);
#else
        APSARA_TEST_TRUE(req->mHTTPSFlag);
#endif
        APSARA_TEST_EQUAL("/logstores/test_logstore/shards/route", req->mUrl);
        map<string, string> params{{"key", "hash_key"}};
        APSARA_TEST_EQUAL(GetQueryString(params), req->mQueryString);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(12U, req->mHeader.size());
#else
        APSARA_TEST_EQUAL(11U, req->mHeader.size());
#endif
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", req->mHeader[HOST]);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", req->mHeader[HOST]);
#endif
        APSARA_TEST_EQUAL(SLSClientManager::GetInstance()->GetUserAgent(), req->mHeader[USER_AGENT]);
        APSARA_TEST_FALSE(req->mHeader[DATE].empty());
        APSARA_TEST_EQUAL(TYPE_LOG_PROTOBUF, req->mHeader[CONTENT_TYPE]);
        APSARA_TEST_EQUAL(bodyLenStr, req->mHeader[CONTENT_LENGTH]);
        APSARA_TEST_EQUAL(CalcMD5(req->mBody), req->mHeader[CONTENT_MD5]);
        APSARA_TEST_EQUAL(LOG_API_VERSION, req->mHeader[X_LOG_APIVERSION]);
        APSARA_TEST_EQUAL(HMAC_SHA1, req->mHeader[X_LOG_SIGNATUREMETHOD]);
        APSARA_TEST_EQUAL("lz4", req->mHeader[X_LOG_COMPRESSTYPE]);
        APSARA_TEST_EQUAL(rawSizeStr, req->mHeader[X_LOG_BODYRAWSIZE]);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(MD5_SHA1_SALT_KEYPROVIDER, req->mHeader[X_LOG_KEYPROVIDER]);
#endif
        APSARA_TEST_FALSE(req->mHeader[AUTHORIZATION].empty());
        APSARA_TEST_EQUAL(body, req->mBody);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", req->mHost);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", req->mHost);
#endif
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(80, req->mPort);
#else
        APSARA_TEST_EQUAL(443, req->mPort);
#endif
        APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(default_http_request_timeout_sec)), req->mTimeout);
        APSARA_TEST_EQUAL(1U, req->mMaxTryCnt);
        APSARA_TEST_FALSE(req->mFollowRedirects);
        APSARA_TEST_EQUAL(&item, req->mItem);
        APSARA_TEST_FALSE(item.mRealIpFlag);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", item.mCurrentHost);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", item.mCurrentHost);
#endif
    }
    {
        // exactly once
        auto cpt = make_shared<RangeCheckpoint>();
        cpt->index = 0;
        cpt->data.set_hash_key("hash_key_0");
        cpt->data.set_sequence_id(1);
        SLSSenderQueueItem item("hello, world!",
                                rawSize,
                                &flusher,
                                flusher.GetQueueKey(),
                                flusher.mLogstore,
                                RawDataType::EVENT_GROUP,
                                "hash_key_0",
                                std::move(cpt));
        APSARA_TEST_TRUE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        APSARA_TEST_EQUAL(HTTP_POST, req->mMethod);
#ifdef __ENTERPRISE__
        APSARA_TEST_FALSE(req->mHTTPSFlag);
#else
        APSARA_TEST_TRUE(req->mHTTPSFlag);
#endif
        APSARA_TEST_EQUAL("/logstores/test_logstore/shards/route", req->mUrl);
        map<string, string> params{{"key", "hash_key_0"}, {"seqid", "1"}};
        APSARA_TEST_EQUAL(GetQueryString(params), req->mQueryString);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(12U, req->mHeader.size());
#else
        APSARA_TEST_EQUAL(11U, req->mHeader.size());
#endif
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", req->mHeader[HOST]);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", req->mHeader[HOST]);
#endif
        APSARA_TEST_EQUAL(SLSClientManager::GetInstance()->GetUserAgent(), req->mHeader[USER_AGENT]);
        APSARA_TEST_FALSE(req->mHeader[DATE].empty());
        APSARA_TEST_EQUAL(TYPE_LOG_PROTOBUF, req->mHeader[CONTENT_TYPE]);
        APSARA_TEST_EQUAL(bodyLenStr, req->mHeader[CONTENT_LENGTH]);
        APSARA_TEST_EQUAL(CalcMD5(req->mBody), req->mHeader[CONTENT_MD5]);
        APSARA_TEST_EQUAL(LOG_API_VERSION, req->mHeader[X_LOG_APIVERSION]);
        APSARA_TEST_EQUAL(HMAC_SHA1, req->mHeader[X_LOG_SIGNATUREMETHOD]);
        APSARA_TEST_EQUAL("lz4", req->mHeader[X_LOG_COMPRESSTYPE]);
        APSARA_TEST_EQUAL(rawSizeStr, req->mHeader[X_LOG_BODYRAWSIZE]);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(MD5_SHA1_SALT_KEYPROVIDER, req->mHeader[X_LOG_KEYPROVIDER]);
#endif
        APSARA_TEST_FALSE(req->mHeader[AUTHORIZATION].empty());
        APSARA_TEST_EQUAL(body, req->mBody);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", req->mHost);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", req->mHost);
#endif
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(80, req->mPort);
#else
        APSARA_TEST_EQUAL(443, req->mPort);
#endif
        APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(default_http_request_timeout_sec)), req->mTimeout);
        APSARA_TEST_EQUAL(1U, req->mMaxTryCnt);
        APSARA_TEST_FALSE(req->mFollowRedirects);
        APSARA_TEST_EQUAL(&item, req->mItem);
        APSARA_TEST_FALSE(item.mRealIpFlag);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", item.mCurrentHost);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", item.mCurrentHost);
#endif
    }
    // metric telemtery type
    flusher.mTelemetryType = sls_logs::SlsTelemetryType::SLS_TELEMETRY_TYPE_METRICS;
    {
        SLSSenderQueueItem item("hello, world!", rawSize, &flusher, flusher.GetQueueKey(), flusher.mLogstore);
        APSARA_TEST_TRUE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        APSARA_TEST_EQUAL(HTTP_POST, req->mMethod);
#ifdef __ENTERPRISE__
        APSARA_TEST_FALSE(req->mHTTPSFlag);
#else
        APSARA_TEST_TRUE(req->mHTTPSFlag);
#endif
        APSARA_TEST_EQUAL("/prometheus/test_project/test_logstore/api/v1/write", req->mUrl);
        APSARA_TEST_EQUAL("", req->mQueryString);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(12U, req->mHeader.size());
#else
        APSARA_TEST_EQUAL(11U, req->mHeader.size());
#endif
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", req->mHeader[HOST]);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", req->mHeader[HOST]);
#endif
        APSARA_TEST_EQUAL(SLSClientManager::GetInstance()->GetUserAgent(), req->mHeader[USER_AGENT]);
        APSARA_TEST_FALSE(req->mHeader[DATE].empty());
        APSARA_TEST_EQUAL(TYPE_LOG_PROTOBUF, req->mHeader[CONTENT_TYPE]);
        APSARA_TEST_EQUAL(bodyLenStr, req->mHeader[CONTENT_LENGTH]);
        APSARA_TEST_EQUAL(CalcMD5(req->mBody), req->mHeader[CONTENT_MD5]);
        APSARA_TEST_EQUAL(LOG_API_VERSION, req->mHeader[X_LOG_APIVERSION]);
        APSARA_TEST_EQUAL(HMAC_SHA1, req->mHeader[X_LOG_SIGNATUREMETHOD]);
        APSARA_TEST_EQUAL("lz4", req->mHeader[X_LOG_COMPRESSTYPE]);
        APSARA_TEST_EQUAL(rawSizeStr, req->mHeader[X_LOG_BODYRAWSIZE]);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(MD5_SHA1_SALT_KEYPROVIDER, req->mHeader[X_LOG_KEYPROVIDER]);
#endif
        APSARA_TEST_FALSE(req->mHeader[AUTHORIZATION].empty());
        APSARA_TEST_EQUAL(body, req->mBody);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", req->mHost);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", req->mHost);
#endif
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL(80, req->mPort);
#else
        APSARA_TEST_EQUAL(443, req->mPort);
#endif
        APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(default_http_request_timeout_sec)), req->mTimeout);
        APSARA_TEST_EQUAL(1U, req->mMaxTryCnt);
        APSARA_TEST_FALSE(req->mFollowRedirects);
        APSARA_TEST_EQUAL(&item, req->mItem);
        APSARA_TEST_FALSE(item.mRealIpFlag);
#ifdef __ENTERPRISE__
        APSARA_TEST_EQUAL("test_project.test_region-b.log.aliyuncs.com", item.mCurrentHost);
#else
        APSARA_TEST_EQUAL("test_project.test_endpoint", item.mCurrentHost);
#endif
    }
    flusher.mTelemetryType = sls_logs::SlsTelemetryType::SLS_TELEMETRY_TYPE_LOGS;
#ifdef __ENTERPRISE__
    {
        // region mode changed
        EnterpriseSLSClientManager::GetInstance()->CopyLocalRegionEndpointsAndHttpsInfoIfNotExisted("test_region",
                                                                                                    "test_region-b");
        auto old = flusher.mCandidateHostsInfo.get();
        APSARA_TEST_FALSE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        APSARA_TEST_NOT_EQUAL(old, flusher.mCandidateHostsInfo.get());

        EnterpriseSLSClientManager::GetInstance()->UpdateHostLatency("test_project",
                                                                     EndpointMode::ACCELERATE,
                                                                     "test_project." + kAccelerationDataEndpoint,
                                                                     chrono::milliseconds(10));
        flusher.mCandidateHostsInfo->SelectBestHost();
        APSARA_TEST_TRUE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        APSARA_TEST_EQUAL("test_project." + kAccelerationDataEndpoint, req->mHost);
    }
    // real ip
    BOOL_FLAG(send_prefer_real_ip) = true;
    {
        // ip not empty
        EnterpriseSLSClientManager::GetInstance()->SetRealIp("test_region-b", "192.168.0.1");
        SLSSenderQueueItem item("hello, world!", rawSize, &flusher, flusher.GetQueueKey(), flusher.mLogstore);
        APSARA_TEST_TRUE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        APSARA_TEST_EQUAL(HTTP_POST, req->mMethod);
        APSARA_TEST_FALSE(req->mHTTPSFlag);
        APSARA_TEST_EQUAL("/logstores/test_logstore/shards/lb", req->mUrl);
        APSARA_TEST_EQUAL("", req->mQueryString);
        APSARA_TEST_EQUAL(12U, req->mHeader.size());
        APSARA_TEST_EQUAL("test_project.192.168.0.1", req->mHeader[HOST]);
        APSARA_TEST_EQUAL(SLSClientManager::GetInstance()->GetUserAgent(), req->mHeader[USER_AGENT]);
        APSARA_TEST_FALSE(req->mHeader[DATE].empty());
        APSARA_TEST_EQUAL(TYPE_LOG_PROTOBUF, req->mHeader[CONTENT_TYPE]);
        APSARA_TEST_EQUAL(bodyLenStr, req->mHeader[CONTENT_LENGTH]);
        APSARA_TEST_EQUAL(CalcMD5(req->mBody), req->mHeader[CONTENT_MD5]);
        APSARA_TEST_EQUAL(LOG_API_VERSION, req->mHeader[X_LOG_APIVERSION]);
        APSARA_TEST_EQUAL(HMAC_SHA1, req->mHeader[X_LOG_SIGNATUREMETHOD]);
        APSARA_TEST_EQUAL("lz4", req->mHeader[X_LOG_COMPRESSTYPE]);
        APSARA_TEST_EQUAL(rawSizeStr, req->mHeader[X_LOG_BODYRAWSIZE]);
        APSARA_TEST_EQUAL(MD5_SHA1_SALT_KEYPROVIDER, req->mHeader[X_LOG_KEYPROVIDER]);
        APSARA_TEST_FALSE(req->mHeader[AUTHORIZATION].empty());
        APSARA_TEST_EQUAL(body, req->mBody);
        APSARA_TEST_EQUAL("192.168.0.1", req->mHost);
        APSARA_TEST_EQUAL(80, req->mPort);
        APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(default_http_request_timeout_sec)), req->mTimeout);
        APSARA_TEST_EQUAL(1U, req->mMaxTryCnt);
        APSARA_TEST_FALSE(req->mFollowRedirects);
        APSARA_TEST_EQUAL(&item, req->mItem);
        APSARA_TEST_TRUE(item.mRealIpFlag);
        APSARA_TEST_EQUAL("192.168.0.1", item.mCurrentHost);
    }
    {
        // ip empty
        EnterpriseSLSClientManager::GetInstance()->SetRealIp("test_region-b", "");
        SLSSenderQueueItem item("hello, world!", rawSize, &flusher, flusher.GetQueueKey(), flusher.mLogstore);
        APSARA_TEST_TRUE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        APSARA_TEST_EQUAL("test_project." + kAccelerationDataEndpoint, req->mHeader[HOST]);
        APSARA_TEST_EQUAL(SLSClientManager::GetInstance()->GetUserAgent(), req->mHeader[USER_AGENT]);
        APSARA_TEST_FALSE(req->mHeader[DATE].empty());
        APSARA_TEST_EQUAL(TYPE_LOG_PROTOBUF, req->mHeader[CONTENT_TYPE]);
        APSARA_TEST_EQUAL(bodyLenStr, req->mHeader[CONTENT_LENGTH]);
        APSARA_TEST_EQUAL(CalcMD5(req->mBody), req->mHeader[CONTENT_MD5]);
        APSARA_TEST_EQUAL(LOG_API_VERSION, req->mHeader[X_LOG_APIVERSION]);
        APSARA_TEST_EQUAL(HMAC_SHA1, req->mHeader[X_LOG_SIGNATUREMETHOD]);
        APSARA_TEST_EQUAL("lz4", req->mHeader[X_LOG_COMPRESSTYPE]);
        APSARA_TEST_EQUAL(rawSizeStr, req->mHeader[X_LOG_BODYRAWSIZE]);
        APSARA_TEST_EQUAL(MD5_SHA1_SALT_KEYPROVIDER, req->mHeader[X_LOG_KEYPROVIDER]);
        APSARA_TEST_FALSE(req->mHeader[AUTHORIZATION].empty());
        APSARA_TEST_EQUAL(body, req->mBody);
        APSARA_TEST_EQUAL("test_project." + kAccelerationDataEndpoint, req->mHost);
        APSARA_TEST_EQUAL(80, req->mPort);
        APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(default_http_request_timeout_sec)), req->mTimeout);
        APSARA_TEST_EQUAL(1U, req->mMaxTryCnt);
        APSARA_TEST_FALSE(req->mFollowRedirects);
        APSARA_TEST_EQUAL(&item, req->mItem);
        APSARA_TEST_FALSE(item.mRealIpFlag);
        APSARA_TEST_EQUAL("test_project." + kAccelerationDataEndpoint, item.mCurrentHost);
    }
    {
        // ip empty, and region mode changed
        auto& endpoints = EnterpriseSLSClientManager::GetInstance()->mRegionCandidateEndpointsMap["test_region-b"];
        endpoints.mMode = EndpointMode::CUSTOM;
        endpoints.mLocalEndpoints = {"custom.endpoint"};

        auto old = flusher.mCandidateHostsInfo.get();
        APSARA_TEST_FALSE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        APSARA_TEST_NOT_EQUAL(old, flusher.mCandidateHostsInfo.get());

        EnterpriseSLSClientManager::GetInstance()->UpdateHostLatency(
            "test_project", EndpointMode::CUSTOM, "test_project.custom.endpoint", chrono::milliseconds(10));
        flusher.mCandidateHostsInfo->SelectBestHost();
        APSARA_TEST_TRUE(flusher.BuildRequest(&item, req, &keepItem, &errMsg));
        APSARA_TEST_EQUAL("test_project.custom.endpoint", req->mHost);
    }
    BOOL_FLAG(send_prefer_real_ip) = false;
#endif
}

void FlusherSLSUnittest::TestSend() {
    {
        // exactly once enabled
        // create flusher
        Json::Value configJson, optionalGoPipeline;
        string configStr, errorMsg;
        configStr = R"(
            {
                "Type": "flusher_sls",
                "Project": "test_project",
                "Logstore": "test_logstore",
                "Region": "test_region",
                "Endpoint": "test_region.log.aliyuncs.com",
                "Aliuid": "123456789"
            }
        )";
        ParseJsonTable(configStr, configJson, errorMsg);
        FlusherSLS flusher;
        CollectionPipelineContext ctx;
        ctx.SetConfigName("test_config");
        ctx.SetExactlyOnceFlag(true);
        flusher.SetContext(ctx);
        flusher.CreateMetricsRecordRef(FlusherSLS::sName, "1");
        flusher.Init(configJson, optionalGoPipeline);
        flusher.CommitMetricsRecordRef();

        // create exactly once queue
        vector<RangeCheckpointPtr> checkpoints;
        for (size_t i = 0; i < 2; ++i) {
            auto cpt = make_shared<RangeCheckpoint>();
            cpt->index = i;
            cpt->data.set_hash_key("hash_key_" + ToString(i));
            cpt->data.set_sequence_id(0);
            checkpoints.emplace_back(cpt);
        }
        QueueKey eooKey = QueueKeyManager::GetInstance()->GetKey("eoo");
        ExactlyOnceQueueManager::GetInstance()->CreateOrUpdateQueue(
            eooKey, ProcessQueueManager::sMaxPriority, flusher.GetContext(), checkpoints);

        {
            // replayed group
            PipelineEventGroup group(make_shared<SourceBuffer>());
            group.SetMetadata(EventGroupMetaKey::SOURCE_ID, string("source-id"));
            group.SetTag(LOG_RESERVED_KEY_SOURCE, "172.0.0.1");
            group.SetTag(LOG_RESERVED_KEY_MACHINE_UUID, "uuid");
            group.SetTag(LOG_RESERVED_KEY_TOPIC, "topic");
            auto cpt = make_shared<RangeCheckpoint>();
            cpt->index = 1;
            cpt->fbKey = eooKey;
            cpt->data.set_hash_key("hash_key_1");
            cpt->data.set_sequence_id(0);
            cpt->data.set_read_offset(0);
            cpt->data.set_read_length(10);
            group.SetExactlyOnceCheckpoint(cpt);
            auto e = group.AddLogEvent();
            e->SetTimestamp(1234567890);
            e->SetContent(string("content_key"), string("content_value"));

            APSARA_TEST_TRUE(flusher.Send(std::move(group)));
            vector<SenderQueueItem*> res;
            ExactlyOnceQueueManager::GetInstance()->GetAvailableSenderQueueItems(res, 80);
            APSARA_TEST_EQUAL(1U, res.size());
            auto item = static_cast<SLSSenderQueueItem*>(res[0]);
            APSARA_TEST_EQUAL(RawDataType::EVENT_GROUP, item->mType);
            APSARA_TEST_FALSE(item->mBufferOrNot);
            APSARA_TEST_EQUAL(&flusher, item->mFlusher);
            APSARA_TEST_EQUAL(eooKey, item->mQueueKey);
            APSARA_TEST_EQUAL("hash_key_1", item->mShardHashKey);
            APSARA_TEST_EQUAL(flusher.mLogstore, item->mLogstore);
            APSARA_TEST_EQUAL(cpt, item->mExactlyOnceCheckpoint);

            auto compressor
                = CompressorFactory::GetInstance()->Create(Json::Value(), ctx, "flusher_sls", "1", CompressType::LZ4);
            string output, errorMsg;
            output.resize(item->mRawSize);
            APSARA_TEST_TRUE(compressor->UnCompress(item->mData, output, errorMsg));

            sls_logs::LogGroup logGroup;
            APSARA_TEST_TRUE(logGroup.ParseFromString(output));
            APSARA_TEST_EQUAL("topic", logGroup.topic());
            APSARA_TEST_EQUAL("uuid", logGroup.machineuuid());
            APSARA_TEST_EQUAL("172.0.0.1", logGroup.source());
            APSARA_TEST_EQUAL(1, logGroup.logtags_size());
            APSARA_TEST_EQUAL("__pack_id__", logGroup.logtags(0).key());
            APSARA_TEST_EQUAL(1, logGroup.logs_size());
            APSARA_TEST_EQUAL(1234567890U, logGroup.logs(0).time());
            APSARA_TEST_EQUAL(1, logGroup.logs(0).contents_size());
            APSARA_TEST_EQUAL("content_key", logGroup.logs(0).contents(0).key());
            APSARA_TEST_EQUAL("content_value", logGroup.logs(0).contents(0).value());

            ExactlyOnceQueueManager::GetInstance()->RemoveSenderQueueItem(eooKey, item);
        }
        {
            // non-replay group
            flusher.mBatcher.GetEventFlushStrategy().SetMinCnt(1);
            PipelineEventGroup group(make_shared<SourceBuffer>());
            group.SetMetadata(EventGroupMetaKey::SOURCE_ID, string("source-id"));
            group.SetTag(LOG_RESERVED_KEY_SOURCE, "172.0.0.1");
            group.SetTag(LOG_RESERVED_KEY_MACHINE_UUID, "uuid");
            group.SetTag(LOG_RESERVED_KEY_TOPIC, "topic");
            auto cpt = make_shared<RangeCheckpoint>();
            cpt->fbKey = eooKey;
            cpt->data.set_read_offset(0);
            cpt->data.set_read_length(10);
            group.SetExactlyOnceCheckpoint(cpt);
            auto e = group.AddLogEvent();
            e->SetTimestamp(1234567890);
            e->SetContent(string("content_key"), string("content_value"));

            APSARA_TEST_TRUE(flusher.Send(std::move(group)));
            vector<SenderQueueItem*> res;
            ExactlyOnceQueueManager::GetInstance()->GetAvailableSenderQueueItems(res, 80);
            APSARA_TEST_EQUAL(1U, res.size());
            auto item = static_cast<SLSSenderQueueItem*>(res[0]);
            APSARA_TEST_EQUAL(RawDataType::EVENT_GROUP, item->mType);
            APSARA_TEST_FALSE(item->mBufferOrNot);
            APSARA_TEST_EQUAL(&flusher, item->mFlusher);
            APSARA_TEST_EQUAL(eooKey, item->mQueueKey);
            APSARA_TEST_EQUAL("hash_key_0", item->mShardHashKey);
            APSARA_TEST_EQUAL(flusher.mLogstore, item->mLogstore);
            APSARA_TEST_EQUAL(checkpoints[0], item->mExactlyOnceCheckpoint);

            auto compressor
                = CompressorFactory::GetInstance()->Create(Json::Value(), ctx, "flusher_sls", "1", CompressType::LZ4);
            string output, errorMsg;
            output.resize(item->mRawSize);
            APSARA_TEST_TRUE(compressor->UnCompress(item->mData, output, errorMsg));

            sls_logs::LogGroup logGroup;
            APSARA_TEST_TRUE(logGroup.ParseFromString(output));
            APSARA_TEST_EQUAL("topic", logGroup.topic());
            APSARA_TEST_EQUAL("uuid", logGroup.machineuuid());
            APSARA_TEST_EQUAL("172.0.0.1", logGroup.source());
            APSARA_TEST_EQUAL(1, logGroup.logtags_size());
            APSARA_TEST_EQUAL("__pack_id__", logGroup.logtags(0).key());
            APSARA_TEST_EQUAL(1, logGroup.logs_size());
            APSARA_TEST_EQUAL(1234567890U, logGroup.logs(0).time());
            APSARA_TEST_EQUAL(1, logGroup.logs(0).contents_size());
            APSARA_TEST_EQUAL("content_key", logGroup.logs(0).contents(0).key());
            APSARA_TEST_EQUAL("content_value", logGroup.logs(0).contents(0).value());

            ExactlyOnceQueueManager::GetInstance()->RemoveSenderQueueItem(eooKey, item);
        }
    }
    {
        // normal flusher, without group batch
        Json::Value configJson, optionalGoPipeline;
        string configStr, errorMsg;
        configStr = R"(
            {
                "Type": "flusher_sls",
                "Project": "test_project",
                "Logstore": "test_logstore",
                "Region": "test_region",
                "Endpoint": "test_region.log.aliyuncs.com",
                "Aliuid": "123456789",
                "ShardHashKeys": [
                    "tag_key"
                ]
            }
        )";
        ParseJsonTable(configStr, configJson, errorMsg);
        FlusherSLS flusher;
        flusher.SetContext(ctx);
        flusher.CreateMetricsRecordRef(FlusherSLS::sName, "1");
        flusher.Init(configJson, optionalGoPipeline);
        flusher.CommitMetricsRecordRef();
        {
            // empty group
            PipelineEventGroup group(make_shared<SourceBuffer>());
            APSARA_TEST_TRUE(flusher.Send(std::move(group)));
            vector<SenderQueueItem*> res;
            SenderQueueManager::GetInstance()->GetAvailableItems(res, 80);
            APSARA_TEST_TRUE(res.empty());
        }
        {
            // group
            flusher.mBatcher.GetEventFlushStrategy().SetMinCnt(1);
            PipelineEventGroup group(make_shared<SourceBuffer>());
            group.SetMetadata(EventGroupMetaKey::SOURCE_ID, string("source-id"));
            group.SetTag(LOG_RESERVED_KEY_SOURCE, "172.0.0.1");
            group.SetTag(LOG_RESERVED_KEY_MACHINE_UUID, "uuid");
            group.SetTag(LOG_RESERVED_KEY_TOPIC, "topic");
            group.SetTag(string("tag_key"), string("tag_value"));
            auto e = group.AddLogEvent();
            e->SetTimestamp(1234567890);
            e->SetContent(string("content_key"), string("content_value"));

            APSARA_TEST_TRUE(flusher.Send(std::move(group)));
            vector<SenderQueueItem*> res;
            SenderQueueManager::GetInstance()->GetAvailableItems(res, 80);
            APSARA_TEST_EQUAL(1U, res.size());
            auto item = static_cast<SLSSenderQueueItem*>(res[0]);
            APSARA_TEST_EQUAL(RawDataType::EVENT_GROUP, item->mType);
            APSARA_TEST_TRUE(item->mBufferOrNot);
            APSARA_TEST_EQUAL(&flusher, item->mFlusher);
            APSARA_TEST_EQUAL(flusher.mQueueKey, item->mQueueKey);
            APSARA_TEST_EQUAL(CalcMD5("tag_value"), item->mShardHashKey);
            APSARA_TEST_EQUAL(flusher.mLogstore, item->mLogstore);

            auto compressor
                = CompressorFactory::GetInstance()->Create(Json::Value(), ctx, "flusher_sls", "1", CompressType::LZ4);
            string output, errorMsg;
            output.resize(item->mRawSize);
            APSARA_TEST_TRUE(compressor->UnCompress(item->mData, output, errorMsg));

            sls_logs::LogGroup logGroup;
            APSARA_TEST_TRUE(logGroup.ParseFromString(output));
            APSARA_TEST_EQUAL("topic", logGroup.topic());
            APSARA_TEST_EQUAL("uuid", logGroup.machineuuid());
            APSARA_TEST_EQUAL("172.0.0.1", logGroup.source());
            APSARA_TEST_EQUAL(2, logGroup.logtags_size());
            APSARA_TEST_EQUAL("__pack_id__", logGroup.logtags(0).key());
            APSARA_TEST_EQUAL("tag_key", logGroup.logtags(1).key());
            APSARA_TEST_EQUAL("tag_value", logGroup.logtags(1).value());
            APSARA_TEST_EQUAL(1, logGroup.logs_size());
            APSARA_TEST_EQUAL(1234567890U, logGroup.logs(0).time());
            APSARA_TEST_EQUAL(1, logGroup.logs(0).contents_size());
            APSARA_TEST_EQUAL("content_key", logGroup.logs(0).contents(0).key());
            APSARA_TEST_EQUAL("content_value", logGroup.logs(0).contents(0).value());

            SenderQueueManager::GetInstance()->RemoveItem(item->mQueueKey, item);
            flusher.mBatcher.GetEventFlushStrategy().SetMinCnt(4000);
        }
        {
            // oversized group
            INT32_FLAG(max_send_log_group_size) = 1;
            flusher.mBatcher.GetEventFlushStrategy().SetMinCnt(1);
            PipelineEventGroup group(make_shared<SourceBuffer>());
            auto e = group.AddLogEvent();
            e->SetTimestamp(1234567890);
            e->SetContent(string("content_key"), string("content_value"));
            APSARA_TEST_FALSE(flusher.Send(std::move(group)));
            INT32_FLAG(max_send_log_group_size) = 10 * 1024 * 1024;
            flusher.mBatcher.GetEventFlushStrategy().SetMinCnt(4000);
        }
    }
    {
        // normal flusher, with group batch
        Json::Value configJson, optionalGoPipeline;
        string configStr, errorMsg;
        configStr = R"(
            {
                "Type": "flusher_sls",
                "Project": "test_project",
                "Logstore": "test_logstore",
                "Region": "test_region",
                "Endpoint": "test_region.log.aliyuncs.com",
                "Aliuid": "123456789"
            }
        )";
        ParseJsonTable(configStr, configJson, errorMsg);
        FlusherSLS flusher;
        flusher.SetContext(ctx);
        flusher.CreateMetricsRecordRef(FlusherSLS::sName, "1");
        flusher.Init(configJson, optionalGoPipeline);
        flusher.CommitMetricsRecordRef();

        PipelineEventGroup group(make_shared<SourceBuffer>());
        group.SetMetadata(EventGroupMetaKey::SOURCE_ID, string("source-id"));
        group.SetTag(LOG_RESERVED_KEY_SOURCE, "172.0.0.1");
        group.SetTag(LOG_RESERVED_KEY_MACHINE_UUID, "uuid");
        group.SetTag(LOG_RESERVED_KEY_TOPIC, "topic");
        {
            auto e = group.AddLogEvent();
            e->SetTimestamp(1234567890);
            e->SetContent(string("content_key"), string("content_value"));
        }
        {
            auto e = group.AddLogEvent();
            e->SetTimestamp(1234567990);
            e->SetContent(string("content_key"), string("content_value"));
        }
        flusher.mBatcher.GetGroupFlushStrategy()->SetMinSizeBytes(group.DataSize());
        // flush the above two events from group item by the following event
        {
            auto e = group.AddLogEvent();
            e->SetTimestamp(1234568990);
            e->SetContent(string("content_key"), string("content_value"));
        }

        APSARA_TEST_TRUE(flusher.Send(std::move(group)));
        vector<SenderQueueItem*> res;
        SenderQueueManager::GetInstance()->GetAvailableItems(res, 80);
        APSARA_TEST_EQUAL(1U, res.size());
        auto item = static_cast<SLSSenderQueueItem*>(res[0]);
        APSARA_TEST_EQUAL(RawDataType::EVENT_GROUP_LIST, item->mType);
        APSARA_TEST_TRUE(item->mBufferOrNot);
        APSARA_TEST_EQUAL(&flusher, item->mFlusher);
        APSARA_TEST_EQUAL(flusher.mQueueKey, item->mQueueKey);
        APSARA_TEST_EQUAL("", item->mShardHashKey);
        APSARA_TEST_EQUAL(flusher.mLogstore, item->mLogstore);

        auto compressor
            = CompressorFactory::GetInstance()->Create(Json::Value(), ctx, "flusher_sls", "1", CompressType::LZ4);

        sls_logs::SlsLogPackageList packageList;
        APSARA_TEST_TRUE(packageList.ParseFromString(item->mData));
        APSARA_TEST_EQUAL(2, packageList.packages_size());
        uint32_t rawSize = 0;
        for (size_t i = 0; i < 2; ++i) {
            APSARA_TEST_EQUAL(sls_logs::SlsCompressType::SLS_CMP_LZ4, packageList.packages(i).compress_type());

            string output, errorMsg;
            rawSize += packageList.packages(i).uncompress_size();
            output.resize(packageList.packages(i).uncompress_size());
            APSARA_TEST_TRUE(compressor->UnCompress(packageList.packages(i).data(), output, errorMsg));

            sls_logs::LogGroup logGroup;
            APSARA_TEST_TRUE(logGroup.ParseFromString(output));
            APSARA_TEST_EQUAL("topic", logGroup.topic());
            APSARA_TEST_EQUAL("uuid", logGroup.machineuuid());
            APSARA_TEST_EQUAL("172.0.0.1", logGroup.source());
            APSARA_TEST_EQUAL(1, logGroup.logtags_size());
            APSARA_TEST_EQUAL("__pack_id__", logGroup.logtags(0).key());
            APSARA_TEST_EQUAL(1, logGroup.logs_size());
            if (i == 0) {
                APSARA_TEST_EQUAL(1234567890U, logGroup.logs(0).time());
            } else {
                APSARA_TEST_EQUAL(1234567990U, logGroup.logs(0).time());
            }
            APSARA_TEST_EQUAL(1, logGroup.logs(0).contents_size());
            APSARA_TEST_EQUAL("content_key", logGroup.logs(0).contents(0).key());
            APSARA_TEST_EQUAL("content_value", logGroup.logs(0).contents(0).value());
        }
        APSARA_TEST_EQUAL(rawSize, item->mRawSize);

        SenderQueueManager::GetInstance()->RemoveItem(item->mQueueKey, item);
        flusher.FlushAll();
        res.clear();
        SenderQueueManager::GetInstance()->GetAvailableItems(res, 80);
        for (auto& tmp : res) {
            SenderQueueManager::GetInstance()->RemoveItem(tmp->mQueueKey, tmp);
        }
        flusher.mBatcher.GetGroupFlushStrategy()->SetMinSizeBytes(256 * 1024);
    }
}

void FlusherSLSUnittest::TestFlush() {
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region",
            "Endpoint": "test_region.log.aliyuncs.com",
            "Aliuid": "123456789"
        }
    )";
    ParseJsonTable(configStr, configJson, errorMsg);
    FlusherSLS flusher;
    flusher.SetContext(ctx);
    flusher.CreateMetricsRecordRef(FlusherSLS::sName, "1");
    flusher.Init(configJson, optionalGoPipeline);
    flusher.CommitMetricsRecordRef();

    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetMetadata(EventGroupMetaKey::SOURCE_ID, string("source-id"));
    group.SetTag(LOG_RESERVED_KEY_SOURCE, "172.0.0.1");
    group.SetTag(LOG_RESERVED_KEY_MACHINE_UUID, "uuid");
    group.SetTag(LOG_RESERVED_KEY_TOPIC, "topic");
    {
        auto e = group.AddLogEvent();
        e->SetTimestamp(1234567890);
        e->SetContent(string("content_key"), string("content_value"));
    }

    size_t batchKey = group.GetTagsHash();
    flusher.Send(std::move(group));

    flusher.Flush(batchKey);
    vector<SenderQueueItem*> res;
    SenderQueueManager::GetInstance()->GetAvailableItems(res, 80);
    APSARA_TEST_EQUAL(0U, res.size());

    flusher.Flush(0);
    SenderQueueManager::GetInstance()->GetAvailableItems(res, 80);
    APSARA_TEST_EQUAL(1U, res.size());
}

void FlusherSLSUnittest::TestFlushAll() {
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "test_region",
            "Endpoint": "test_region.log.aliyuncs.com",
            "Aliuid": "123456789"
        }
    )";
    ParseJsonTable(configStr, configJson, errorMsg);
    FlusherSLS flusher;
    flusher.SetContext(ctx);
    flusher.CreateMetricsRecordRef(FlusherSLS::sName, "1");
    flusher.Init(configJson, optionalGoPipeline);
    flusher.CommitMetricsRecordRef();

    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetMetadata(EventGroupMetaKey::SOURCE_ID, string("source-id"));
    group.SetTag(LOG_RESERVED_KEY_SOURCE, "172.0.0.1");
    group.SetTag(LOG_RESERVED_KEY_MACHINE_UUID, "uuid");
    group.SetTag(LOG_RESERVED_KEY_TOPIC, "topic");
    {
        auto e = group.AddLogEvent();
        e->SetTimestamp(1234567890);
        e->SetContent(string("content_key"), string("content_value"));
    }

    flusher.Send(std::move(group));

    flusher.FlushAll();
    vector<SenderQueueItem*> res;
    SenderQueueManager::GetInstance()->GetAvailableItems(res, 80);
    APSARA_TEST_EQUAL(1U, res.size());
}

void FlusherSLSUnittest::TestAddPackId() {
    FlusherSLS flusher;
    flusher.mProject = "test_project";
    flusher.mLogstore = "test_logstore";

    BatchedEvents batch;
    batch.mPackIdPrefix = "source-id";
    batch.mSourceBuffers.emplace_back(make_shared<SourceBuffer>());
    flusher.AddPackId(batch);
    APSARA_TEST_STREQ("34451096883514E2-0", batch.mTags.mInner["__pack_id__"].data());
}

void FlusherSLSUnittest::OnGoPipelineSend() {
    {
        Json::Value configJson, optionalGoPipeline;
        string configStr, errorMsg;
        configStr = R"(
            {
                "Type": "flusher_sls",
                "Project": "test_project",
                "Logstore": "test_logstore",
                "Region": "test_region",
                "Endpoint": "test_region.log.aliyuncs.com",
                "Aliuid": "123456789"
            }
        )";
        ParseJsonTable(configStr, configJson, errorMsg);
        FlusherSLS flusher;
        flusher.SetContext(ctx);
        flusher.CreateMetricsRecordRef(FlusherSLS::sName, "1");
        flusher.Init(configJson, optionalGoPipeline);
        flusher.CommitMetricsRecordRef();
        {
            APSARA_TEST_TRUE(flusher.Send("content", "shardhash_key", "other_logstore"));

            vector<SenderQueueItem*> res;
            SenderQueueManager::GetInstance()->GetAvailableItems(res, 80);

            APSARA_TEST_EQUAL(1U, res.size());
            auto item = static_cast<SLSSenderQueueItem*>(res[0]);
            APSARA_TEST_EQUAL(RawDataType::EVENT_GROUP, item->mType);
            APSARA_TEST_TRUE(item->mBufferOrNot);
            APSARA_TEST_EQUAL(&flusher, item->mFlusher);
            APSARA_TEST_EQUAL(flusher.mQueueKey, item->mQueueKey);
            APSARA_TEST_EQUAL("shardhash_key", item->mShardHashKey);
            APSARA_TEST_EQUAL("other_logstore", item->mLogstore);

            auto compressor
                = CompressorFactory::GetInstance()->Create(Json::Value(), ctx, "flusher_sls", "1", CompressType::LZ4);
            string output;
            output.resize(item->mRawSize);
            APSARA_TEST_TRUE(compressor->UnCompress(item->mData, output, errorMsg));
            APSARA_TEST_EQUAL("content", output);
        }
        {
            APSARA_TEST_TRUE(flusher.Send("content", "shardhash_key", ""));

            vector<SenderQueueItem*> res;
            SenderQueueManager::GetInstance()->GetAvailableItems(res, 80);

            APSARA_TEST_EQUAL(1U, res.size());
            auto item = static_cast<SLSSenderQueueItem*>(res[0]);
            APSARA_TEST_EQUAL("test_logstore", item->mLogstore);
        }
    }
    {
        // go profile flusher has no context
        FlusherSLS flusher;
        flusher.mProject = "test_project";
        flusher.mLogstore = "test_logstore";
        flusher.mCompressor = CompressorFactory::GetInstance()->Create(
            Json::Value(), CollectionPipelineContext(), "flusher_sls", "1", CompressType::LZ4);

        APSARA_TEST_TRUE(flusher.Send("content", ""));

        auto key = QueueKeyManager::GetInstance()->GetKey("test_project-test_logstore");

        APSARA_TEST_NOT_EQUAL(nullptr, SenderQueueManager::GetInstance()->GetQueue(key));

        vector<SenderQueueItem*> res;
        SenderQueueManager::GetInstance()->GetAvailableItems(res, 80);

        APSARA_TEST_EQUAL(1U, res.size());
        auto item = static_cast<SLSSenderQueueItem*>(res[0]);
        APSARA_TEST_EQUAL(RawDataType::EVENT_GROUP, item->mType);
        APSARA_TEST_TRUE(item->mBufferOrNot);
        APSARA_TEST_EQUAL(&flusher, item->mFlusher);
        APSARA_TEST_EQUAL(key, item->mQueueKey);
        APSARA_TEST_EQUAL("test_logstore", item->mLogstore);

        auto compressor
            = CompressorFactory::GetInstance()->Create(Json::Value(), ctx, "flusher_sls", "1", CompressType::LZ4);
        string output;
        output.resize(item->mRawSize);
        string errorMsg;
        APSARA_TEST_TRUE(compressor->UnCompress(item->mData, output, errorMsg));
        APSARA_TEST_EQUAL("content", output);
    }
}

UNIT_TEST_CASE(FlusherSLSUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(FlusherSLSUnittest, OnFailedInit)
UNIT_TEST_CASE(FlusherSLSUnittest, OnPipelineUpdate)
UNIT_TEST_CASE(FlusherSLSUnittest, TestBuildRequest)
UNIT_TEST_CASE(FlusherSLSUnittest, TestSend)
UNIT_TEST_CASE(FlusherSLSUnittest, TestFlush)
UNIT_TEST_CASE(FlusherSLSUnittest, TestFlushAll)
UNIT_TEST_CASE(FlusherSLSUnittest, TestAddPackId)
UNIT_TEST_CASE(FlusherSLSUnittest, OnGoPipelineSend)

} // namespace logtail

UNIT_TEST_MAIN
