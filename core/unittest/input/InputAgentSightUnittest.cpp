// Copyright 2026 iLogtail Authors
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

#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/JsonUtil.h"
#include "common/http/AsynCurlRunner.h"
#include "common/timer/Timer.h"
#include "ebpf/Config.h"
#include "ebpf/EBPFServer.h"
#include "plugin/input/InputAgentSight.h"
#include "unittest/Unittest.h"

namespace logtail {

class InputAgentSightUnittest : public testing::Test {
public:
    void TestNameAndQueueType();
    void TestInitWithProbeConfig();
    void TestInitWithHttpsAndHttp();
    void TestMetricKeysCoverManagerCounters();

protected:
    void SetUp() override {
        mPipeline.mName = "test_as";
        mContex.SetConfigName("test_as");
        mContex.SetPipeline(mPipeline);
        ebpf::EBPFServer::GetInstance()->Init();
    }

    void TearDown() override {
        ebpf::EBPFServer::GetInstance()->Stop();
        Timer::GetInstance()->Stop();
        AsynCurlRunner::GetInstance()->Stop();
    }

private:
    CollectionPipeline mPipeline;
    CollectionPipelineContext mContex;
};

void InputAgentSightUnittest::TestNameAndQueueType() {
    InputAgentSight input;
    APSARA_TEST_EQUAL(input.Name(), "input_agentsight");
    APSARA_TEST_EQUAL(input.GetProcessQueueType(), QueueType::COUNT_BOUNDED);
}

void InputAgentSightUnittest::TestInitWithProbeConfig() {
    std::string err;
    Json::Value configJson;
    Json::Value optionalGoPipeline;
    APSARA_TEST_TRUE(ParseJsonTable(
        R"({"Type":"input_agentsight","ProbeConfig":{"Verbose":0,"LogPath":"","CmdlineWhitelist":[{"AgentType":"openclaw","Args":["node*","*openclaw*"]}]}})",
        configJson,
        err));
    InputAgentSight input;
    input.SetContext(mContex);
    input.CreateMetricsRecordRef("t", "1");
    APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
    input.CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(input.mSecurityOptions.mProbeType, ebpf::SecurityProbeType::AGENTSIGHT_OBSERVE);
}

void InputAgentSightUnittest::TestInitWithHttpsAndHttp() {
    std::string err;
    Json::Value configJson;
    Json::Value optionalGoPipeline;
    APSARA_TEST_TRUE(ParseJsonTable(
        R"({"Type":"input_agentsight","ProbeConfig":{"Verbose":0,"LogPath":"","CmdlineWhitelist":[{"AgentType":"openclaw","Args":["node*","*openclaw*"]}],"Https":["*.openai.com"],"Http":[":8080","model-svc.default.svc"]}})",
        configJson,
        err));
    InputAgentSight input;
    input.SetContext(mContex);
    input.CreateMetricsRecordRef("t", "1");
    APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
    input.CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(1UL, input.mSecurityOptions.mAgentsightHttps.size());
    APSARA_TEST_EQUAL("*.openai.com", input.mSecurityOptions.mAgentsightHttps[0]);
    APSARA_TEST_EQUAL(2UL, input.mSecurityOptions.mAgentsightHttp.size());
    APSARA_TEST_EQUAL(":8080", input.mSecurityOptions.mAgentsightHttp[0]);
    APSARA_TEST_EQUAL("model-svc.default.svc", input.mSecurityOptions.mAgentsightHttp[1]);
}

void InputAgentSightUnittest::TestMetricKeysCoverManagerCounters() {
    // Guards a failure mode that is silent end to end: AgentsightManager fetches counters by key from
    // the ref this input's kMetricKeys builds, ReentrantMetricsRecord only creates counters for keys in
    // that map, GetCounter returns nullptr for the rest, and ADD_COUNTER null-checks. A key missing
    // from kMetricKeys therefore turns every increment of it into a no-op — no build error, no runtime
    // error, just a metric stuck at zero. That is exactly how the loss counters shipped dead.
    //
    // The assertions deliberately go through the input's own kMetricKeys rather than a copy of the key
    // list: a local copy would keep passing while production stayed broken.
    std::string err;
    Json::Value configJson;
    Json::Value optionalGoPipeline;
    APSARA_TEST_TRUE(ParseJsonTable(R"({"Type":"input_agentsight","ProbeConfig":{"Verbose":0}})", configJson, err));
    InputAgentSight input;
    input.SetContext(mContex);
    input.CreateMetricsRecordRef("t", "1");
    APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
    input.CommitMetricsRecordRef();
    APSARA_TEST_TRUE(input.mPluginMetricPtr != nullptr);

    // Same label sets AgentsightManager::AddOrUpdateConfig builds, one per stream.
    for (const auto& recordType : {METRIC_LABEL_VALUE_RECORD_TYPE_RAW_HTTP, METRIC_LABEL_VALUE_RECORD_TYPE_GEN_AI}) {
        MetricLabels labels = {{METRIC_LABEL_KEY_EVENT_TYPE, METRIC_LABEL_VALUE_EVENT_TYPE_LOG},
                               {METRIC_LABEL_KEY_RECORD_TYPE, recordType}};
        auto ref = input.mPluginMetricPtr->GetOrCreateReentrantMetricsRecordRef(labels);
        APSARA_TEST_TRUE(ref != nullptr);
        // Every key AgentsightManager reads must resolve to a real counter.
        APSARA_TEST_TRUE(ref->GetCounter(METRIC_PLUGIN_IN_EVENTS_TOTAL) != nullptr);
        APSARA_TEST_TRUE(ref->GetCounter(METRIC_PLUGIN_OUT_EVENTS_TOTAL) != nullptr);
        APSARA_TEST_TRUE(ref->GetCounter(METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL) != nullptr);
        APSARA_TEST_TRUE(ref->GetCounter(METRIC_PLUGIN_EBPF_LOSS_KERNEL_EVENTS_TOTAL) != nullptr);
        input.mPluginMetricPtr->ReleaseReentrantMetricsRecordRef(labels);
    }
}

UNIT_TEST_CASE(InputAgentSightUnittest, TestNameAndQueueType)
UNIT_TEST_CASE(InputAgentSightUnittest, TestInitWithProbeConfig)
UNIT_TEST_CASE(InputAgentSightUnittest, TestInitWithHttpsAndHttp)
UNIT_TEST_CASE(InputAgentSightUnittest, TestMetricKeysCoverManagerCounters)

} // namespace logtail

UNIT_TEST_MAIN
