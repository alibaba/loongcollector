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

#include "app_config/AppConfig.h"
#include "common/JsonUtil.h"
#include "ebpf/config.h"
#include "plugin/input/InputNetworkSecurity.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineContext.h"
#include "unittest/Unittest.h"
#include "ebpf/eBPFServer.h"

using namespace std;

namespace logtail {

class InputNetworkSecurityUnittest : public testing::Test {
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
        ebpf::eBPFServer::GetInstance()->Init();
    }

private:
    Pipeline p;
    PipelineContext ctx;
};

void InputNetworkSecurityUnittest::TestName() {
    InputNetworkSecurity input;
    std::string name = input.Name();
    APSARA_TEST_EQUAL(name, "input_network_security");
}

void InputNetworkSecurityUnittest::TestSupportAck() {
    InputNetworkSecurity input;
    bool supportAck = input.SupportAck();
    APSARA_TEST_FALSE(supportAck);
}

void InputNetworkSecurityUnittest::OnSuccessfulInit() {
    unique_ptr<InputNetworkSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // valid optional param
    configStr = R"(
        {
            "Type": "input_network_security",
            "ProbeConfig":
            {
                "AddrFilter": {
                    "DestAddrList": ["10.0.0.0/8","92.168.0.0/16"],
                    "DestPortList": [80],
                    "SourceAddrBlackList": ["127.0.0.1/8"],
                    "SourcePortBlackList": [9300]
                }
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputNetworkSecurity());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_network_security");
    nami::SecurityNetworkFilter thisFilter1
        = std::get<nami::SecurityNetworkFilter>(input->mSecurityOptions.mOptionList[0].filter_);
    APSARA_TEST_EQUAL("10.0.0.0/8", thisFilter1.mDestAddrList[0]);
    APSARA_TEST_EQUAL("92.168.0.0/16", thisFilter1.mDestAddrList[1]);
    APSARA_TEST_EQUAL(1, thisFilter1.mDestPortList.size());
    APSARA_TEST_EQUAL("127.0.0.1/8", thisFilter1.mSourceAddrBlackList[0]);
    APSARA_TEST_EQUAL(9300, thisFilter1.mSourcePortBlackList[0]);
}

void InputNetworkSecurityUnittest::OnFailedInit() {
    unique_ptr<InputNetworkSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // invalid optional param
    configStr = R"(
        {
            "Type": "input_network_security",
            "ProbeConfig": 
            {
                "AddrFilter": {
                    "DestAddrList": ["10.0.0.0/8","92.168.0.0/16"],
                    "DestPortList": ["80"],
                    "SourceAddrBlackList": ["127.0.0.1/8"],
                    "SourcePortBlackList": [9300]
                }
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputNetworkSecurity());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_network_security");
    nami::SecurityNetworkFilter thisFilter1
        = std::get<nami::SecurityNetworkFilter>(input->mSecurityOptions.mOptionList[0].filter_);
    APSARA_TEST_EQUAL("10.0.0.0/8", thisFilter1.mDestAddrList[0]);
    APSARA_TEST_EQUAL("92.168.0.0/16", thisFilter1.mDestAddrList[1]);
    APSARA_TEST_EQUAL(0, thisFilter1.mDestPortList.size());
    APSARA_TEST_EQUAL("127.0.0.1/8", thisFilter1.mSourceAddrBlackList[0]);
    APSARA_TEST_EQUAL(9300, thisFilter1.mSourcePortBlackList[0]);

    // error param level
    configStr = R"(
        {
            "Type": "input_network_security",
            "ProbeConfig": 
            {
                "DestAddrList": ["10.0.0.0/8","92.168.0.0/16"],
                "DestPortList": ["80"],
                "SourceAddrBlackList": ["127.0.0.1/8"],
                "SourcePortBlackList": [9300]
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputNetworkSecurity());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    nami::SecurityNetworkFilter thisFilter5
        = std::get<nami::SecurityNetworkFilter>(input->mSecurityOptions.mOptionList[0].filter_);
    APSARA_TEST_EQUAL(thisFilter5.mDestAddrList.size(), 0);
    APSARA_TEST_EQUAL(thisFilter5.mDestPortList.size(), 0);
    APSARA_TEST_EQUAL(thisFilter5.mSourceAddrBlackList.size(), 0);
    APSARA_TEST_EQUAL(thisFilter5.mSourcePortBlackList.size(), 0);

    // valid and invalid optional param
    // if the optional param in a list is invalid, the valid param after it will be read
    configStr = R"(
        {
            "Type": "input_network_security",
            "ProbeConfig": 
            {
                "AddrFilter": {
                    "DestAddrList": ["10.0.0.0/8","92.168.0.0/16"],
                    "DestPortList": [40, "80", 160],
                    "SourceAddrBlackList": ["127.0.0.1/8"],
                    "SourcePortBlackList": [9300]
                }
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputNetworkSecurity());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    nami::SecurityNetworkFilter thisFilter6
        = std::get<nami::SecurityNetworkFilter>(input->mSecurityOptions.mOptionList[0].filter_);
    APSARA_TEST_EQUAL(2, thisFilter6.mDestAddrList.size());
}

void InputNetworkSecurityUnittest::OnSuccessfulStart() {
    unique_ptr<InputNetworkSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_network_security",
            "ProbeConfig":
            {
                "AddrFilter": {
                    "DestAddrList": ["10.0.0.0/8","92.168.0.0/16"],
                    "DestPortList": [80],
                    "SourceAddrBlackList": ["127.0.0.1/8"],
                    "SourcePortBlackList": [9300]
                }
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputNetworkSecurity());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(input->Start());
    string serverPipelineName = ebpf::eBPFServer::GetInstance()->CheckLoadedPipelineName(nami::PluginType::NETWORK_SECURITY);
    string pipelineName = input->GetContext().GetConfigName();
    APSARA_TEST_TRUE(serverPipelineName.size() && serverPipelineName == pipelineName);
}

void InputNetworkSecurityUnittest::OnSuccessfulStop() {
    unique_ptr<InputNetworkSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_network_security",
            "ProbeConfig":
            {
                "AddrFilter": {
                    "DestAddrList": ["10.0.0.0/8","92.168.0.0/16"],
                    "DestPortList": [80],
                    "SourceAddrBlackList": ["127.0.0.1/8"],
                    "SourcePortBlackList": [9300]
                }
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputNetworkSecurity());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(input->Start());
    string serverPipelineName = ebpf::eBPFServer::GetInstance()->CheckLoadedPipelineName(nami::PluginType::NETWORK_SECURITY);
    string pipelineName = input->GetContext().GetConfigName();
    APSARA_TEST_TRUE(serverPipelineName.size() && serverPipelineName == pipelineName);
    APSARA_TEST_TRUE(input->Stop(false));
    serverPipelineName = ebpf::eBPFServer::GetInstance()->CheckLoadedPipelineName(nami::PluginType::NETWORK_SECURITY);
    APSARA_TEST_TRUE(serverPipelineName.size() && serverPipelineName == pipelineName);
    APSARA_TEST_TRUE(input->Stop(true));
    serverPipelineName = ebpf::eBPFServer::GetInstance()->CheckLoadedPipelineName(nami::PluginType::NETWORK_SECURITY);
    APSARA_TEST_TRUE(serverPipelineName.empty());
}

UNIT_TEST_CASE(InputNetworkSecurityUnittest, TestName)
UNIT_TEST_CASE(InputNetworkSecurityUnittest, TestSupportAck)
UNIT_TEST_CASE(InputNetworkSecurityUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputNetworkSecurityUnittest, OnFailedInit)
UNIT_TEST_CASE(InputNetworkSecurityUnittest, OnSuccessfulStart)
UNIT_TEST_CASE(InputNetworkSecurityUnittest, OnSuccessfulStop)
// UNIT_TEST_CASE(InputNetworkSecurityUnittest, OnPipelineUpdate)

} // namespace logtail

UNIT_TEST_MAIN
