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
#include "common/LogtailCommonFlags.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif
#include "pipeline/compression/CompressorFactory.h"
#include "plugin/flusher/sls/FlusherSLS.h"
#include "plugin/flusher/sls/PackIdManager.h"
#include "plugin/flusher/sls/SLSClientManager.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineContext.h"
#include "pipeline/queue/ExactlyOnceQueueManager.h"
#include "pipeline/queue/ProcessQueueManager.h"
#include "pipeline/queue/QueueKeyManager.h"
#include "pipeline/queue/SLSSenderQueueItem.h"
#include "pipeline/queue/SenderQueueManager.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_INT32(batch_send_interval);
DECLARE_FLAG_INT32(merge_log_count_limit);
DECLARE_FLAG_INT32(batch_send_metric_size);
DECLARE_FLAG_INT32(max_send_log_group_size);

using namespace std;

namespace logtail {

class FlusherSLSUnittest : public testing::Test {
public:
    void OnSuccessfulInit();
    void OnFailedInit();
    void OnPipelineUpdate();
    void TestSend();
    void TestFlush();
    void TestFlushAll();
    void TestAddPackId();
    void OnGoPipelineSend();

protected:
    void SetUp() override {
        ctx.SetConfigName("test_config");
        ctx.SetPipeline(pipeline);
    }

