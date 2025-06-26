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

#include "json/json.h"

#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/JsonUtil.h"
#include "common/http/AsynCurlRunner.h"
#include "common/timer/Timer.h"
#include "ebpf/Config.h"
#include "ebpf/EBPFServer.h"
#include "plugin/input/InputProcessSecurity.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class InputProcessSecurityUnittest : public testing::Test {
public:
    void TestName();
    void TestSupportAck();
    void OnSuccessfulInit();
    void OnSuccessfulStart();
    void OnSuccessfulStop();
    // void OnPipelineUpdate();

protected:
    void SetUp() override {
        p.mName = "test_config";
        ctx.SetConfigName("test_config");
        ctx.SetPipeline(p);
        ebpf::EBPFServer::GetInstance()->Init();
    }

    void TearDown() override {
        ebpf::EBPFServer::GetInstance()->Stop();
        Timer::GetInstance()->Stop();
        AsynCurlRunner::GetInstance()->Stop();
    }

private:
    CollectionPipeline p;
    CollectionPipelineContext ctx;
};

void InputProcessSecurityUnittest::TestName() {
    InputProcessSecurity input;
    std::string name = input.Name();
    APSARA_TEST_EQUAL(name, "input_process_security");
}

void InputProcessSecurityUnittest::TestSupportAck() {
    InputProcessSecurity input;
    bool supportAck = input.SupportAck();
    APSARA_TEST_TRUE(supportAck);
}

void InputProcessSecurityUnittest::OnSuccessfulInit() {
    unique_ptr<InputProcessSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // valid param
    configStr = R"(
        {
            "Type": "input_process_security",
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputProcessSecurity());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_process_security");
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList[0].mCallNames.size(), 5UL);
    // no general filter, default is monostate
    APSARA_TEST_EQUAL(std::holds_alternative<std::monostate>(input->mSecurityOptions.mOptionList[0].mFilter), true);
}

void InputProcessSecurityUnittest::OnSuccessfulStart() {
    unique_ptr<InputProcessSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_process_security",
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputProcessSecurity());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(input->Start());
    string serverPipelineName
        = ebpf::EBPFServer::GetInstance()->checkLoadedPipelineName(logtail::ebpf::PluginType::PROCESS_SECURITY);
    string pipelineName = input->GetContext().GetConfigName();
    APSARA_TEST_TRUE(serverPipelineName.size() && serverPipelineName == pipelineName);
    input->Stop(true);
}

void InputProcessSecurityUnittest::OnSuccessfulStop() {
    unique_ptr<InputProcessSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_process_security",
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputProcessSecurity());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(input->Start());
    string serverPipelineName
        = ebpf::EBPFServer::GetInstance()->checkLoadedPipelineName(logtail::ebpf::PluginType::PROCESS_SECURITY);
    string pipelineName = input->GetContext().GetConfigName();
    APSARA_TEST_TRUE(serverPipelineName.size() && serverPipelineName == pipelineName);
    // APSARA_TEST_TRUE(input->Stop(false));
    serverPipelineName
        = ebpf::EBPFServer::GetInstance()->checkLoadedPipelineName(logtail::ebpf::PluginType::PROCESS_SECURITY);
    APSARA_TEST_TRUE(serverPipelineName.size() && serverPipelineName == pipelineName);
    APSARA_TEST_TRUE(input->Stop(true));
    serverPipelineName
        = ebpf::EBPFServer::GetInstance()->checkLoadedPipelineName(logtail::ebpf::PluginType::PROCESS_SECURITY);
    APSARA_TEST_TRUE(serverPipelineName.empty());
}

UNIT_TEST_CASE(InputProcessSecurityUnittest, TestName)
UNIT_TEST_CASE(InputProcessSecurityUnittest, TestSupportAck)
UNIT_TEST_CASE(InputProcessSecurityUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputProcessSecurityUnittest, OnSuccessfulStart)
UNIT_TEST_CASE(InputProcessSecurityUnittest, OnSuccessfulStop)
// UNIT_TEST_CASE(InputProcessSecurityUnittest, OnPipelineUpdate)

} // namespace logtail

UNIT_TEST_MAIN
