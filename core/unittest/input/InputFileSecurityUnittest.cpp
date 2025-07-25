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

#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/JsonUtil.h"
#include "common/http/AsynCurlRunner.h"
#include "common/timer/Timer.h"
#include "ebpf/Config.h"
#include "ebpf/EBPFServer.h"
#include "plugin/input/InputFileSecurity.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class InputFileSecurityUnittest : public testing::Test {
public:
    void TestName();
    void TestSupportAck();
    void OnSuccessfulInit();
    void OnFailedInit();
    void OnStart();
    void OnSuccessfulStop();
    // void OnPipelineUpdate();

protected:
    void SetUp() override {
        mPipeline.mName = "test_config";
        mContex.SetConfigName("test_config");
        mContex.SetPipeline(mPipeline);
        ebpf::EBPFServer::GetInstance()->Init();
    }

    void TearDown() override {
        ebpf::EBPFServer::GetInstance()->Stop();
        Timer::GetInstance()->Stop();
        AsynCurlRunner::GetInstance()->Stop();
    }

private:
    CollectionPipeline mPipeline;
    CollectionPipelineContext mContex;
};

void InputFileSecurityUnittest::TestName() {
    InputFileSecurity input;
    std::string name = input.Name();
    APSARA_TEST_EQUAL(name, "input_file_security");
}

void InputFileSecurityUnittest::TestSupportAck() {
    InputFileSecurity input;
    bool supportAck = input.SupportAck();
    APSARA_TEST_TRUE(supportAck);
}

void InputFileSecurityUnittest::OnSuccessfulInit() {
    unique_ptr<InputFileSecurity> input;
    Json::Value configJson;
    Json::Value optionalGoPipeline;
    string configStr;
    string errorMsg;

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
    input->SetContext(mContex);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(input->sName, "input_file_security");
    logtail::ebpf::SecurityFileFilter thisFilter1
        = std::get<logtail::ebpf::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].mFilter);
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
    input->SetContext(mContex);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(input->sName, "input_file_security");
    logtail::ebpf::SecurityFileFilter thisFilter2
        = std::get<logtail::ebpf::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].mFilter);
    APSARA_TEST_EQUAL("/etc/passwd", thisFilter2.mFilePathList[0]);
    APSARA_TEST_EQUAL("/etc/shadow", thisFilter2.mFilePathList[1]);
    APSARA_TEST_EQUAL("/bin", thisFilter2.mFilePathList[2]);

    // test deduplication
    configStr = R"(
        {
            "Type": "input_file_security",
            "ProbeConfig": 
            {
                "FilePathFilter": [
                    "/etc/passwd",
                    "/etc/shadow",
                    "/etc/passwd",
                    "/bin",
                    "/bin"
                ]
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputFileSecurity());
    input->SetContext(mContex);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(input->sName, "input_file_security");
    logtail::ebpf::SecurityFileFilter thisFilter3
        = std::get<logtail::ebpf::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].mFilter);
    APSARA_TEST_EQUAL(static_cast<size_t>(3), thisFilter3.mFilePathList.size());
    APSARA_TEST_EQUAL("/etc/passwd", thisFilter3.mFilePathList[0]);
    APSARA_TEST_EQUAL("/etc/shadow", thisFilter3.mFilePathList[1]);
    APSARA_TEST_EQUAL("/bin", thisFilter3.mFilePathList[2]);

    // test excessive filters
    stringstream ss;
    ss << R"(
        {
            "Type": "input_file_security",
            "ProbeConfig": 
            {
                "FilePathFilter": [)";

    for (int i = 0; i < 70; i++) {
        if (i > 0) {
            ss << ",";
        }
        ss << "\"/test/path" << i << "\"";
    }

    ss << R"(
                ]
            }
        }
    )";

    configStr = ss.str();
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    input.reset(new InputFileSecurity());
    input->SetContext(mContex);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(input->sName, "input_file_security");
    logtail::ebpf::SecurityFileFilter thisFilter
        = std::get<logtail::ebpf::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].mFilter);
    // the portion exceeding 64 has been discarded.
    APSARA_TEST_EQUAL(64UL, thisFilter.mFilePathList.size());
}

void InputFileSecurityUnittest::OnFailedInit() {
    unique_ptr<InputFileSecurity> input;
    Json::Value configJson;
    Json::Value optionalGoPipeline;
    string configStr;
    string errorMsg;

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
    input->SetContext(mContex);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(input->sName, "input_file_security");
    logtail::ebpf::SecurityFileFilter thisFilter
        = std::get<logtail::ebpf::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].mFilter);
    APSARA_TEST_EQUAL(0UL, thisFilter.mFilePathList.size());

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
    input->SetContext(mContex);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(input->sName, "input_file_security");
    logtail::ebpf::SecurityFileFilter thisFilter1
        = std::get<logtail::ebpf::SecurityFileFilter>(input->mSecurityOptions.mOptionList[0].mFilter);
    APSARA_TEST_EQUAL(0UL, thisFilter1.mFilePathList.size());

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
    input->SetContext(mContex);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_EQUAL(input->sName, "input_file_security");
    APSARA_TEST_EQUAL(1UL, input->mSecurityOptions.mOptionList.size()); // default callname
    APSARA_TEST_EQUAL(3UL, input->mSecurityOptions.mOptionList[0].mCallNames.size()); // default callname
}

void InputFileSecurityUnittest::OnStart() {
    unique_ptr<InputFileSecurity> input;
    Json::Value configJson;
    Json::Value optionalGoPipeline;
    string configStr;
    string errorMsg;

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
    input->SetContext(mContex);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_TRUE(input->Start());
    APSARA_TEST_TRUE(input->Stop(true));

    // simulate an unsupported environment
    auto& envMgr = ebpf::EBPFServer::GetInstance()->mEnvMgr;
    envMgr.mArchSupport = false;
    envMgr.mBTFSupport = false;
    APSARA_TEST_FALSE(input->Start());
    envMgr.mArchSupport = true;
    envMgr.mBTFSupport = true;
}

void InputFileSecurityUnittest::OnSuccessfulStop() {
    unique_ptr<InputFileSecurity> input;
    Json::Value configJson;
    Json::Value optionalGoPipeline;
    string configStr;
    string errorMsg;

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
    input->SetContext(mContex);
    input->CreateMetricsRecordRef("test", "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    input->CommitMetricsRecordRef();
    APSARA_TEST_TRUE(input->Start());
}

UNIT_TEST_CASE(InputFileSecurityUnittest, TestName)
UNIT_TEST_CASE(InputFileSecurityUnittest, TestSupportAck)
UNIT_TEST_CASE(InputFileSecurityUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputFileSecurityUnittest, OnFailedInit)
UNIT_TEST_CASE(InputFileSecurityUnittest, OnStart)
UNIT_TEST_CASE(InputFileSecurityUnittest, OnSuccessfulStop)
// UNIT_TEST_CASE(InputFileSecurityUnittest, OnPipelineUpdate)

} // namespace logtail

UNIT_TEST_MAIN