    void TearDown() override {
        PackIdManager::GetInstance()->mPackIdSeq.clear();
        QueueKeyManager::GetInstance()->Clear();
        SenderQueueManager::GetInstance()->Clear();
        ExactlyOnceQueueManager::GetInstance()->Clear();
    }

private:
    Pipeline pipeline;
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
            "Logstore": "test_logstore"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
#ifndef __ENTERPRISE__
    configJson["Endpoint"] = "cn-hangzhou.log.aliyuncs.com";
#endif
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1", "1", "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(optionalGoPipeline.isNull());
    APSARA_TEST_EQUAL("test_project", flusher->mProject);
    APSARA_TEST_EQUAL("test_logstore", flusher->mLogstore);
    APSARA_TEST_EQUAL(STRING_FLAG(default_region_name), flusher->mRegion);
#ifndef __ENTERPRISE__
    APSARA_TEST_EQUAL("cn-hangzhou.log.aliyuncs.com", flusher->mEndpoint);
#else
    APSARA_TEST_EQUAL("", flusher->mEndpoint);
#endif
    APSARA_TEST_EQUAL("", flusher->mAliuid);
    APSARA_TEST_EQUAL(FlusherSLS::TelemetryType::LOG, flusher->mTelemetryType);
    APSARA_TEST_TRUE(flusher->mShardHashKeys.empty());
    APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(merge_log_count_limit)),
                      flusher->mBatcher.GetEventFlushStrategy().GetMaxCnt());
    APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(batch_send_metric_size)),
                      flusher->mBatcher.GetEventFlushStrategy().GetMaxSizeBytes());
    uint32_t timeout = static_cast<uint32_t>(INT32_FLAG(batch_send_interval)) / 2;
    APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(batch_send_interval)) - timeout,
                      flusher->mBatcher.GetEventFlushStrategy().GetTimeoutSecs());
    APSARA_TEST_TRUE(flusher->mBatcher.GetGroupFlushStrategy().has_value());
    APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(batch_send_metric_size)),
                      flusher->mBatcher.GetGroupFlushStrategy()->GetMaxSizeBytes());
    APSARA_TEST_EQUAL(timeout, flusher->mBatcher.GetGroupFlushStrategy()->GetTimeoutSecs());
    APSARA_TEST_TRUE(flusher->mGroupSerializer);
    APSARA_TEST_TRUE(flusher->mGroupListSerializer);
    APSARA_TEST_EQUAL(CompressType::LZ4, flusher->mCompressor->GetCompressType());
    APSARA_TEST_EQUAL(QueueKeyManager::GetInstance()->GetKey("test_config-flusher_sls-test_project#test_logstore"),
                      flusher->GetQueueKey());
    auto que = SenderQueueManager::GetInstance()->GetQueue(flusher->GetQueueKey());
    APSARA_TEST_NOT_EQUAL(nullptr, que);
    APSARA_TEST_FALSE(que->GetRateLimiter().has_value());
    APSARA_TEST_EQUAL(2U, que->GetConcurrencyLimiters().size());

    // valid optional param
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com",
            "Aliuid": "123456789",
            "TelemetryType": "metrics",
            "ShardHashKeys": [
                "__source__"
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1", "1", "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL("cn-hangzhou", flusher->mRegion);
#ifdef __ENTERPRISE__
    APSARA_TEST_EQUAL("123456789", flusher->mAliuid);
#else
    APSARA_TEST_EQUAL("cn-hangzhou.log.aliyuncs.com", flusher->mEndpoint);
    APSARA_TEST_EQUAL("", flusher->mAliuid);
#endif
    APSARA_TEST_EQUAL(FlusherSLS::TelemetryType::METRIC, flusher->mTelemetryType);
    APSARA_TEST_EQUAL(1U, flusher->mShardHashKeys.size());
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
    configJson["Endpoint"] = true;
#else
    configJson["Endpoint"] = "cn-hangzhou.log.aliyuncs.com";
#endif
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1", "1", "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(STRING_FLAG(default_region_name), flusher->mRegion);
#ifdef __ENTERPRISE__
    APSARA_TEST_EQUAL("", flusher->mEndpoint);
#endif
    APSARA_TEST_EQUAL("", flusher->mAliuid);
    APSARA_TEST_EQUAL(FlusherSLS::TelemetryType::LOG, flusher->mTelemetryType);
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
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1", "1", "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(STRING_FLAG(default_region_name), flusher->mRegion);
    EnterpriseConfigProvider::GetInstance()->mIsPrivateCloud = false;
    SenderQueueManager::GetInstance()->Clear();
#endif

    // Endpoint
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
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1", "1", "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL("cn-hangzhou.log.aliyuncs.com", flusher->mEndpoint);
    auto iter = SLSClientManager::GetInstance()->mRegionEndpointEntryMap.find("cn-hangzhou");
    APSARA_TEST_NOT_EQUAL(SLSClientManager::GetInstance()->mRegionEndpointEntryMap.end(), iter);
    APSARA_TEST_NOT_EQUAL(iter->second.mEndpointInfoMap.end(),
                          iter->second.mEndpointInfoMap.find("http://cn-hangzhou.log.aliyuncs.com"));
    SenderQueueManager::GetInstance()->Clear();

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
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1", "1", "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(FlusherSLS::TelemetryType::LOG, flusher->mTelemetryType);
    SenderQueueManager::GetInstance()->Clear();

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
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1", "1", "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(FlusherSLS::TelemetryType::LOG, flusher->mTelemetryType);
    SenderQueueManager::GetInstance()->Clear();

    // ShardHashKeys
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com",
            "ShardHashKeys": [
                "__source__"
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    ctx.SetExactlyOnceFlag(true);
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1", "1", "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(flusher->mShardHashKeys.empty());
    ctx.SetExactlyOnceFlag(false);
    SenderQueueManager::GetInstance()->Clear();

    // group batch && sender queue
    configStr = R"(
        {
            "Type": "flusher_sls",
            "Project": "test_project",
            "Logstore": "test_logstore",
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com",
            "ShardHashKeys": [
                "__source__"
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1", "1", "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_FALSE(flusher->mBatcher.GetGroupFlushStrategy().has_value());
    SenderQueueManager::GetInstance()->Clear();

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
    ctx.SetExactlyOnceFlag(true);
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1", "1", "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_FALSE(flusher->mBatcher.GetGroupFlushStrategy().has_value());
    APSARA_TEST_EQUAL(nullptr, SenderQueueManager::GetInstance()->GetQueue(flusher->GetQueueKey()));
    ctx.SetExactlyOnceFlag(false);
    SenderQueueManager::GetInstance()->Clear();

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
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1", "1", "1");
    APSARA_TEST_TRUE(flusher->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(optionalGoPipelineJson.toStyledString(), optionalGoPipeline.toStyledString());
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
            "Endpoint": "cn-hangzhou.log.aliyuncs.com"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1", "1", "1");
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
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1", "1", "1");
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
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1", "1", "1");
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
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1", "1", "1");
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
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1", "1", "1");
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
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1", "1", "1");
    APSARA_TEST_FALSE(flusher->Init(configJson, optionalGoPipeline));
#endif
}

void FlusherSLSUnittest::OnPipelineUpdate() {
    PipelineContext ctx1;
    ctx1.SetConfigName("test_config_1");

    Json::Value configJson, optionalGoPipeline;
    FlusherSLS flusher1;
    flusher1.SetContext(ctx1);
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
    APSARA_TEST_TRUE(flusher1.Start());
    APSARA_TEST_EQUAL(1U, FlusherSLS::sProjectRefCntMap.size());
    APSARA_TEST_TRUE(FlusherSLS::IsRegionContainingConfig("cn-hangzhou"));
    APSARA_TEST_EQUAL(1U, SLSClientManager::GetInstance()->GetRegionAliuids("cn-hangzhou").size());

    {
        PipelineContext ctx2;
        ctx2.SetConfigName("test_config_2");
        FlusherSLS flusher2;
        flusher2.SetContext(ctx2);
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

        APSARA_TEST_TRUE(flusher1.Stop(false));
        APSARA_TEST_TRUE(FlusherSLS::sProjectRefCntMap.empty());
        APSARA_TEST_FALSE(FlusherSLS::IsRegionContainingConfig("cn-hangzhou"));
        APSARA_TEST_TRUE(SLSClientManager::GetInstance()->GetRegionAliuids("cn-hangzhou").empty());
        APSARA_TEST_TRUE(SenderQueueManager::GetInstance()->IsQueueMarkedDeleted(flusher1.GetQueueKey()));

        APSARA_TEST_TRUE(flusher2.Start());
        APSARA_TEST_EQUAL(1U, FlusherSLS::sProjectRefCntMap.size());
        APSARA_TEST_TRUE(FlusherSLS::IsRegionContainingConfig("cn-hangzhou"));
        APSARA_TEST_EQUAL(1U, SLSClientManager::GetInstance()->GetRegionAliuids("cn-hangzhou").size());
        APSARA_TEST_TRUE(SenderQueueManager::GetInstance()->IsQueueMarkedDeleted(flusher1.GetQueueKey()));
        APSARA_TEST_FALSE(SenderQueueManager::GetInstance()->IsQueueMarkedDeleted(flusher2.GetQueueKey()));
        flusher2.Stop(true);
        flusher1.Start();
    }
    {
        PipelineContext ctx2;
        ctx2.SetConfigName("test_config_1");
        FlusherSLS flusher2;
        flusher2.SetContext(ctx2);
        configStr = R"(
            {
                "Type": "flusher_sls",
                "Project": "test_project",
                "Logstore": "test_logstore",
                "Region": "cn-hangzhou",
                "Endpoint": "cn-hangzhou.log.aliyuncs.com",
                "Aliuid": "123456789"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        APSARA_TEST_TRUE(flusher2.Init(configJson, optionalGoPipeline));

        APSARA_TEST_TRUE(flusher1.Stop(false));
        APSARA_TEST_TRUE(SenderQueueManager::GetInstance()->IsQueueMarkedDeleted(flusher1.GetQueueKey()));

        APSARA_TEST_TRUE(flusher2.Start());
        APSARA_TEST_FALSE(SenderQueueManager::GetInstance()->IsQueueMarkedDeleted(flusher1.GetQueueKey()));
        APSARA_TEST_FALSE(SenderQueueManager::GetInstance()->IsQueueMarkedDeleted(flusher2.GetQueueKey()));
        flusher2.Stop(true);
    }
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
                "Region": "cn-hangzhou",
                "Endpoint": "cn-hangzhou.log.aliyuncs.com",
                "Aliuid": "123456789"
            }
        )";
        ParseJsonTable(configStr, configJson, errorMsg);
        FlusherSLS flusher;
        PipelineContext ctx;
        ctx.SetConfigName("test_config");
        ctx.SetExactlyOnceFlag(true);
        flusher.SetContext(ctx);
        flusher.Init(configJson, optionalGoPipeline);

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
            group.SetTag(LOG_RESERVED_KEY_HOSTNAME, "hostname");
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
            ExactlyOnceQueueManager::GetInstance()->GetAllAvailableSenderQueueItems(res);
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
                = CompressorFactory::GetInstance()->Create(Json::Value(), ctx, "flusher_sls", CompressType::LZ4);
            string output, errorMsg;
            output.resize(item->mRawSize);
            APSARA_TEST_TRUE(compressor->UnCompress(item->mData, output, errorMsg));

            sls_logs::LogGroup logGroup;
            APSARA_TEST_TRUE(logGroup.ParseFromString(output));
            APSARA_TEST_EQUAL("topic", logGroup.topic());
            APSARA_TEST_EQUAL("uuid", logGroup.machineuuid());
            APSARA_TEST_EQUAL("172.0.0.1", logGroup.source());
            APSARA_TEST_EQUAL(2, logGroup.logtags_size());
            APSARA_TEST_EQUAL("__hostname__", logGroup.logtags(0).key());
            APSARA_TEST_EQUAL("hostname", logGroup.logtags(0).value());
            APSARA_TEST_EQUAL("__pack_id__", logGroup.logtags(1).key());
            APSARA_TEST_EQUAL(1, logGroup.logs_size());
            APSARA_TEST_EQUAL(1234567890U, logGroup.logs(0).time());
            APSARA_TEST_EQUAL(1, logGroup.logs(0).contents_size());
            APSARA_TEST_EQUAL("content_key", logGroup.logs(0).contents(0).key());
            APSARA_TEST_EQUAL("content_value", logGroup.logs(0).contents(0).value());

            ExactlyOnceQueueManager::GetInstance()->RemoveSenderQueueItem(eooKey, item);
        }
        {
            // non-replay group
            flusher.mBatcher.GetEventFlushStrategy().SetMaxCnt(1);
            PipelineEventGroup group(make_shared<SourceBuffer>());
            group.SetMetadata(EventGroupMetaKey::SOURCE_ID, string("source-id"));
            group.SetTag(LOG_RESERVED_KEY_HOSTNAME, "hostname");
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
            ExactlyOnceQueueManager::GetInstance()->GetAllAvailableSenderQueueItems(res);
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
                = CompressorFactory::GetInstance()->Create(Json::Value(), ctx, "flusher_sls", CompressType::LZ4);
            string output, errorMsg;
            output.resize(item->mRawSize);
            APSARA_TEST_TRUE(compressor->UnCompress(item->mData, output, errorMsg));

            sls_logs::LogGroup logGroup;
            APSARA_TEST_TRUE(logGroup.ParseFromString(output));
            APSARA_TEST_EQUAL("topic", logGroup.topic());
            APSARA_TEST_EQUAL("uuid", logGroup.machineuuid());
            APSARA_TEST_EQUAL("172.0.0.1", logGroup.source());
            APSARA_TEST_EQUAL(2, logGroup.logtags_size());
            APSARA_TEST_EQUAL("__hostname__", logGroup.logtags(0).key());
            APSARA_TEST_EQUAL("hostname", logGroup.logtags(0).value());
            APSARA_TEST_EQUAL("__pack_id__", logGroup.logtags(1).key());
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
                "Region": "cn-hangzhou",
                "Endpoint": "cn-hangzhou.log.aliyuncs.com",
                "Aliuid": "123456789",
                "ShardHashKeys": [
                    "tag_key"
                ]
            }
        )";
        ParseJsonTable(configStr, configJson, errorMsg);
        FlusherSLS flusher;
        flusher.SetContext(ctx);
        flusher.Init(configJson, optionalGoPipeline);
        {
            // empty group
            PipelineEventGroup group(make_shared<SourceBuffer>());
            APSARA_TEST_TRUE(flusher.Send(std::move(group)));
            vector<SenderQueueItem*> res;
            SenderQueueManager::GetInstance()->GetAllAvailableItems(res);
            APSARA_TEST_TRUE(res.empty());
        }
        {
            // group
            flusher.mBatcher.GetEventFlushStrategy().SetMaxCnt(1);
            PipelineEventGroup group(make_shared<SourceBuffer>());
            group.SetMetadata(EventGroupMetaKey::SOURCE_ID, string("source-id"));
            group.SetTag(LOG_RESERVED_KEY_HOSTNAME, "hostname");
            group.SetTag(LOG_RESERVED_KEY_SOURCE, "172.0.0.1");
            group.SetTag(LOG_RESERVED_KEY_MACHINE_UUID, "uuid");
            group.SetTag(LOG_RESERVED_KEY_TOPIC, "topic");
            group.SetTag(string("tag_key"), string("tag_value"));
            auto e = group.AddLogEvent();
            e->SetTimestamp(1234567890);
            e->SetContent(string("content_key"), string("content_value"));

            APSARA_TEST_TRUE(flusher.Send(std::move(group)));
            vector<SenderQueueItem*> res;
            SenderQueueManager::GetInstance()->GetAllAvailableItems(res);
            APSARA_TEST_EQUAL(1U, res.size());
            auto item = static_cast<SLSSenderQueueItem*>(res[0]);
            APSARA_TEST_EQUAL(RawDataType::EVENT_GROUP, item->mType);
            APSARA_TEST_TRUE(item->mBufferOrNot);
            APSARA_TEST_EQUAL(&flusher, item->mFlusher);
            APSARA_TEST_EQUAL(flusher.mQueueKey, item->mQueueKey);
            APSARA_TEST_EQUAL(sdk::CalcMD5("tag_value"), item->mShardHashKey);
            APSARA_TEST_EQUAL(flusher.mLogstore, item->mLogstore);

            auto compressor
                = CompressorFactory::GetInstance()->Create(Json::Value(), ctx, "flusher_sls", CompressType::LZ4);
            string output, errorMsg;
            output.resize(item->mRawSize);
            APSARA_TEST_TRUE(compressor->UnCompress(item->mData, output, errorMsg));

            sls_logs::LogGroup logGroup;
            APSARA_TEST_TRUE(logGroup.ParseFromString(output));
            APSARA_TEST_EQUAL("topic", logGroup.topic());
            APSARA_TEST_EQUAL("uuid", logGroup.machineuuid());
            APSARA_TEST_EQUAL("172.0.0.1", logGroup.source());
            APSARA_TEST_EQUAL(3, logGroup.logtags_size());
            APSARA_TEST_EQUAL("__hostname__", logGroup.logtags(0).key());
            APSARA_TEST_EQUAL("hostname", logGroup.logtags(0).value());
            APSARA_TEST_EQUAL("__pack_id__", logGroup.logtags(1).key());
            APSARA_TEST_EQUAL("tag_key", logGroup.logtags(2).key());
            APSARA_TEST_EQUAL("tag_value", logGroup.logtags(2).value());
            APSARA_TEST_EQUAL(1, logGroup.logs_size());
            APSARA_TEST_EQUAL(1234567890U, logGroup.logs(0).time());
            APSARA_TEST_EQUAL(1, logGroup.logs(0).contents_size());
            APSARA_TEST_EQUAL("content_key", logGroup.logs(0).contents(0).key());
            APSARA_TEST_EQUAL("content_value", logGroup.logs(0).contents(0).value());

            SenderQueueManager::GetInstance()->RemoveItem(item->mQueueKey, item);
            flusher.mBatcher.GetEventFlushStrategy().SetMaxCnt(4000);
        }
        {
            // oversized group
            INT32_FLAG(max_send_log_group_size) = 1;
            flusher.mBatcher.GetEventFlushStrategy().SetMaxCnt(1);
            PipelineEventGroup group(make_shared<SourceBuffer>());
            auto e = group.AddLogEvent();
            e->SetTimestamp(1234567890);
            e->SetContent(string("content_key"), string("content_value"));
            APSARA_TEST_FALSE(flusher.Send(std::move(group)));
            INT32_FLAG(max_send_log_group_size) = 10 * 1024 * 1024;
            flusher.mBatcher.GetEventFlushStrategy().SetMaxCnt(4000);
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
                "Region": "cn-hangzhou",
                "Endpoint": "cn-hangzhou.log.aliyuncs.com",
                "Aliuid": "123456789"
            }
        )";
        ParseJsonTable(configStr, configJson, errorMsg);
        FlusherSLS flusher;
        flusher.SetContext(ctx);
        flusher.Init(configJson, optionalGoPipeline);

        PipelineEventGroup group(make_shared<SourceBuffer>());
        group.SetMetadata(EventGroupMetaKey::SOURCE_ID, string("source-id"));
        group.SetTag(LOG_RESERVED_KEY_HOSTNAME, "hostname");
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
        flusher.mBatcher.GetGroupFlushStrategy()->SetMaxSizeBytes(group.DataSize());
        // flush the above two events from group item by the following event
        {
            auto e = group.AddLogEvent();
            e->SetTimestamp(1234568990);
            e->SetContent(string("content_key"), string("content_value"));
        }

        APSARA_TEST_TRUE(flusher.Send(std::move(group)));
        vector<SenderQueueItem*> res;
        SenderQueueManager::GetInstance()->GetAllAvailableItems(res);
        APSARA_TEST_EQUAL(1U, res.size());
        auto item = static_cast<SLSSenderQueueItem*>(res[0]);
        APSARA_TEST_EQUAL(RawDataType::EVENT_GROUP_LIST, item->mType);
        APSARA_TEST_TRUE(item->mBufferOrNot);
        APSARA_TEST_EQUAL(&flusher, item->mFlusher);
        APSARA_TEST_EQUAL(flusher.mQueueKey, item->mQueueKey);
        APSARA_TEST_EQUAL("", item->mShardHashKey);
        APSARA_TEST_EQUAL(flusher.mLogstore, item->mLogstore);

        auto compressor
            = CompressorFactory::GetInstance()->Create(Json::Value(), ctx, "flusher_sls", CompressType::LZ4);

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
            APSARA_TEST_EQUAL(2, logGroup.logtags_size());
            APSARA_TEST_EQUAL("__hostname__", logGroup.logtags(0).key());
            APSARA_TEST_EQUAL("hostname", logGroup.logtags(0).value());
            APSARA_TEST_EQUAL("__pack_id__", logGroup.logtags(1).key());
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
        SenderQueueManager::GetInstance()->GetAllAvailableItems(res);
        for (auto& tmp : res) {
            SenderQueueManager::GetInstance()->RemoveItem(tmp->mQueueKey, tmp);
        }
        flusher.mBatcher.GetGroupFlushStrategy()->SetMaxSizeBytes(256 * 1024);
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
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com",
            "Aliuid": "123456789"
        }
    )";
    ParseJsonTable(configStr, configJson, errorMsg);
    FlusherSLS flusher;
    flusher.SetContext(ctx);
    flusher.Init(configJson, optionalGoPipeline);

    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetMetadata(EventGroupMetaKey::SOURCE_ID, string("source-id"));
    group.SetTag(LOG_RESERVED_KEY_HOSTNAME, "hostname");
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
    SenderQueueManager::GetInstance()->GetAllAvailableItems(res);
    APSARA_TEST_EQUAL(0U, res.size());

    flusher.Flush(0);
    SenderQueueManager::GetInstance()->GetAllAvailableItems(res);
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
            "Region": "cn-hangzhou",
            "Endpoint": "cn-hangzhou.log.aliyuncs.com",
            "Aliuid": "123456789"
        }
    )";
    ParseJsonTable(configStr, configJson, errorMsg);
    FlusherSLS flusher;
    flusher.SetContext(ctx);
    flusher.Init(configJson, optionalGoPipeline);

    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetMetadata(EventGroupMetaKey::SOURCE_ID, string("source-id"));
    group.SetTag(LOG_RESERVED_KEY_HOSTNAME, "hostname");
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
    SenderQueueManager::GetInstance()->GetAllAvailableItems(res);
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
                "Region": "cn-hangzhou",
                "Endpoint": "cn-hangzhou.log.aliyuncs.com",
                "Aliuid": "123456789"
            }
        )";
        ParseJsonTable(configStr, configJson, errorMsg);
        FlusherSLS flusher;
        flusher.SetContext(ctx);
        flusher.Init(configJson, optionalGoPipeline);
        {
            APSARA_TEST_TRUE(flusher.Send("content", "shardhash_key", "other_logstore"));

            vector<SenderQueueItem*> res;
            SenderQueueManager::GetInstance()->GetAllAvailableItems(res);

            APSARA_TEST_EQUAL(1U, res.size());
            auto item = static_cast<SLSSenderQueueItem*>(res[0]);
            APSARA_TEST_EQUAL(RawDataType::EVENT_GROUP, item->mType);
            APSARA_TEST_TRUE(item->mBufferOrNot);
            APSARA_TEST_EQUAL(&flusher, item->mFlusher);
            APSARA_TEST_EQUAL(flusher.mQueueKey, item->mQueueKey);
            APSARA_TEST_EQUAL("shardhash_key", item->mShardHashKey);
            APSARA_TEST_EQUAL("other_logstore", item->mLogstore);

            auto compressor
                = CompressorFactory::GetInstance()->Create(Json::Value(), ctx, "flusher_sls", CompressType::LZ4);
            string output;
            output.resize(item->mRawSize);
            APSARA_TEST_TRUE(compressor->UnCompress(item->mData, output, errorMsg));
            APSARA_TEST_EQUAL("content", output);
        }
        {
            APSARA_TEST_TRUE(flusher.Send("content", "shardhash_key", ""));

            vector<SenderQueueItem*> res;
            SenderQueueManager::GetInstance()->GetAllAvailableItems(res);

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
            Json::Value(), PipelineContext(), "flusher_sls", CompressType::LZ4);

        APSARA_TEST_TRUE(flusher.Send("content", ""));

        auto key = QueueKeyManager::GetInstance()->GetKey("test_project-test_logstore");

        APSARA_TEST_NOT_EQUAL(nullptr, SenderQueueManager::GetInstance()->GetQueue(key));

        vector<SenderQueueItem*> res;
        SenderQueueManager::GetInstance()->GetAllAvailableItems(res);

        APSARA_TEST_EQUAL(1U, res.size());
        auto item = static_cast<SLSSenderQueueItem*>(res[0]);
        APSARA_TEST_EQUAL(RawDataType::EVENT_GROUP, item->mType);
        APSARA_TEST_TRUE(item->mBufferOrNot);
        APSARA_TEST_EQUAL(&flusher, item->mFlusher);
        APSARA_TEST_EQUAL(key, item->mQueueKey);
        APSARA_TEST_EQUAL("test_logstore", item->mLogstore);

        auto compressor
            = CompressorFactory::GetInstance()->Create(Json::Value(), ctx, "flusher_sls", CompressType::LZ4);
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
UNIT_TEST_CASE(FlusherSLSUnittest, TestSend)
UNIT_TEST_CASE(FlusherSLSUnittest, TestFlush)
UNIT_TEST_CASE(FlusherSLSUnittest, TestFlushAll)
UNIT_TEST_CASE(FlusherSLSUnittest, TestAddPackId)
UNIT_TEST_CASE(FlusherSLSUnittest, OnGoPipelineSend)

} // namespace logtail

UNIT_TEST_MAIN
