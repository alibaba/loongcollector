// Copyright 2025 iLogtail Authors
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

#include "json/json.h"

#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/JsonUtil.h"
#include "common/http/AsynCurlRunner.h"
#include "ebpf/EBPFServer.h"
#include "plugin/input/InputCpuProfiling.h"
#include "unittest/Unittest.h"

namespace logtail {

class InputCpuProfilingUnittest : public testing::Test {
public:
    void TestName();
    void TestSupportAck();
    void OnSuccessfulInit();
    void OnFailedInit();
    void OnSuccessfulStart();
    void OnSuccessfulStop();

protected:
    void SetUp() override {
        p.mName = "test_config";
        ctx.SetConfigName("test_config");
        ctx.SetPipeline(p);
        ebpf::EBPFServer::GetInstance()->Init();
    }

    void TearDown() override {
        ebpf::EBPFServer::GetInstance()->Stop();
        AsynCurlRunner::GetInstance()->Stop();
    }

private:
    CollectionPipeline p;
    CollectionPipelineContext ctx;
};

void InputCpuProfilingUnittest::TestName() {
    InputCpuProfiling input;
    std::string name = input.Name();
    APSARA_TEST_EQUAL(name, "input_cpu_profiling");
}

void InputCpuProfilingUnittest::TestSupportAck() {
    InputCpuProfiling input;
    bool supportAck = input.SupportAck();
    APSARA_TEST_TRUE(supportAck);
}

void InputCpuProfilingUnittest::OnSuccessfulInit() {
    std::unique_ptr<InputCpuProfiling> input;
    Json::Value configJson, optionalGoPipeline;
    std::string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_cpu_profiling",
            "CommandLines": ["java"],
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputCpuProfiling());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(input->sName, "input_cpu_profiling");
    logtail::ebpf::CpuProfilingOption option = input->mCpuProfilingOption;
    APSARA_TEST_TRUE(option.mCmdlines.size() == 1);
    APSARA_TEST_TRUE(option.mCmdlines[0] == "java");
}

void InputCpuProfilingUnittest::OnFailedInit() {
    std::unique_ptr<InputCpuProfiling> input;
    Json::Value configJson, optionalGoPipeline;
    std::string configStr, errorMsg;

    // Invalid Pids
    configStr = R"(
        {
            "Type": "input_cpu_profiling",
            "CommandLines": [1, 2, 3],
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputCpuProfiling());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(input->sName, "input_cpu_profiling");
    logtail::ebpf::CpuProfilingOption option = input->mCpuProfilingOption;
    APSARA_TEST_TRUE(option.mCmdlines.empty());
}

void InputCpuProfilingUnittest::OnSuccessfulStart() {
    std::unique_ptr<InputCpuProfiling> input;
    Json::Value configJson, optionalGoPipeline;
    std::string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_cpu_profiling",
            "CommandLines": ["java"],
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputCpuProfiling());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_TRUE(input->Start());
    APSARA_TEST_TRUE(input->Stop(true));
}

void InputCpuProfilingUnittest::OnSuccessfulStop() {
    std::unique_ptr<InputCpuProfiling> input;
    Json::Value configJson, optionalGoPipeline;
    std::string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_cpu_profiling",
            "CommandLines": ["java"],
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputCpuProfiling());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_TRUE(input->Start());
    APSARA_TEST_TRUE(input->Stop(true));
}

UNIT_TEST_CASE(InputCpuProfilingUnittest, TestName)
UNIT_TEST_CASE(InputCpuProfilingUnittest, TestSupportAck)
UNIT_TEST_CASE(InputCpuProfilingUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputCpuProfilingUnittest, OnFailedInit)
UNIT_TEST_CASE(InputCpuProfilingUnittest, OnSuccessfulStart)
UNIT_TEST_CASE(InputCpuProfilingUnittest, OnSuccessfulStop)

} // namespace logtail

UNIT_TEST_MAIN