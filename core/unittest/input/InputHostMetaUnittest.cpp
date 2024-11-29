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

#include "PluginRegistry.h"
#include "common/JsonUtil.h"
#include "ebpf/config.h"
#include "pipeline/Pipeline.h"
#include "plugin/input/InputHostMeta.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class InputHostMetaUnittest : public testing::Test {
public:
    void TestName();
    void TestSupportAck();
    void OnSuccessfulInit();
    void OnFailedInit();
    void OnSuccessfulStart();
    void OnSuccessfulStop();
    // void OnPipelineUpdate();

protected:
    void SetUp() override {
        p.mName = "test_config";
        ctx.SetConfigName("test_config");
        ctx.SetPipeline(p);
        PluginRegistry::GetInstance()->LoadPlugins();
    }

private:
    Pipeline p;
    PipelineContext ctx;
};

void InputHostMetaUnittest::TestName() {
    InputHostMeta input;
    std::string name = input.Name();
    APSARA_TEST_EQUAL(name, "input_host_meta");
}

void InputHostMetaUnittest::TestSupportAck() {
    InputHostMeta input;
    bool supportAck = input.SupportAck();
    APSARA_TEST_FALSE(supportAck);
}

void InputHostMetaUnittest::OnSuccessfulInit() {
    unique_ptr<InputHostMeta> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // valid optional param
    configStr = R"(
        {
            "Type": "input_host_meta"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputHostMeta());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_host_meta");
}

void InputHostMetaUnittest::OnFailedInit() {
}

void InputHostMetaUnittest::OnSuccessfulStart() {
    unique_ptr<InputHostMeta> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_host_meta"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputHostMeta());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(input->Start());
}

void InputHostMetaUnittest::OnSuccessfulStop() {
    unique_ptr<InputHostMeta> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_host_meta"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputHostMeta());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(input->Start());
    APSARA_TEST_TRUE(input->Stop(false));
}

UNIT_TEST_CASE(InputHostMetaUnittest, TestName)
UNIT_TEST_CASE(InputHostMetaUnittest, TestSupportAck)
UNIT_TEST_CASE(InputHostMetaUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputHostMetaUnittest, OnFailedInit)
UNIT_TEST_CASE(InputHostMetaUnittest, OnSuccessfulStart)
UNIT_TEST_CASE(InputHostMetaUnittest, OnSuccessfulStop)

} // namespace logtail

UNIT_TEST_MAIN
