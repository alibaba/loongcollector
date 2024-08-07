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
    uint32_t pluginIdx = 0;

    // only mandatory param
    configStr = R"(
        {
            "Type": "input_ebpf_fileprobe_security",
            "ProbeConfig": [
                {
                    "CallName": ["security_file_permission"],
                    "FilePathFilter": [
                        {
                            "FilePath": "/etc",
                        },
                        {
                            "FilePath": "/bin"
                        }
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFFileSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, pluginIdx, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_ebpf_fileprobe_security");
    nami::SecurityFileFilter thisFilter1 = std::get<nami::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].filter_);
    // APSARA_TEST_EQUAL(ebpf::SecurityFilterType::FILE, input->mSecurityOptions.filter_Type);
    APSARA_TEST_EQUAL("security_file_permission", input->mSecurityOptions.mOptionList[0].call_names_[0]);
    APSARA_TEST_EQUAL("/etc", thisFilter1.mFileFilterItem[0].mFilePath);
    APSARA_TEST_EQUAL("/bin", thisFilter1.mFileFilterItem[1].mFilePath);

    // valid optional param
    configStr = R"(
        {
            "Type": "input_ebpf_fileprobe_security",
            "ProbeConfig": [
                {
                    "CallName": ["security_file_permission"],
                    "FilePathFilter": [
                        {
                            "FilePath": "/etc",
                            "FileName": "passwd"
                        },
                        {
                            "FilePath": "/etc",
                            "FileName": "shadow"
                        },
                        {
                            "FilePath": "/bin"
                        }
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFFileSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, pluginIdx, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_ebpf_fileprobe_security");
    nami::SecurityFileFilter thisFilter2 = std::get<nami::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].filter_);
    // APSARA_TEST_EQUAL(ebpf::SecurityFilterType::FILE, input->mSecurityOptions.filter_Type);
    APSARA_TEST_EQUAL("security_file_permission", input->mSecurityOptions.mOptionList[0].call_names_[0]);
    APSARA_TEST_EQUAL("/etc", thisFilter2.mFileFilterItem[0].mFilePath);
    APSARA_TEST_EQUAL("passwd", thisFilter2.mFileFilterItem[0].mFileName);
    APSARA_TEST_EQUAL("/etc", thisFilter2.mFileFilterItem[1].mFilePath);
    APSARA_TEST_EQUAL("shadow", thisFilter2.mFileFilterItem[1].mFileName);
    APSARA_TEST_EQUAL("/bin", thisFilter2.mFileFilterItem[2].mFilePath);
}

void InputEBPFFileSecurityUnittest::OnFailedInit() {
    unique_ptr<InputEBPFFileSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    uint32_t pluginIdx = 0;

    // invalid mandatory param
    configStr = R"(
        {
            "Type": "input_ebpf_fileprobe_security",
            "ProbeConfig": [
                {
                    "CallName": ["security_file_permission"],
                    "FilePathFilter": [
                        {
                            "FilePath": 1,
                            "FileName": "name"
                        }
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFFileSecurity());
    input->SetContext(ctx);
    APSARA_TEST_FALSE(input->Init(configJson, pluginIdx, optionalGoPipeline));

    // invalid optional param
    configStr = R"(
        {
            "Type": "input_ebpf_fileprobe_security",
            "ProbeConfig": [
                {
                    "CallName": ["security_file_permission"],
                    "FilePathFilter": [
                        {
                            "FilePath": "/etc",
                            "FileName": 1
                        }
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFFileSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, pluginIdx, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_ebpf_fileprobe_security");
    nami::SecurityFileFilter thisFilter1 = std::get<nami::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].filter_);
    // APSARA_TEST_EQUAL(ebpf::SecurityFilterType::FILE, input->mSecurityOptions.filter_Type);
    APSARA_TEST_EQUAL("security_file_permission", input->mSecurityOptions.mOptionList[0].call_names_[0]);
    APSARA_TEST_EQUAL("/etc", thisFilter1.mFileFilterItem[0].mFilePath);
    APSARA_TEST_EQUAL("", thisFilter1.mFileFilterItem[0].mFileName);

    // lose mandatory param
    configStr = R"(
        {
            "Type": "input_ebpf_fileprobe_security",
            "ProbeConfig": [
                {
                    "CallName": ["security_file_permission"],
                    "FilePathFilter": [
                        {
                            "FileName": "passwd"
                        }
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFFileSecurity());
    input->SetContext(ctx);
    APSARA_TEST_FALSE(input->Init(configJson, pluginIdx, optionalGoPipeline));

    // error param level
    configStr = R"(
        {
            "Type": "input_ebpf_fileprobe_security",
            "ProbeConfig": [
                {
                    "CallName": ["security_file_permission"],
                    "FileName": "passwd"
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFFileSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, pluginIdx, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_ebpf_fileprobe_security");
    // APSARA_TEST_EQUAL(ebpf::SecurityFilterType::FILE, input->mSecurityOptions.filter_Type);
    APSARA_TEST_EQUAL("security_file_permission", input->mSecurityOptions.mOptionList[0].call_names_[0]);
    APSARA_TEST_EQUAL(
        0, std::get<nami::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].filter_).mFileFilterItem.size());
}

void InputEBPFFileSecurityUnittest::OnSuccessfulStart() {
    unique_ptr<InputEBPFFileSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    uint32_t pluginIdx = 0;

    // only mandatory param
    configStr = R"(
        {
            "Type": "input_ebpf_fileprobe_security",
            "ProbeConfig": [
                {
                    "CallName": ["security_file_permission"],
                    "FilePathFilter": [
                        {
                            "FilePath": "/etc",
                        },
                        {
                            "FilePath": "/bin"
                        }
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFFileSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, pluginIdx, optionalGoPipeline));
    APSARA_TEST_TRUE(input->Start());
    string serverPipelineName = ebpf::eBPFServer::GetInstance()->CheckLoadedPipelineName(nami::PluginType::FILE_SECURITY);
    string pipelineName = input->GetContext().GetConfigName();
    APSARA_TEST_TRUE(serverPipelineName.size() && serverPipelineName == pipelineName);
}

void InputEBPFFileSecurityUnittest::OnSuccessfulStop() {
    unique_ptr<InputEBPFFileSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    uint32_t pluginIdx = 0;

    // only mandatory param
    configStr = R"(
        {
            "Type": "input_ebpf_fileprobe_security",
            "ProbeConfig": [
                {
                    "CallName": ["security_file_permission"],
                    "FilePathFilter": [
                        {
                            "FilePath": "/etc",
                        },
                        {
                            "FilePath": "/bin"
                        }
                    ]
                }
            ]
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputEBPFFileSecurity());
    input->SetContext(ctx);
    APSARA_TEST_TRUE(input->Init(configJson, pluginIdx, optionalGoPipeline));
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
