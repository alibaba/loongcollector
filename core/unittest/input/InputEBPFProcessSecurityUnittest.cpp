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
#include "plugin/input/InputEBPFProcessSecurity.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineContext.h"
#include "unittest/Unittest.h"
#include "ebpf/eBPFServer.h"

using namespace std;

namespace logtail {

class InputEBPFProcessSecurityUnittest : public testing::Test {
public:
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

void InputEBPFProcessSecurityUnittest::OnSuccessfulInit() {
    unique_ptr<InputEBPFProcessSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // valid param
    configStr = R"(
        {
            "Type": "input_ebpf_processprobe_security",
            "ProbeConfig": [
                {
                    "CallNameFilter": [
                        "sys_enter_execve",
                        "disassociate_ctty",
                        "acct_process",
                        "wake_up_new_task"
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFProcessSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_ebpf_processprobe_security");
    // four callnames
    APSARA_TEST_EQUAL("sys_enter_execve", input->mSecurityOptions.mOptionList[0].call_names_[0]);
    APSARA_TEST_EQUAL("disassociate_ctty", input->mSecurityOptions.mOptionList[0].call_names_[1]);
    APSARA_TEST_EQUAL("acct_process", input->mSecurityOptions.mOptionList[0].call_names_[2]);
    APSARA_TEST_EQUAL("wake_up_new_task", input->mSecurityOptions.mOptionList[0].call_names_[3]);
    // no general filter, default is monostate
    APSARA_TEST_EQUAL(std::holds_alternative<std::monostate>(input->mSecurityOptions.mOptionList[0].filter_), true);
}

void InputEBPFProcessSecurityUnittest::OnFailedInit() {
    unique_ptr<InputEBPFProcessSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // no probeconfig
    configStr = R"(
        {
            "Type": "input_ebpf_processprobe_security"
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFProcessSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList.size(), 1); // default callname
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList[0].call_names_.size(), 5); // default callname

    // probeconfig typo error
    configStr = R"(
        {
            "Type": "input_ebpf_processprobe_security",
            "ProbeConfiggg": [
                {
                    "CallNameFilter": [
                        "sys_enter_execve",
                        "disassociate_ctty",
                        "acct_process",
                        "wake_up_new_task"
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFProcessSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList.size(), 1); // default callname
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList[0].call_names_.size(), 5); // default callname

    // probeconfig type error
    configStr = R"(
        {
            "Type": "input_ebpf_processprobe_security",
            "ProbeConfig": {
                "CallNameFilter": [
                    "sys_enter_execve",
                    "disassociate_ctty",
                    "acct_process",
                    "wake_up_new_task"
                ]
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFProcessSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList.size(), 1); // default callname
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList[0].call_names_.size(), 5); // default callname

    // no callname
    configStr = R"(
        {
            "Type": "input_ebpf_processprobe_security",
            "ProbeConfig": [
                {
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFProcessSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList.size(), 1); // default callname
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList[0].call_names_.size(), 5); // default callname

    // callname typo error
    configStr = R"(
        {
            "Type": "input_ebpf_processprobe_security",
            "ProbeConfig": [
                {
                    "CallNameeee": [
                        "sys_enter_execve",
                        "disassociate_ctty",
                        "acct_process",
                        "wake_up_new_task"
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFProcessSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList.size(), 1); // default callname
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList[0].call_names_.size(), 5); // default callname

    // callname type error
    configStr = R"(
        {
            "Type": "input_ebpf_processprobe_security",
            "ProbeConfig": [
                {
                    "CallName": "sys_enter_execve"
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFProcessSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList.size(), 1); // default callname
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList[0].call_names_.size(), 5); // default callname

    // callname element type error at the first element
    configStr = R"(
        {
            "Type": "input_ebpf_processprobe_security",
            "ProbeConfig": [
                {
                    "CallName": [
                        1,
                        "disassociate_ctty",
                        "acct_process",
                        "wake_up_new_task"
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFProcessSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList.size(), 1); // default callname
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList[0].call_names_.size(), 5); // default callname

    // callname element type error at the last element
    configStr = R"(
        {
            "Type": "input_ebpf_processprobe_security",
            "ProbeConfig": [
                {
                    "CallName": [
                        "disassociate_ctty",
                        "acct_process",
                        "wake_up_new_task",
                        1
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFProcessSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList.size(), 1); // default callname
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList[0].call_names_.size(), 5); // default callname

    // null callname
    configStr = R"(
        {
            "Type": "input_ebpf_processprobe_security",
            "ProbeConfig": [
                {
                    "CallNameFilter": [
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFProcessSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList.size(), 1); // default callname
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList[0].call_names_.size(), 5); // default callname

    // invalid callname
    configStr = R"(
        {
            "Type": "input_ebpf_processprobe_security",
            "ProbeConfig": [
                {
                    "CallNameFilter": [
                        "sys_enter_execve_error"
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFProcessSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList.size(), 1); // default callname
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList[0].call_names_.size(), 5); // default callname

    // invalid callname of two
    configStr = R"(
        {
            "Type": "input_ebpf_processprobe_security",
            "ProbeConfig": [
                {
                    "CallNameFilter": [
                        "sys_enter_execve_error",
                        "disassociate_ctty",
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFProcessSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList.size(), 1);
    APSARA_TEST_EQUAL(input->mSecurityOptions.mOptionList[0].call_names_[0], "disassociate_ctty");
}

void InputEBPFProcessSecurityUnittest::OnSuccessfulStart() {
    unique_ptr<InputEBPFProcessSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_ebpf_processprobe_security",
            "ProbeConfig": [
                {
                    "CallNameFilter": [
                        "sys_enter_execve",
                        "disassociate_ctty",
                        "acct_process",
                        "wake_up_new_task"
                    ]

                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFProcessSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(input->Start());
    string serverPipelineName = ebpf::eBPFServer::GetInstance()->CheckLoadedPipelineName(nami::PluginType::PROCESS_SECURITY);
    string pipelineName = input->GetContext().GetConfigName();
    APSARA_TEST_TRUE(serverPipelineName.size() && serverPipelineName == pipelineName);
}

void InputEBPFProcessSecurityUnittest::OnSuccessfulStop() {
    unique_ptr<InputEBPFProcessSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_ebpf_processprobe_security",
            "ProbeConfig": [
                {
                    "CallNameFilter": [
                        "sys_enter_execve",
                        "disassociate_ctty",
                        "acct_process",
                        "wake_up_new_task"
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFProcessSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(input->Start());
    string serverPipelineName = ebpf::eBPFServer::GetInstance()->CheckLoadedPipelineName(nami::PluginType::PROCESS_SECURITY);
    string pipelineName = input->GetContext().GetConfigName();
    APSARA_TEST_TRUE(serverPipelineName.size() && serverPipelineName == pipelineName);
    APSARA_TEST_TRUE(input->Stop(false));
    serverPipelineName = ebpf::eBPFServer::GetInstance()->CheckLoadedPipelineName(nami::PluginType::PROCESS_SECURITY);
    APSARA_TEST_TRUE(serverPipelineName.size() && serverPipelineName == pipelineName);
    APSARA_TEST_TRUE(input->Stop(true));
    serverPipelineName = ebpf::eBPFServer::GetInstance()->CheckLoadedPipelineName(nami::PluginType::PROCESS_SECURITY);
    APSARA_TEST_TRUE(serverPipelineName.empty());
}

UNIT_TEST_CASE(InputEBPFProcessSecurityUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputEBPFProcessSecurityUnittest, OnFailedInit)
UNIT_TEST_CASE(InputEBPFProcessSecurityUnittest, OnSuccessfulStart)
UNIT_TEST_CASE(InputEBPFProcessSecurityUnittest, OnSuccessfulStop)
// UNIT_TEST_CASE(InputEBPFProcessSecurityUnittest, OnPipelineUpdate)

} // namespace logtail

UNIT_TEST_MAIN
