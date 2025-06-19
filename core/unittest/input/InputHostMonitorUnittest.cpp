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

#include <json/json.h>

#include "PluginRegistry.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "common/JsonUtil.h"
#include "plugin/input/InputHostMonitor.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class InputHostMonitorUnittest : public testing::Test {
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
    CollectionPipeline p;
    CollectionPipelineContext ctx;
};

void InputHostMonitorUnittest::TestName() {
    InputHostMonitor input;
    std::string name = input.Name();
    APSARA_TEST_EQUAL(name, "input_host_monitor");
}

void InputHostMonitorUnittest::TestSupportAck() {
    InputHostMonitor input;
    bool supportAck = input.SupportAck();
    APSARA_TEST_TRUE(supportAck);
}

void InputHostMonitorUnittest::OnSuccessfulInit() {
    unique_ptr<InputHostMonitor> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // valid optional param
    configStr = R"(
        {
            "Type": "input_host_monitor"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputHostMonitor());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(input->sName, "input_host_monitor");
}

void InputHostMonitorUnittest::OnFailedInit() {
    unique_ptr<InputHostMonitor> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // valid optional param
    configStr = R"(
        {
            "Type": "input_host_monitor",
            "CPU": 123456
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputHostMonitor());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL_FATAL(input->mInterval, 15);
}

void InputHostMonitorUnittest::OnSuccessfulStart() {
    unique_ptr<InputHostMonitor> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_host_monitor"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputHostMonitor());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_TRUE(input->Start());
}

void InputHostMonitorUnittest::OnSuccessfulStop() {
    unique_ptr<InputHostMonitor> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_host_monitor"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputHostMonitor());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_TRUE(input->Start());
    APSARA_TEST_TRUE(input->Stop(false));
}

UNIT_TEST_CASE(InputHostMonitorUnittest, TestName)
UNIT_TEST_CASE(InputHostMonitorUnittest, TestSupportAck)
UNIT_TEST_CASE(InputHostMonitorUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputHostMonitorUnittest, OnFailedInit)
UNIT_TEST_CASE(InputHostMonitorUnittest, OnSuccessfulStart)
UNIT_TEST_CASE(InputHostMonitorUnittest, OnSuccessfulStop)

} // namespace logtail

UNIT_TEST_MAIN
