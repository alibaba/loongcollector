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

#include <memory>
#include <string>

#include "json/json.h"

#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "collection_pipeline/plugin/PluginRegistry.h"
#include "common/JsonUtil.h"
#include "forward/GrpcInputManager.h"
#include "plugin/input/InputForward.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class InputForwardUnittest : public testing::Test {
public:
    void TestName();
    void TestSupportAck();
    void OnSuccessfulInit();
    void OnFailedInit();
    void OnSuccessfulStart();
    void OnFailedStart();
    void OnSuccessfulStop();
    void OnFailedStop();
    void TestInvalidProtocol();
    void TestMissingMandatoryParams();
    void TestEdgeCases();

protected:
    void SetUp() override {
        ctx.SetConfigName("test_config");
        ctx.SetPipeline(p);
        // Initialize GrpcInputManager for testing
        GrpcInputManager::GetInstance()->Stop();
        GrpcInputManager::GetInstance()->Init();
    }

    void TearDown() override { GrpcInputManager::GetInstance()->Stop(); }

    static void SetUpTestCase() { PluginRegistry::GetInstance()->LoadPlugins(); }

    static void TearDownTestCase() { PluginRegistry::GetInstance()->UnloadPlugins(); }

private:
    CollectionPipeline p;
    CollectionPipelineContext ctx;
};

void InputForwardUnittest::TestName() {
    InputForward input;
    APSARA_TEST_EQUAL(InputForward::sName, input.Name());
    APSARA_TEST_EQUAL("input_forward", input.Name());
}

void InputForwardUnittest::TestSupportAck() {
    InputForward input;
    APSARA_TEST_TRUE(input.SupportAck());
}

void InputForwardUnittest::OnSuccessfulInit() {
    unique_ptr<InputForward> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_forward",
            "Protocol": "LoongSuite",
            "Endpoint": "127.0.0.1:8080",
            "MatchRule": {
                "Key": "service",
                "Value": "test-service"
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputForward());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "2");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();

    APSARA_TEST_EQUAL("service", input->mMatchRule.key);
    APSARA_TEST_EQUAL("test-service", input->mMatchRule.value);
    APSARA_TEST_FALSE(input->mMatchRule.IsEmpty());
}

void InputForwardUnittest::OnFailedInit() {
    unique_ptr<InputForward> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // Test with empty config
    configStr = R"({})";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputForward());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "3");
    APSARA_TEST_FALSE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
}

void InputForwardUnittest::TestInvalidProtocol() {
    unique_ptr<InputForward> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // Test with unsupported protocol
    configStr = R"(
        {
            "Type": "input_forward",
            "Protocol": "UnsupportedProtocol",
            "Endpoint": "0.0.0.0:9999"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputForward());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "4");
    APSARA_TEST_FALSE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();

    // Test with empty protocol
    configStr = R"(
        {
            "Type": "input_forward",
            "Protocol": "",
            "Endpoint": "0.0.0.0:9999"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputForward());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "5");
    APSARA_TEST_FALSE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
}

void InputForwardUnittest::TestMissingMandatoryParams() {
    unique_ptr<InputForward> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // Test missing Protocol
    configStr = R"(
        {
            "Type": "input_forward",
            "Endpoint": "0.0.0.0:9999"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputForward());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "6");
    APSARA_TEST_FALSE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();

    // Test missing Endpoint
    configStr = R"(
        {
            "Type": "input_forward",
            "Protocol": "LoongSuite"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputForward());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "7");
    APSARA_TEST_FALSE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();

    // Test empty Endpoint
    configStr = R"(
        {
            "Type": "input_forward",
            "Protocol": "LoongSuite",
            "Endpoint": ""
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputForward());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "8");
    APSARA_TEST_FALSE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL("", input->mEndpoint);

    // Test missing MatchRule
    configStr = R"(
        {
            "Type": "input_forward",
            "Protocol": "LoongSuite",
            "Endpoint": "0.0.0.0:9999"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputForward());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "9");
    APSARA_TEST_FALSE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();

    // Test empty MatchRule Key
    configStr = R"(
        {
            "Type": "input_forward",
            "Protocol": "LoongSuite",
            "Endpoint": "0.0.0.0:9999",
            "MatchRule": {}
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputForward());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "10");
    APSARA_TEST_FALSE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
}

void InputForwardUnittest::TestEdgeCases() {
    unique_ptr<InputForward> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // Test MatchRule IsMatch functionality
    MatchRule testRule;
    testRule.key = "service";
    testRule.value = "test-service";

    APSARA_TEST_TRUE(testRule.IsMatch("service", "test-service"));
    APSARA_TEST_FALSE(testRule.IsMatch("service", "different-value"));
    APSARA_TEST_FALSE(testRule.IsMatch("different-key", "test-service"));
    APSARA_TEST_FALSE(testRule.IsMatch("", ""));

    // Test empty MatchRule
    MatchRule emptyRule;
    APSARA_TEST_TRUE(emptyRule.IsEmpty());
    APSARA_TEST_FALSE(testRule.IsEmpty());

    // Test with numeric endpoint formats
    configStr = R"(
        {
            "Type": "input_forward",
            "Protocol": "LoongSuite",
            "Endpoint": "192.168.1.100:8080"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputForward());
    input->SetContext(ctx);
    input->CreateMetricsRecordRef("test", "11");
    APSARA_TEST_FALSE(input->Init(configJson, optionalGoPipeline));
}

UNIT_TEST_CASE(InputForwardUnittest, TestName)
UNIT_TEST_CASE(InputForwardUnittest, TestSupportAck)
UNIT_TEST_CASE(InputForwardUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputForwardUnittest, OnFailedInit)
UNIT_TEST_CASE(InputForwardUnittest, TestInvalidProtocol)
UNIT_TEST_CASE(InputForwardUnittest, TestMissingMandatoryParams)
UNIT_TEST_CASE(InputForwardUnittest, TestEdgeCases)

} // namespace logtail

UNIT_TEST_MAIN
