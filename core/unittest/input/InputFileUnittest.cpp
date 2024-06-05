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

#include <filesystem>
#include <memory>
#include <string>

#include "app_config/AppConfig.h"
#include "common/JsonUtil.h"
#include "file_server/FileServer.h"
#include "input/InputFile.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineContext.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_INT32(default_plugin_log_queue_size);

using namespace std;

namespace logtail {

class InputFileUnittest : public testing::Test {
public:
    void OnSuccessfulInit();
    void OnFailedInit();
    void OnEnableContainerDiscovery();
    void OnPipelineUpdate();
    void TestSetContainerBaseDir();

protected:
    static void SetUpTestCase() { AppConfig::GetInstance()->mPurageContainerMode = true; }
    void SetUp() override {
        p.mName = "test_config";
        ctx.SetConfigName("test_config");
        ctx.SetPipeline(p);
    }

private:
    Pipeline p;
    PipelineContext ctx;
};

void InputFileUnittest::OnSuccessfulInit() {
    unique_ptr<InputFile> input;
    Json::Value configJson, optionalGoPipeline;
    string configStr, errorMsg;
    filesystem::path filePath = filesystem::absolute("*.log");

    // only mandatory param
    configStr = R"(
        {
            "Type": "input_file",
            "FilePaths": []
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    input.reset(new InputFile());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputFile::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_FALSE(input->mEnableContainerDiscovery);
    APSARA_TEST_EQUAL(0U, input->mMaxCheckpointDirSearchDepth);
    APSARA_TEST_EQUAL(0U, input->mExactlyOnceConcurrency);

    // valid optional param
    configStr = R"(
        {
            "Type": "input_file",
            "FilePaths": [],
            "EnableContainerDiscovery": true,
            "MaxCheckpointDirSearchDepth": 1,
            "EnableExactlyOnce": 1
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    input.reset(new InputFile());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputFile::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(input->mEnableContainerDiscovery);
    APSARA_TEST_EQUAL(1, input->mMaxCheckpointDirSearchDepth);
    APSARA_TEST_EQUAL(1, input->mExactlyOnceConcurrency);

    // invalid optional param
    configStr = R"(
        {
            "Type": "input_file",
            "FilePaths": [],
            "EnableContainerDiscovery": "true",
            "MaxCheckpointDirSearchDepth": true,
            "EnableExactlyOnce": true
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    input.reset(new InputFile());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputFile::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_FALSE(input->mEnableContainerDiscovery);
    APSARA_TEST_EQUAL(0U, input->mMaxCheckpointDirSearchDepth);
    APSARA_TEST_EQUAL(0U, input->mExactlyOnceConcurrency);

    // TailingAllMatchedFiles
    configStr = R"(
        {
            "Type": "input_file",
            "FilePaths": [],
            "TailingAllMatchedFiles": true,
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    input.reset(new InputFile());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputFile::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_TRUE(input->mFileReader.mTailingAllMatchedFiles);
    APSARA_TEST_TRUE(input->mFileDiscovery.IsTailingAllMatchedFiles());

    // ExactlyOnceConcurrency
    configStr = R"(
        {
            "Type": "input_file",
            "FilePaths": [],
            "EnableExactlyConcurrency": 600,
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    input.reset(new InputFile());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputFile::sName, "1");
    APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
    APSARA_TEST_EQUAL(0U, input->mExactlyOnceConcurrency);
}

void InputFileUnittest::OnFailedInit() {
    unique_ptr<InputFile> input;
    Json::Value configJson, optionalGoPipeline;

    input.reset(new InputFile());
    input->SetContext(ctx);
    input->SetMetricsRecordRef(InputFile::sName, "1");
    APSARA_TEST_FALSE(input->Init(configJson, optionalGoPipeline));
}

void InputFileUnittest::OnEnableContainerDiscovery() {
    // 启用容器元信息预览
    {
        unique_ptr<InputFile> input;
        Json::Value configJson, optionalGoPipelineJson, optionalGoPipeline;
        string configStr, optionalGoPipelineStr, errorMsg;
        filesystem::path filePath = filesystem::absolute("*.log");

        configStr = R"(
            {
                "Type": "input_file",
                "FilePaths": [],
                "EnableContainerDiscovery": true,
                "ContainerFilters": {
                    "K8sNamespaceRegex": "default"
                },
                "CollectingContainersMeta": true
            }
        )";
        optionalGoPipelineStr = R"(
            {
                "global": {
                    "AlwaysOnline": true
                },
                "inputs": [
                    {                
                        "type": "metric_container_info",
                        "detail": {
                            "CollectingContainersMeta": true,
                            "FilePattern": "*.log",
                            "K8sNamespaceRegex": "default",
                            "MaxDepth": 0,
                            "LogPath": ""
                        }
                    }
                ]
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        APSARA_TEST_TRUE(ParseJsonTable(optionalGoPipelineStr, optionalGoPipelineJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        optionalGoPipelineJson["global"]["DefaultLogQueueSize"]
            = Json::Value(INT32_FLAG(default_plugin_log_queue_size));
        optionalGoPipelineJson["inputs"][0]["detail"]["LogPath"] = Json::Value(filePath.parent_path().string());
        input.reset(new InputFile());
        input->SetContext(ctx);
        input->SetMetricsRecordRef(InputFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        APSARA_TEST_TRUE(input->mEnableContainerDiscovery);
        APSARA_TEST_TRUE(input->mFileDiscovery.IsContainerDiscoveryEnabled());
        APSARA_TEST_EQUAL(optionalGoPipelineJson.toStyledString(), optionalGoPipeline.toStyledString());
    }
    // 不启用容器元信息预览
    {
        unique_ptr<InputFile> input;
        Json::Value configJson, optionalGoPipelineJson, optionalGoPipeline;
        string configStr, optionalGoPipelineStr, errorMsg;
        filesystem::path filePath = filesystem::absolute("*.log");

        configStr = R"(
            {
                "Type": "input_file",
                "FilePaths": [],
                "EnableContainerDiscovery": true,
                "ContainerFilters": {
                    "K8sNamespaceRegex": "default"
                }
            }
        )";
        optionalGoPipelineStr = R"(
            {
                "global": {
                    "AlwaysOnline": true
                },
                "inputs": [
                    {                
                        "type": "metric_container_info",
                        "detail": {
                            "FilePattern": "*.log",
                            "K8sNamespaceRegex": "default",
                            "MaxDepth": 0,
                            "LogPath": ""
                        }
                    }
                ]
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
        APSARA_TEST_TRUE(ParseJsonTable(optionalGoPipelineStr, optionalGoPipelineJson, errorMsg));
        configJson["FilePaths"].append(Json::Value(filePath.string()));
        optionalGoPipelineJson["global"]["DefaultLogQueueSize"]
            = Json::Value(INT32_FLAG(default_plugin_log_queue_size));
        optionalGoPipelineJson["inputs"][0]["detail"]["LogPath"] = Json::Value(filePath.parent_path().string());
        input.reset(new InputFile());
        input->SetContext(ctx);
        input->SetMetricsRecordRef(InputFile::sName, "1");
        APSARA_TEST_TRUE(input->Init(configJson, optionalGoPipeline));
        APSARA_TEST_TRUE(input->mEnableContainerDiscovery);
        APSARA_TEST_TRUE(input->mFileDiscovery.IsContainerDiscoveryEnabled());
        APSARA_TEST_EQUAL(optionalGoPipelineJson.toStyledString(), optionalGoPipeline.toStyledString());
    }
}

void InputFileUnittest::OnPipelineUpdate() {
    Json::Value configJson, optionalGoPipeline;
    InputFile input;
    string configStr, errorMsg;
    filesystem::path filePath = filesystem::absolute("*.log");

    configStr = R"(
        {
            "Type": "input_file",
            "FilePaths": [],
            "EnableExactlyOnce": 2
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    configJson["FilePaths"].append(Json::Value(filePath.string()));
    APSARA_TEST_TRUE(input.Init(configJson, optionalGoPipeline));
    input.SetContext(ctx);

    APSARA_TEST_TRUE(input.Start());
    APSARA_TEST_NOT_EQUAL(nullptr, FileServer::GetInstance()->GetFileDiscoveryConfig("test_config").first);
    APSARA_TEST_NOT_EQUAL(nullptr, FileServer::GetInstance()->GetFileReaderConfig("test_config").first);
    APSARA_TEST_NOT_EQUAL(nullptr, FileServer::GetInstance()->GetMultilineConfig("test_config").first);
    APSARA_TEST_EQUAL(2U, FileServer::GetInstance()->GetExactlyOnceConcurrency("test_config"));

    APSARA_TEST_TRUE(input.Stop(true));
    APSARA_TEST_EQUAL(nullptr, FileServer::GetInstance()->GetFileDiscoveryConfig("test_config").first);
    APSARA_TEST_EQUAL(nullptr, FileServer::GetInstance()->GetFileReaderConfig("test_config").first);
    APSARA_TEST_EQUAL(nullptr, FileServer::GetInstance()->GetMultilineConfig("test_config").first);
    APSARA_TEST_EQUAL(0U, FileServer::GetInstance()->GetExactlyOnceConcurrency("test_config"));
}

void InputFileUnittest::TestSetContainerBaseDir() {
    // Create an InputFile object
    InputFile inputFile;
    {
        ContainerInfo containerInfo;
        containerInfo.mID = "testContainer";
        containerInfo.mUpperDir = "/UpperDir";
        containerInfo.mMounts.push_back(Mount("/source1", "/data1"));
        containerInfo.mMounts.push_back(Mount("/source2", "/data1/data2"));
        containerInfo.mMounts.push_back(Mount("/source3", "/data1/data2/data3"));
        containerInfo.mMounts.push_back(Mount("/source4", "/data1/data2/data3/data4"));

        containerInfo.mRealBaseDir = "";
        ASSERT_TRUE(inputFile.SetContainerBaseDir(containerInfo, "/data2/log"));
        APSARA_TEST_EQUAL("/logtail_host/UpperDir/data2/log", containerInfo.mRealBaseDir);

        containerInfo.mRealBaseDir = "";
        ASSERT_TRUE(inputFile.SetContainerBaseDir(containerInfo, "/data1/log"));
        APSARA_TEST_EQUAL("/logtail_host/source1/log", containerInfo.mRealBaseDir);

        containerInfo.mRealBaseDir = "";
        ASSERT_TRUE(inputFile.SetContainerBaseDir(containerInfo, "/data1/data2/log"));
        APSARA_TEST_EQUAL("/logtail_host/source2/log", containerInfo.mRealBaseDir);

        containerInfo.mRealBaseDir = "";
        ASSERT_TRUE(inputFile.SetContainerBaseDir(containerInfo, "/data1/data2/data3/log"));
        APSARA_TEST_EQUAL("/logtail_host/source3/log", containerInfo.mRealBaseDir);

        containerInfo.mRealBaseDir = "";
        ASSERT_TRUE(inputFile.SetContainerBaseDir(containerInfo, "/data1/data2/data3/data4/log"));
        APSARA_TEST_EQUAL("/logtail_host/source4/log", containerInfo.mRealBaseDir);

        containerInfo.mRealBaseDir = "";
        ASSERT_TRUE(inputFile.SetContainerBaseDir(containerInfo, "/data1/data2/data3/data4/data5/log"));
        APSARA_TEST_EQUAL("/logtail_host/source4/data5/log", containerInfo.mRealBaseDir);
    }
}

UNIT_TEST_CASE(InputFileUnittest, OnSuccessfulInit)
UNIT_TEST_CASE(InputFileUnittest, OnFailedInit)
UNIT_TEST_CASE(InputFileUnittest, OnEnableContainerDiscovery)
UNIT_TEST_CASE(InputFileUnittest, OnPipelineUpdate)
UNIT_TEST_CASE(InputFileUnittest, TestSetContainerBaseDir)

} // namespace logtail

UNIT_TEST_MAIN
