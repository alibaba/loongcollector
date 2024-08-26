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

#include <variant>

#include "app_config/AppConfig.h"
#include "common/JsonUtil.h"
#include "ebpf/config.h"
#include "input/InputEBPFFileSecurity.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineContext.h"
#include "unittest/Unittest.h"
#include "ebpf/eBPFServer.h"

using namespace std;

namespace logtail {

class InputEBPFFileSecurityUnittest : public testing::Test {
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

void InputEBPFFileSecurityUnittest::OnSuccessfulInit() {
    unique_ptr<InputEBPFFileSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // only mandatory param
    configStr = R"(
        {
            "Type": "input_ebpf_fileprobe_security",
            "ProbeConfig": [
                {
                    "CallNameFilter": ["security_file_permission"],
                    "FilePathFilter": [
                        "/etc",
                        "/bin"
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFFileSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_ebpf_fileprobe_security");
    nami::SecurityFileFilter thisFilter1 = std::get<nami::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].filter_);
    // APSARA_TEST_EQUAL(ebpf::SecurityProbeType::FILE, input->mSecurityOptions.filter_Type);
    APSARA_TEST_EQUAL("security_file_permission", input->mSecurityOptions.mOptionList[0].call_names_[0]);
    APSARA_TEST_EQUAL("/etc", thisFilter1.mFilePathList[0]);
    APSARA_TEST_EQUAL("/bin", thisFilter1.mFilePathList[1]);

    // valid optional param
    configStr = R"(
        {
            "Type": "input_ebpf_fileprobe_security",
            "ProbeConfig": [
                {
                    "CallNameFilter": ["security_file_permission"],
                    "FilePathFilter": [
                        "/etc/passwd",
                        "/etc/shadow",
                        "/bin"
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFFileSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_ebpf_fileprobe_security");
    nami::SecurityFileFilter thisFilter2 = std::get<nami::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].filter_);
    // APSARA_TEST_EQUAL(ebpf::SecurityProbeType::FILE, input->mSecurityOptions.filter_Type);
    APSARA_TEST_EQUAL("security_file_permission", input->mSecurityOptions.mOptionList[0].call_names_[0]);
    APSARA_TEST_EQUAL("/etc/passwd", thisFilter2.mFilePathList[0]);
    APSARA_TEST_EQUAL("/etc/shadow", thisFilter2.mFilePathList[1]);
    APSARA_TEST_EQUAL("/bin", thisFilter2.mFilePathList[2]);
}

void InputEBPFFileSecurityUnittest::OnFailedInit() {
    unique_ptr<InputEBPFFileSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // invalid mandatory param
    configStr = R"(
        {
            "Type": "input_ebpf_fileprobe_security",
            "ProbeConfig": [
                {
                    "CallNameFilter": ["security_file_permission"],
                    "FilePathFilter": [1]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFFileSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_ebpf_fileprobe_security");
    nami::SecurityFileFilter thisFilter = std::get<nami::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].filter_);
    APSARA_TEST_EQUAL("security_file_permission", input->mSecurityOptions.mOptionList[0].call_names_[0]);
    APSARA_TEST_EQUAL(0, thisFilter.mFilePathList.size());

    // invalid optional param
    configStr = R"(
        {
            "Type": "input_ebpf_fileprobe_security",
            "ProbeConfig": [
                {
                    "CallNameFilter": ["security_file_permission"],
                    "FilePathFilter": [
                        "/etc",
                        1
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFFileSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_ebpf_fileprobe_security");
    nami::SecurityFileFilter thisFilter1 = std::get<nami::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].filter_);
    // APSARA_TEST_EQUAL(ebpf::SecurityProbeType::FILE, input->mSecurityOptions.filter_Type);
    APSARA_TEST_EQUAL("security_file_permission", input->mSecurityOptions.mOptionList[0].call_names_[0]);
    APSARA_TEST_EQUAL(0, thisFilter1.mFilePathList.size());

    // lose mandatory param
    configStr = R"(
        {
            "Type": "input_ebpf_fileprobe_security",
            "ProbeConfig": [
                {
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFFileSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_ebpf_fileprobe_security");
    APSARA_TEST_EQUAL(1, input->mSecurityOptions.mOptionList.size()); // default callname
    APSARA_TEST_EQUAL(3, input->mSecurityOptions.mOptionList[0].call_names_.size()); // default callname

    // invalid callname
    configStr = R"(
        {
            "Type": "input_ebpf_fileprobe_security",
            "ProbeConfig": [
                {
                    "CallNameFilter": ["security_file_permission_error"],
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFFileSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_ebpf_fileprobe_security");
    APSARA_TEST_EQUAL(1, input->mSecurityOptions.mOptionList.size()); // default callname
    APSARA_TEST_EQUAL(3, input->mSecurityOptions.mOptionList[0].call_names_.size()); // default callname
}

void InputEBPFFileSecurityUnittest::OnSuccessfulStart() {
    unique_ptr<InputEBPFFileSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_ebpf_fileprobe_security",
            "ProbeConfig": [
                {
                    "CallNameFilter": ["security_file_permission"],
                    "FilePathFilter": [
                        "/etc",
                        "/bin"
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFFileSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(input->Start());
    string serverPipelineName = ebpf::eBPFServer::GetInstance()->CheckLoadedPipelineName(nami::PluginType::FILE_SECURITY);
    string pipelineName = input->GetContext().GetConfigName();
    APSARA_TEST_TRUE(serverPipelineName.size() && serverPipelineName == pipelineName);
}

void InputEBPFFileSecurityUnittest::OnSuccessfulStop() {
    unique_ptr<InputEBPFFileSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_ebpf_fileprobe_security",
            "ProbeConfig": [
                {
                    "CallNameFilter": ["security_file_permission"],
                    "FilePathFilter": [
                        "/etc",
                        "/bin"
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFFileSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(input->Start());
    string serverPipelineName = ebpf::eBPFServer::GetInstance()->CheckLoadedPipelineName(nami::PluginType::FILE_SECURITY);
    string pipelineName = input->GetContext().GetConfigName();
    APSARA_TEST_TRUE(serverPipelineName.size() && serverPipelineName == pipelineName);
    APSARA_TEST_TRUE(input->Stop(false));
    serverPipelineName = ebpf::eBPFServer::GetInstance()->CheckLoadedPipelineName(nami::PluginType::FILE_SECURITY);
    APSARA_TEST_TRUE(serverPipelineName.size() && serverPipelineName == pipelineName);
    APSARA_TEST_TRUE(input->Stop(true));
    serverPipelineName = ebpf::eBPFServer::GetInstance()->CheckLoadedPipelineName(nami::PluginType::FILE_SECURITY);
    APSARA_TEST_TRUE(serverPipelineName.empty());
}

UNIT_TEST_CASE(InputEBPFFileSecurityUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputEBPFFileSecurityUnittest, OnFailedInit)
UNIT_TEST_CASE(InputEBPFFileSecurityUnittest, OnSuccessfulStart)
UNIT_TEST_CASE(InputEBPFFileSecurityUnittest, OnSuccessfulStop)
// UNIT_TEST_CASE(InputEBPFFileSecurityUnittest, OnPipelineUpdate)

} // namespace logtail

UNIT_TEST_MAIN
