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
    APSARA_TEST_TRUE(
        ParseJsonTable(
            R"({"Type":"input_agentsight","ProbeConfig":{"Verbose":0,"LogPath":"","CmdlineWhitelist":[{"AgentType":"openclaw","Rule":["node*","*openclaw*"]}]}})",
            configJson,
            err));
    InputAgentSight input;
    input.SetContext(mContex);
    input.CreateMetricsRecordRef("t", "1");
    APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
    input.CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(input.mSecurityOptions.mProbeType, ebpf::SecurityProbeType::AGENTSIGHT_OBSERVE);
}

UNIT_TEST_CASE(InputAgentSightUnittest, TestNameAndQueueType)
UNIT_TEST_CASE(InputAgentSightUnittest, TestInitWithProbeConfig)

} // namespace logtail

UNIT_TEST_MAIN
