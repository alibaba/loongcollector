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
#include "plugin/input/InputFileSecurity.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineContext.h"
#include "unittest/Unittest.h"
#include "ebpf/eBPFServer.h"

using namespace std;

namespace logtail {

class InputFileSecurityUnittest : public testing::Test {
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

void InputFileSecurityUnittest::TestName() {
    InputFileSecurity input;
    std::string name = input.Name();
    APSARA_TEST_EQUAL(name, "input_file_security");
}

void InputFileSecurityUnittest::TestSupportAck() {
    InputFileSecurity input;
    bool supportAck = input.SupportAck();
    APSARA_TEST_FALSE(supportAck);
}

void InputFileSecurityUnittest::OnSuccessfulInit() {
    unique_ptr<InputFileSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // only mandatory param
    configStr = R"(
        {
            "Type": "input_file_security",
            "ProbeConfig": 
            {
                "FilePathFilter": [
                    "/etc",
                    "/bin"
                ]
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputFileSecurity());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_file_security");
    nami::SecurityFileFilter thisFilter1 = std::get<nami::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].filter_);
    APSARA_TEST_EQUAL("/etc", thisFilter1.mFilePathList[0]);
    APSARA_TEST_EQUAL("/bin", thisFilter1.mFilePathList[1]);

    // valid optional param
    configStr = R"(
        {
            "Type": "input_file_security",
            "ProbeConfig": 
            {
                "FilePathFilter": [
                    "/etc/passwd",
                    "/etc/shadow",
                    "/bin"
                ]
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputFileSecurity());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_file_security");
    nami::SecurityFileFilter thisFilter2 = std::get<nami::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].filter_);
    APSARA_TEST_EQUAL("/etc/passwd", thisFilter2.mFilePathList[0]);
    APSARA_TEST_EQUAL("/etc/shadow", thisFilter2.mFilePathList[1]);
    APSARA_TEST_EQUAL("/bin", thisFilter2.mFilePathList[2]);
}

void InputFileSecurityUnittest::OnFailedInit() {
    unique_ptr<InputFileSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    // invalid mandatory param
    configStr = R"(
        {
            "Type": "input_file_security",
            "ProbeConfig": 
            {
                "FilePathFilter": [1]
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputFileSecurity());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_file_security");
    nami::SecurityFileFilter thisFilter = std::get<nami::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].filter_);
    APSARA_TEST_EQUAL(0, thisFilter.mFilePathList.size());

    // invalid optional param
    configStr = R"(
        {
            "Type": "input_file_security",
            "ProbeConfig": 
            {
                "FilePathFilter": [
                    "/etc",
                    1
                ]
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputFileSecurity());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_file_security");
    nami::SecurityFileFilter thisFilter1 = std::get<nami::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].filter_);
    APSARA_TEST_EQUAL(0, thisFilter1.mFilePathList.size());

    // lose mandatory param
    configStr = R"(
        {
            "Type": "input_file_security",
            "ProbeConfig": 
            {
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputFileSecurity());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(input->sName, "input_file_security");
    APSARA_TEST_EQUAL(1, input->mSecurityOptions.mOptionList.size()); // default callname
    APSARA_TEST_EQUAL(3, input->mSecurityOptions.mOptionList[0].call_names_.size()); // default callname
}

void InputFileSecurityUnittest::OnSuccessfulStart() {
    unique_ptr<InputFileSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_file_security",
            "ProbeConfig": 
            {
                "FilePathFilter": [
                    "/etc",
                    "/bin"
                ]
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputFileSecurity());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(input->Start());
    string serverPipelineName = ebpf::eBPFServer::GetInstance()->CheckLoadedPipelineName(nami::PluginType::FILE_SECURITY);
    string pipelineName = input->GetContext().GetConfigName();
    APSARA_TEST_TRUE(serverPipelineName.size() && serverPipelineName == pipelineName);
}

void InputFileSecurityUnittest::OnSuccessfulStop() {
    unique_ptr<InputFileSecurity> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;

    configStr = R"(
        {
            "Type": "input_file_security",
            "ProbeConfig": 
            {
                "FilePathFilter": [
                    "/etc",
                    "/bin"
                ]
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputFileSecurity());
    input->SetContext(ctx);
    input->SetMetricsRecordRef("test", "1");
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

UNIT_TEST_CASE(InputFileSecurityUnittest, TestName)
UNIT_TEST_CASE(InputFileSecurityUnittest, TestSupportAck)   
UNIT_TEST_CASE(InputFileSecurityUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputFileSecurityUnittest, OnFailedInit)
UNIT_TEST_CASE(InputFileSecurityUnittest, OnSuccessfulStart)
UNIT_TEST_CASE(InputFileSecurityUnittest, OnSuccessfulStop)
// UNIT_TEST_CASE(InputFileSecurityUnittest, OnPipelineUpdate)

} // namespace logtail

UNIT_TEST_MAIN
